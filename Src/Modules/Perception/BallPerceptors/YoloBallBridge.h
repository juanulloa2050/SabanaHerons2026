/**
 * @file YoloBallBridge.h
 *
 * Receives YOLO ball detections from the PC over TCP and publishes them
 * as BallPercept, replacing the on-robot BallPerceptor.
 *
 * Protocol (PC → Robot):
 *   DETS (4 bytes magic)
 *   count (1 byte, 0 = no detection)
 *   [count × { float32_LE x, float32_LE y, float32_LE radius, float32_LE conf }]
 *
 * Upper camera thread listens on upperPort (default 7779).
 * Lower camera thread listens on lowerPort (default 7780).
 * If no detection arrives within timeoutMs, publishes notSeen.
 */

#pragma once

#include "Representations/Configuration/BallSpecification.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Perception/BallPercepts/RawBallPatch.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Framework/Module.h"
#include <vector>

MODULE(YoloBallBridge,
{,
  REQUIRES(BallSpecification),
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  PROVIDES(BallPercept),
  PROVIDES(RawBallPatch),
  LOADS_PARAMETERS(
  {,
    (int)(7779) upperPort,
    (int)(7780) lowerPort,
    (int)(150)  timeoutMs,
    (bool)(true) enabled,
    // Per-camera consecutive-frame requirement before publishing seen.
    // Lower = 1 to minimize lag. Upper = 3 to suppress FPs at the cost of small delay.
    (int)(1) lowerMinConsecutive,
    (int)(1) upperMinConsecutive,
  }),
});

class YoloBallBridge : public YoloBallBridgeBase
{
public:
  YoloBallBridge();
  ~YoloBallBridge();

private:
  void update(BallPercept& ballPercept) override;
  void update(RawBallPatch& rawBallPatch) override;

  bool initServer(int port);
  void acceptClient();
  void pollDetections();

  int serverFd = -1;
  int clientFd = -1;

  struct Det { float x, y, r, conf; };
  static_assert(sizeof(Det) == 16, "Det struct must be 16 bytes");

  std::vector<Det> latest;
  long long lastDetTimeMs   = 0;
  int       consecutiveSeen = 0;
};
