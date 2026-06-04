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
    (float)(0.20f) lowerConf,
    (float)(0.55f) upperConf,
    (int)(1)       lowerMinConsecutive,
    (int)(1)       upperMinConsecutive,
    (int)(2000)    timeoutMs,
    (int)(200)     inferenceIntervalMs,
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

  // Convert YUYV CameraImage → letterboxed float RGB CHW tensor
  static bool buildTensor(const unsigned char* yuyvData,
                          int camW, int camH,
                          int modelW, int modelH,
                          std::vector<float>& out);

  static constexpr int MODEL_W = 160;
  static constexpr int MODEL_H = 160;

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

  // ── Background thread ─────────────────────────────────────────────────────
  std::thread       bgThread;
  std::atomic<bool> bgRunning{false};

  int consecutiveSeen = 0;
  unsigned lastProcessedSequence = 0;
};
