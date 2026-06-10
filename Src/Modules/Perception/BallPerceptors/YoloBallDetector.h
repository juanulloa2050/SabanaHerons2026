/**
 * @file YoloBallDetector.h
 *
 * Runs YOLOv8 ONNX inference on-robot to detect the trionda ball.
 *
 * Reads CameraImage (YUYV) directly — avoids the fake-CMYK JPEG that
 * B-Human uses internally. YUYV → RGB conversion matches the full-swing
 * BT.601 formula that libjpeg uses when decoding standard YCbCr JPEGs,
 * so pixel values match the training pipeline (stream JPEG → cv2.imdecode).
 *
 * Inference runs in a background thread (never blocks the RT camera thread).
 *
 * Model: Config/NeuralNets/BallDetector/yolo_ball.onnx
 *   Export: model.export(format="onnx", imgsz=160, simplify=True, opset=12)
 *   Input:  [1, 3, 160, 160]  float32 RGB CHW  normalized [0,1]
 *   Output: [1, 5, N]         float32  cx,cy,w,h,conf in model-pixel coords
 */

#pragma once

#include "Representations/Configuration/BallSpecification.h"
#include "Representations/Infrastructure/CameraImage.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Perception/BallPercepts/RawBallPatch.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Framework/Module.h"
#include "Math/Eigen.h"

#include <onnxruntime_cxx_api.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

MODULE(YoloBallDetector,
{,
  REQUIRES(BallSpecification),
  REQUIRES(CameraImage),
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  PROVIDES(BallPercept),
  PROVIDES(RawBallPatch),
  LOADS_PARAMETERS(
  {,
    (bool)(true)   enabled,
    (std::string)("NeuralNets/BallDetector/yolo_ball.onnx") modelName,
    (float)(0.20f) lowerConf,
    (float)(0.55f) upperConf,
    (int)(1)       lowerMinConsecutive,
    (int)(1)       upperMinConsecutive,
    (int)(2000)    timeoutMs,
    (int)(200)     inferenceIntervalMs,
    (bool)(false)  enableKalman,
    (bool)(false)  publishPredictedPercepts,
    (float)(0.35f) kalmanInitConf,
    (float)(0.18f) kalmanActiveConf,
    (float)(0.55f) kalmanInstantInitConf,
    (int)(2)       kalmanInitConfirmFrames,
    (float)(65.f)  kalmanInitGatePx,
    (float)(55.f)  kalmanFarGatePx,
    (float)(2.5f)  kalmanNearGateScale,
    (int)(10)      kalmanMaxMissed,
    (int)(3)       kalmanStrongPredictionMissed,
    (float)(85.f)  kalmanMaxSpeedPx,
    (float)(0.82f) kalmanMissedVelocityDecay,
    (float)(8.f)   kalmanProcessNoise,
    (float)(18.f)  kalmanMeasurementNoise,
  }),
});

class YoloBallDetector : public YoloBallDetectorBase
{
public:
  YoloBallDetector();
  ~YoloBallDetector();

private:
  void update(BallPercept& bp) override;
  void update(RawBallPatch& rp) override;

  void loadModel();
  void inferenceLoop();

  struct DetectionCandidate
  {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float confidence = 0.f;
  };

  struct TrackState
  {
    bool active = false;
    bool visible = false;
    bool predictedOnly = false;
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float confidence = 0.f;
    int age = 0;
    int missed = 0;
  };

  class LightweightBallKalman
  {
  public:
    void configure(float initConf,
                   float activeConf,
                   float instantInitConf,
                   int initConfirmFrames,
                   float initGatePx,
                   float farGatePx,
                   float nearGateScale,
                   int maxMissed,
                   int strongPredictionMissed,
                   float maxSpeedPx,
                   float missedVelocityDecay,
                   float processNoise,
                   float measurementNoise);
    void reset();
    TrackState update(const std::vector<DetectionCandidate>& detections, float dt = 1.f);

  private:
    bool confirmInitialDetection(const DetectionCandidate& detection);
    const DetectionCandidate* selectDetection(const std::vector<DetectionCandidate>& detections);
    void initialize(const DetectionCandidate& detection);
    void relock(const DetectionCandidate& detection);
    void correct(const DetectionCandidate& detection);
    void predict(float dt);
    void clampVelocity();
    TrackState state(bool active, bool visible, bool predictedOnly) const;

    Vector6f x = Vector6f::Zero();
    Matrix6f p = Matrix6f::Identity() * 500.f;
    int age = 0;
    int missed = 0;
    bool active = false;
    float lastConfidence = 0.f;
    DetectionCandidate pendingDetection;
    bool hasPendingDetection = false;
    int pendingHits = 0;
    bool lastSelectedWasRelock = false;

    float initConf = 0.35f;
    float activeConf = 0.18f;
    float instantInitConf = 0.55f;
    int initConfirmFrames = 2;
    float initGatePx = 65.f;
    float farGatePx = 55.f;
    float nearGateScale = 2.5f;
    int maxMissed = 10;
    int strongPredictionMissed = 3;
    float maxSpeedPx = 85.f;
    float missedVelocityDecay = 0.82f;
    float processNoise = 8.f;
    float measurementNoise = 18.f;
  };

  // Convert YUYV CameraImage → letterboxed float RGB CHW tensor
  static bool buildTensor(const unsigned char* yuyvData,
                          int camW, int camH,
                          int modelW, int modelH,
                          std::vector<float>& out);

  // ── Frame handoff (camera thread -> BG thread) ───────────────────────────
  std::vector<unsigned char> frameBuf;   // raw YUYV bytes
  unsigned                   frameW = 0; // actual camera width  (e.g. 320)
  unsigned                   frameH = 0; // actual camera height (e.g. 240)
  CameraInfo                 frameCameraInfo;
  CameraMatrix               frameCameraMatrix;
  unsigned                   frameSequence = 0;
  bool                       frameReady  = false;
  std::mutex                 frameMtx;
  std::condition_variable    frameCV;

  // ── Detection result (BG thread -> camera thread) ────────────────────────
  struct Det
  {
    float cx = 0.f;
    float cy = 0.f;
    float radius = 0.f;
    float conf = 0.f;
    bool valid = false;
    unsigned sequence = 0;
    CameraInfo cameraInfo;
    CameraMatrix cameraMatrix;
  };
  Det                                   latestDet;
  std::chrono::steady_clock::time_point detTime{};
  std::mutex                            detMtx;

  // ── ONNX Runtime ─────────────────────────────────────────────────────────
  Ort::Env                      ortEnv;
  std::unique_ptr<Ort::Session> ortSession;
  bool                          modelLoaded = false;
  int                           modelW = 160;
  int                           modelH = 160;

  // ── Background thread ─────────────────────────────────────────────────────
  std::thread       bgThread;
  std::atomic<bool> bgRunning{false};
  LightweightBallKalman kalman;

  int consecutiveSeen = 0;
  unsigned lastProcessedSequence = 0;
};
