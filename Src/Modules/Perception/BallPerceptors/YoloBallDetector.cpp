/**
 * @file YoloBallDetector.cpp
 */

#include "YoloBallDetector.h"
#include "Platform/File.h"
#include "Tools/Math/Transformation.h"
#include "Streaming/Output.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

MAKE_MODULE(YoloBallDetector);

static constexpr const char* ORT_IN_NAME    = "images";
static constexpr const char* ORT_OUT_NAME   = "output0";

// ── Constructor / destructor ──────────────────────────────────────────────────

YoloBallDetector::YoloBallDetector()
    : ortEnv(ORT_LOGGING_LEVEL_WARNING, "YoloBallDetector")
{
  kalman.configure(kalmanInitConf, kalmanActiveConf, kalmanInstantInitConf, kalmanInitConfirmFrames,
                   kalmanInitGatePx, kalmanFarGatePx, kalmanNearGateScale, kalmanMaxMissed,
                   kalmanStrongPredictionMissed, kalmanMaxSpeedPx, kalmanMissedVelocityDecay,
                   kalmanProcessNoise, kalmanMeasurementNoise);
  loadModel();
  if(modelLoaded)
  {
    bgRunning = true;
    bgThread  = std::thread(&YoloBallDetector::inferenceLoop, this);
    OUTPUT_TEXT("[YoloBallDetector] Ready (YUYV→RGB→ONNX pipeline).");
  }
}

YoloBallDetector::~YoloBallDetector()
{
  bgRunning = false;
  frameCV.notify_all();
  if(bgThread.joinable())
    bgThread.join();
}

// ── Model loading ─────────────────────────────────────────────────────────────

void YoloBallDetector::loadModel()
{
  const std::string path = std::string(File::getBHDir()) + "/Config/" + modelName;
  try
  {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    opts.SetInterOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    ortSession  = std::make_unique<Ort::Session>(ortEnv, path.c_str(), opts);
    const auto inputShape = ortSession->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if(inputShape.size() == 4 &&
       inputShape[2] > 0 &&
       inputShape[3] > 0)
    {
      modelH = static_cast<int>(inputShape[2]);
      modelW = static_cast<int>(inputShape[3]);
    }
    modelLoaded = true;
    OUTPUT_TEXT("[YoloBallDetector] Model loaded: " << path << " (" << modelW << "x" << modelH << ")");
  }
  catch(const Ort::Exception& e)
  {
    OUTPUT_ERROR("[YoloBallDetector] Cannot load model: " << e.what());
  }
}

// ── Camera thread: update ─────────────────────────────────────────────────────

void YoloBallDetector::update(BallPercept& bp)
{
  bp.status = BallPercept::notSeen;

  if(!enabled || !modelLoaded)
  {
    consecutiveSeen = 0;
    return;
  }

  // Copy YUYV bytes to frame buffer and wake the BG thread
  {
    std::lock_guard<std::mutex> lk(frameMtx);
    const unsigned W    = theCameraImage.width;   // YUYV pairs
    const unsigned H    = theCameraImage.height;
    const unsigned sz   = W * H * 4;              // 4 bytes per YUYVPixel
    frameBuf.resize(sz);
    std::memcpy(frameBuf.data(), theCameraImage[0], sz);
    frameW            = W * 2;   // actual pixel width
    frameH            = H;
    frameCameraInfo   = theCameraInfo;
    frameCameraMatrix = theCameraMatrix;
    ++frameSequence;
    frameReady        = true;
  }
  frameCV.notify_one();

  // Read latest detection
  Det det;
  std::chrono::steady_clock::time_point detectionTime;
  {
    std::lock_guard<std::mutex> lk(detMtx);
    det           = latestDet;
    detectionTime = detTime;
  }

  const auto now   = std::chrono::steady_clock::now();
  const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - detectionTime).count();
  if(!det.valid || ageMs > timeoutMs)
  {
    consecutiveSeen = 0;
    return;
  }

  // Image → field geometry
  const Vector2f imgPos(det.cx, det.cy);
  Vector2f fieldPos;

  bool geomOk = Transformation::imageToRobotHorizontalPlane(
      imgPos, theBallSpecification.radius,
      det.cameraMatrix, det.cameraInfo, fieldPos)
    && std::isfinite(fieldPos.x()) && std::isfinite(fieldPos.y());

  if(!geomOk && det.cameraInfo.camera == CameraInfo::upper)
    geomOk = Transformation::imageToRobotHorizontalPlane(
        imgPos, 0.f, det.cameraMatrix, det.cameraInfo, fieldPos)
      && std::isfinite(fieldPos.x()) && std::isfinite(fieldPos.y());

  if(!geomOk && det.cameraInfo.camera == CameraInfo::upper && det.radius > 0.f)
  {
    const float dist = det.cameraInfo.focalLength * theBallSpecification.radius / det.radius;
    const float ang  = std::atan2(imgPos.x() - det.cameraInfo.width * 0.5f,
                                  det.cameraInfo.focalLength);
    fieldPos = Vector2f(dist * std::cos(ang), dist * std::sin(ang));
    geomOk   = dist > 0.f && std::isfinite(fieldPos.x()) && std::isfinite(fieldPos.y());
  }

  if(!geomOk) { consecutiveSeen = 0; return; }

  if(det.sequence != lastProcessedSequence)
  {
    lastProcessedSequence = det.sequence;
    ++consecutiveSeen;
  }

  const int minConsec = (det.cameraInfo.camera == CameraInfo::upper)
                        ? upperMinConsecutive : lowerMinConsecutive;
  if(consecutiveSeen < minConsec) return;

  bp.status            = BallPercept::seen;
  bp.positionInImage   = imgPos;
  bp.radiusInImage     = det.radius;
  bp.positionOnField   = fieldPos;
  bp.radiusOnField     = theBallSpecification.radius;
  bp.covarianceOnField = Matrix2f::Identity() * 10000.f;
}

