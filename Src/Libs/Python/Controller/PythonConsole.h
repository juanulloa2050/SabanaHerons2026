/**
 * @file PythonConsole.h
 *
 * This file declares a class that controls a particular robot instance.
 *
 * @author Arne Hasselbring
 */

#pragma once

#include "Framework/Communication.h"
#include "Framework/ThreadFrame.h"
#include "Platform/Semaphore.h"
#include "Representations/Communication/GameControllerData.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GroundTruthWorldState.h"
#include "Representations/MotionControl/OdometryData.h"
#include "Representations/Sensing/FallDownState.h"
#include "Representations/Sensing/GroundContactState.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/MotionControl/MotionInfo.h"
#include "Streaming/MessageIDs.h"
#include "Streaming/MessageQueue.h"
#include <atomic>
#include <mutex>
#include <string>

class Debug;
struct Settings;

class PythonConsole : public ThreadFrame
{
public:
  PythonConsole(const Settings& settings, const std::string& robotName, Debug* debug);
  ~PythonConsole() override;

  /**
   * Triggers one synchronised Cognition frame (called from Python main thread).
   * Calls trigger() to wake the robot thread, then waits for frame completion.
   */
  void update();

  /** Set the world state to inject on the next frame. Thread-safe. */
  void setWorldState(const GroundTruthWorldState& ws);

protected:
  int getPriority() const override { return 0; }
  void init() override;
  bool main() override;
  void terminate() override {}

private:
  DebugReceiver<MessageQueue>* connectReceiverWithRobot(Debug* debug);
  DebugSender<MessageQueue>*   connectSenderWithRobot(Debug* debug) const;

  void injectFrame();

  GroundTruthWorldState worldState;
  std::mutex            worldStateMutex;
  unsigned int          frameCounter = 0;
  std::atomic<bool>     updateRequested{false};

  Semaphore updatedSignal;  ///< robot thread → main thread: "frame injected"
};
