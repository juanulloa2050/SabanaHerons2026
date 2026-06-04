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

static constexpr const char* MODEL_REL_PATH = "NeuralNets/BallDetector/yolo_ball.onnx";
static constexpr const char* ORT_IN_NAME    = "images";
static constexpr const char* ORT_OUT_NAME   = "output0";

// ── Constructor / destructor ──────────────────────────────────────────────────

YoloBallDetector::YoloBallDetector()
    : ortEnv(ORT_LOGGING_LEVEL_WARNING, "YoloBallDetector")
{
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
  const std::string path = std::string(File::getBHDir()) + "/Config/" + MODEL_REL_PATH;
  try
  {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    opts.SetInterOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    ortSession  = std::make_unique<Ort::Session>(ortEnv, path.c_str(), opts);
    modelLoaded = true;
    OUTPUT_TEXT("[YoloBallDetector] Model loaded: " << path);
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
  std::vector<float>         tensor(1 * 3 * MODEL_H * MODEL_W);

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
                    MODEL_W, MODEL_H, tensor))
      continue;

    // Run ONNX inference
    try
    {
      Ort::MemoryInfo memInfo =
          Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

      const std::array<int64_t, 4> inShape = {1, 3, MODEL_H, MODEL_W};
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

      // Letterbox scale using actual camera dims (same as buildTensor)
      const float lbScale = std::min(static_cast<float>(MODEL_W) / localW,
                                     static_cast<float>(MODEL_H) / localH);
      const float lbPadX  = (MODEL_W - localW * lbScale) * 0.5f;
      const float lbPadY  = (MODEL_H - localH * lbScale) * 0.5f;

      float bestConf = confThresh;
      int   bestIdx  = -1;
      for(int64_t i = 0; i < N; ++i)
      {
        const float c = data[4 * N + i];
        if(c > bestConf) { bestConf = c; bestIdx = static_cast<int>(i); }
      }

      Det result;
      result.sequence     = localSequence;
      result.cameraInfo   = localCameraInfo;
      result.cameraMatrix = localCameraMatrix;
      if(bestIdx >= 0)
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