void YoloBallDetector::update(RawBallPatch& rp)
{
  rp.valid = false;
  rp.data.clear();
}

// ── Background inference thread ───────────────────────────────────────────────

void YoloBallDetector::inferenceLoop()
{
  std::vector<unsigned char> localBuf;
  unsigned                   localW      = 0;
  unsigned                   localH      = 0;
  CameraInfo                 localCameraInfo;
  CameraMatrix               localCameraMatrix;
  unsigned                   localSequence = 0;
  std::vector<float>         tensor;

  while(bgRunning)
  {
    {
      std::unique_lock<std::mutex> lk(frameMtx);
      frameCV.wait(lk, [this]{ return frameReady || !bgRunning; });
      if(!bgRunning) break;
      localBuf          = frameBuf;
      localW            = frameW;
      localH            = frameH;
      localCameraInfo   = frameCameraInfo;
      localCameraMatrix = frameCameraMatrix;
      localSequence     = frameSequence;
      frameReady        = false;
    }

    if(!buildTensor(localBuf.data(), static_cast<int>(localW), static_cast<int>(localH),
                    modelW, modelH, tensor))
      continue;

    // Run ONNX inference
    try
    {
      Ort::MemoryInfo memInfo =
          Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

      const std::array<int64_t, 4> inShape = {1, 3, modelH, modelW};
      Ort::Value inTensor = Ort::Value::CreateTensor<float>(
          memInfo, tensor.data(), tensor.size(), inShape.data(), inShape.size());

      const char* inNames[]  = {ORT_IN_NAME};
      const char* outNames[] = {ORT_OUT_NAME};

      auto outs = ortSession->Run(
          Ort::RunOptions{nullptr}, inNames, &inTensor, 1, outNames, 1);

      const auto   shape = outs[0].GetTensorTypeAndShapeInfo().GetShape();
      const float* data  = outs[0].GetTensorMutableData<float>();
      if(shape.size() < 3) continue;

      const int64_t N          = shape[2];
      const float   confThresh = (localCameraInfo.camera == CameraInfo::upper) ? upperConf : lowerConf;
      const float   minTrackedConf = std::min(confThresh, std::min(kalmanInitConf, kalmanActiveConf));

      // Letterbox scale using actual camera dims (same as buildTensor)
      const float lbScale = std::min(static_cast<float>(modelW) / localW,
                                     static_cast<float>(modelH) / localH);
      const float lbPadX  = (modelW - localW * lbScale) * 0.5f;
      const float lbPadY  = (modelH - localH * lbScale) * 0.5f;

      std::vector<DetectionCandidate> detections;
      detections.reserve(static_cast<std::size_t>(N));
      float bestConf = confThresh;
      int   bestIdx  = -1;
      for(int64_t i = 0; i < N; ++i)
      {
        const float c = data[4 * N + i];
        if(c >= minTrackedConf)
        {
          const float cx_m = data[0 * N + i];
          const float cy_m = data[1 * N + i];
          const float  w_m = data[2 * N + i];
          const float  h_m = data[3 * N + i];
          DetectionCandidate detection;
          detection.x = (cx_m - lbPadX) / lbScale;
          detection.y = (cy_m - lbPadY) / lbScale;
          detection.w = w_m / lbScale;
          detection.h = h_m / lbScale;
          detection.confidence = c;
          detections.push_back(detection);
        }
        if(c > bestConf) { bestConf = c; bestIdx = static_cast<int>(i); }
      }

      Det result;
      result.sequence     = localSequence;
      result.cameraInfo   = localCameraInfo;
      result.cameraMatrix = localCameraMatrix;
      if(enableKalman)
      {
        const TrackState state = kalman.update(detections);
        if(state.active && (state.visible || (publishPredictedPercepts && state.predictedOnly)))
        {
          result.cx = state.x;
          result.cy = state.y;
          result.radius = (state.w + state.h) * 0.25f;
          result.conf = state.confidence;
          result.valid = true;
        }
      }
      else if(bestIdx >= 0)
      {
        const float cx_m = data[0 * N + bestIdx];
        const float cy_m = data[1 * N + bestIdx];
        const float  w_m = data[2 * N + bestIdx];
        const float  h_m = data[3 * N + bestIdx];
        result.cx     = (cx_m - lbPadX) / lbScale;
        result.cy     = (cy_m - lbPadY) / lbScale;
        result.radius = (w_m + h_m) * 0.25f / lbScale;
        result.conf   = bestConf;
        result.valid  = true;
      }

      {
        std::lock_guard<std::mutex> lk(detMtx);
        latestDet = result;
        detTime   = std::chrono::steady_clock::now();
      }
    }
    catch(const Ort::Exception& e)
    {
      OUTPUT_ERROR("[YoloBallDetector] " << e.what());
    }

    // Throttle: sleep AFTER inference so detection timestamp is fresh when read.
    std::this_thread::sleep_for(std::chrono::milliseconds(inferenceIntervalMs));
  }
}

// ── YUYV → letterboxed float RGB CHW tensor ───────────────────────────────────

bool YoloBallDetector::buildTensor(const unsigned char* yuyvData,
                                   int camW, int camH,
                                   int modelW, int modelH,
                                   std::vector<float>& out)
{
  if(!yuyvData || camW <= 0 || camH <= 0) return false;

  // Letterbox parameters
  const float scale = std::min(static_cast<float>(modelW) / camW,
                               static_cast<float>(modelH) / camH);
  const float padX  = (modelW - camW * scale) * 0.5f;
  const float padY  = (modelH - camH * scale) * 0.5f;

  // Gray fill (0.5) for padding regions
  out.assign(3 * modelH * modelW, 0.5f);

  // YUYV memory layout: each row has camW/2 YUYVPixels (4 bytes each)
  // YUYVPixel = { y0, u, y1, v }
  const int pairsPerRow = camW / 2;

  for(int oy = 0; oy < modelH; ++oy)
  {
    const int srcY = static_cast<int>((oy - padY) / scale + 0.5f);
    if(srcY < 0 || srcY >= camH) continue;

    for(int ox = 0; ox < modelW; ++ox)
    {
      const int srcX = static_cast<int>((ox - padX) / scale + 0.5f);
      if(srcX < 0 || srcX >= camW) continue;

      const int pairIdx = srcX / 2;
      const unsigned char* px = yuyvData + srcY * pairsPerRow * 4 + pairIdx * 4;

      // YUYV: px[0]=Y0, px[1]=U(Cb), px[2]=Y1, px[3]=V(Cr)
      const float Y  = static_cast<float>(srcX & 1 ? px[2] : px[0]);
      const float cb = static_cast<float>(px[1]) - 128.f;
      const float cr = static_cast<float>(px[3]) - 128.f;

      // Full-swing BT.601 — matches JPEG YCbCr decoding (cv2.imdecode on stream frames)
      auto clamp01 = [](float v) -> float { return v < 0.f ? 0.f : v > 255.f ? 1.f : v / 255.f; };
      out[(0 * modelH + oy) * modelW + ox] = clamp01(Y + 1.402f  * cr);           // R
      out[(1 * modelH + oy) * modelW + ox] = clamp01(Y - 0.344f  * cb - 0.714f * cr); // G
      out[(2 * modelH + oy) * modelW + ox] = clamp01(Y + 1.772f  * cb);           // B
    }
  }
  return true;
}

void YoloBallDetector::LightweightBallKalman::configure(float initConf_,
                                                        float activeConf_,
                                                        float instantInitConf_,
                                                        int initConfirmFrames_,
                                                        float initGatePx_,
                                                        float farGatePx_,
                                                        float nearGateScale_,
                                                        int maxMissed_,
                                                        int strongPredictionMissed_,
                                                        float maxSpeedPx_,
                                                        float missedVelocityDecay_,
                                                        float processNoise_,
                                                        float measurementNoise_)
{
  initConf = initConf_;
  activeConf = activeConf_;
  instantInitConf = instantInitConf_;
  initConfirmFrames = initConfirmFrames_;
  initGatePx = initGatePx_;
  farGatePx = farGatePx_;
  nearGateScale = nearGateScale_;
  maxMissed = maxMissed_;
  strongPredictionMissed = strongPredictionMissed_;
  maxSpeedPx = maxSpeedPx_;
  missedVelocityDecay = missedVelocityDecay_;
  processNoise = processNoise_;
  measurementNoise = measurementNoise_;
  reset();
}

void YoloBallDetector::LightweightBallKalman::reset()
{
  x.setZero();
  p = Matrix6f::Identity() * 500.f;
  age = 0;
  missed = 0;
  active = false;
  lastConfidence = 0.f;
  pendingDetection = DetectionCandidate();
  hasPendingDetection = false;
  pendingHits = 0;
  lastSelectedWasRelock = false;
}

void YoloBallDetector::LightweightBallKalman::predict(float dt)
{
  if(!active)
    return;

  clampVelocity();
  Matrix6f f = Matrix6f::Identity();
  f(0, 2) = dt;
  f(1, 3) = dt;

  Matrix6f q = Matrix6f::Identity() * processNoise;
  q(2, 2) *= 2.f;
  q(3, 3) *= 2.f;

  x = f * x;
  p = f * p * f.transpose() + q;
  ++age;
}

YoloBallDetector::TrackState YoloBallDetector::LightweightBallKalman::update(const std::vector<DetectionCandidate>& detections, float dt)
{
  predict(dt);
  const DetectionCandidate* detection = selectDetection(detections);

  if(detection == nullptr)
  {
    if(active)
    {
      ++missed;
      x[2] *= missedVelocityDecay;
      x[3] *= missedVelocityDecay;
      if(missed > maxMissed)
      {
        reset();
        return state(false, false, false);
      }
      return state(true, false, true);
    }
    return state(false, false, false);
  }

  if(!active)
  {
    if(!confirmInitialDetection(*detection))
      return state(false, false, false);
    initialize(*detection);
    return state(true, true, false);
  }

  if(lastSelectedWasRelock)
    relock(*detection);
  else
    correct(*detection);

  missed = 0;
  lastConfidence = detection->confidence;
  return state(true, true, false);
}

bool YoloBallDetector::LightweightBallKalman::confirmInitialDetection(const DetectionCandidate& detection)
{
  if(detection.confidence >= instantInitConf)
  {
    hasPendingDetection = false;
    pendingHits = 0;
    return true;
  }
  if(detection.confidence < initConf)
  {
    hasPendingDetection = false;
    pendingHits = 0;
    return false;
  }
  if(!hasPendingDetection)
  {
    pendingDetection = detection;
    hasPendingDetection = true;
    pendingHits = 1;
    return initConfirmFrames <= 1;
  }

  const float dx = detection.x - pendingDetection.x;
  const float dy = detection.y - pendingDetection.y;
  const float distance = std::hypot(dx, dy);
  pendingDetection = detection;
  if(distance <= initGatePx)
    ++pendingHits;
  else
    pendingHits = 1;
  return pendingHits >= initConfirmFrames;
}

const YoloBallDetector::DetectionCandidate* YoloBallDetector::LightweightBallKalman::selectDetection(const std::vector<DetectionCandidate>& detections)
{
  lastSelectedWasRelock = false;
  if(detections.empty())
    return nullptr;

  if(!active)
  {
    const DetectionCandidate* best = nullptr;
    float bestConfidence = -1.f;
    for(const DetectionCandidate& detection : detections)
    {
      if(detection.confidence < initConf)
        continue;
      if(detection.confidence > bestConfidence)
      {
        bestConfidence = detection.confidence;
        best = &detection;
      }
    }
    return best;
  }

  const float predictedX = x[0];
  const float predictedY = x[1];
  const float predictedSize = std::max({x[4], x[5], 1.f});
  float gate = std::max(farGatePx, predictedSize * nearGateScale);
  if(missed > strongPredictionMissed)
    gate *= 1.25f;

  const DetectionCandidate* best = nullptr;
  float bestScore = -1e9f;
  for(const DetectionCandidate& detection : detections)
  {
    const float dx = detection.x - predictedX;
    const float dy = detection.y - predictedY;
    const float distance = std::hypot(dx, dy);
    const float relockConf = missed <= strongPredictionMissed ? 0.45f : 0.55f;
    const bool isRelock = distance > gate;
    if(isRelock && detection.confidence < relockConf)
      continue;
    if(detection.confidence < activeConf)
      continue;

    float score = detection.confidence - 0.006f * std::min(distance, gate);
    if(isRelock)
      score -= 0.08f;
    if(score > bestScore)
    {
      bestScore = score;
      best = &detection;
      lastSelectedWasRelock = isRelock;
    }
  }

  return best;
}

void YoloBallDetector::LightweightBallKalman::initialize(const DetectionCandidate& detection)
{
  x << detection.x, detection.y, 0.f, 0.f, detection.w, detection.h;
  p = Matrix6f::Identity() * 120.f;
  age = 1;
  missed = 0;
  active = true;
  lastConfidence = detection.confidence;
  hasPendingDetection = false;
  pendingHits = 0;
}

void YoloBallDetector::LightweightBallKalman::relock(const DetectionCandidate& detection)
{
  const int previousAge = age;
  initialize(detection);
  age = std::max(previousAge, age);
  p = Matrix6f::Identity() * 90.f;
}

void YoloBallDetector::LightweightBallKalman::correct(const DetectionCandidate& detection)
{
  Eigen::Matrix<float, 4, 1> z;
  z << detection.x, detection.y, detection.w, detection.h;

  Eigen::Matrix<float, 4, 6> h = Eigen::Matrix<float, 4, 6>::Zero();
  h(0, 0) = 1.f;
  h(1, 1) = 1.f;
  h(2, 4) = 1.f;
  h(3, 5) = 1.f;

  Eigen::Matrix4f r = Eigen::Matrix4f::Identity() * measurementNoise;
  const Eigen::Matrix4f s = h * p * h.transpose() + r;
  const Eigen::Matrix<float, 6, 4> k = p * h.transpose() * s.inverse();
  const Eigen::Matrix<float, 4, 1> innovation = z - h * x;

  x = x + k * innovation;
  p = (Matrix6f::Identity() - k * h) * p;
  clampVelocity();
  lastConfidence = detection.confidence;
}

void YoloBallDetector::LightweightBallKalman::clampVelocity()
{
  const float vx = x[2];
  const float vy = x[3];
  const float speed = std::hypot(vx, vy);
  if(speed <= maxSpeedPx)
    return;

  const float scale = maxSpeedPx / std::max(speed, 1e-6f);
  x[2] *= scale;
  x[3] *= scale;
}

YoloBallDetector::TrackState YoloBallDetector::LightweightBallKalman::state(bool active_, bool visible_, bool predictedOnly_) const
{
  TrackState trackedState;
  trackedState.active = active_;
  trackedState.visible = visible_;
  trackedState.predictedOnly = predictedOnly_;
  trackedState.x = x[0];
  trackedState.y = x[1];
  trackedState.w = std::max(0.f, x[4]);
  trackedState.h = std::max(0.f, x[5]);
  trackedState.confidence = lastConfidence;
  trackedState.age = age;
  trackedState.missed = missed;
  return trackedState;
}
