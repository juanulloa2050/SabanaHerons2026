/**
 * @file PythonConsole.cpp
 *
 * This file implements a class that controls a particular robot instance.
 *
 * @author Arne Hasselbring
 */

#include "PythonConsole.h"
#include "Framework/Communication.h"
#include "Framework/Debug.h"

PythonConsole::PythonConsole(const Settings& settings, const std::string& robotName, Debug* debug, int playerNumber) :
  ThreadFrame(settings, robotName, connectReceiverWithRobot(debug), connectSenderWithRobot(debug))
{
  static_cast<void>(playerNumber);
}

PythonConsole::~PythonConsole()
{
}

void PythonConsole::setWorldState(const GroundTruthWorldState& ws)
{
  std::lock_guard<std::mutex> lock(worldStateMutex);
  worldState = ws;
}

void PythonConsole::update()
{
  updateRequested.store(true, std::memory_order_release);
  trigger();
  updatedSignal.wait();
}

void PythonConsole::init()
{
}

bool PythonConsole::main()
{
  injectFrame();
  if(updateRequested.exchange(false, std::memory_order_acq_rel))
    updatedSignal.post();
  return true;
}

void PythonConsole::injectFrame()
{
  GroundTruthWorldState ws;
  {
    std::lock_guard<std::mutex> lock(worldStateMutex);
    ws = worldState;
  }

  const unsigned int t = ++frameCounter * 33u;

  FrameInfo frameInfo;
  frameInfo.time = t;

  CameraInfo cameraInfo;
  GroundTruthOdometryData odometryData;

  FallDownState fallDownState;
  fallDownState.state = FallDownState::upright;

  GroundContactState groundContactState;
  groundContactState.contact = true;

  CameraMatrix cameraMatrix;
  cameraMatrix.isValid = false;

  MotionInfo motionInfo;
  GameControllerData gameControllerData;

  // Match LocalConsole 2D injection order
  debugSender->bin(idFrameBegin)               << "Cognition";
  debugSender->bin(idFrameInfo)                << frameInfo;
  debugSender->bin(idCameraInfo)               << cameraInfo;
  debugSender->bin(idGroundTruthOdometryData)  << odometryData;
  debugSender->bin(idFallDownState)            << fallDownState;
  debugSender->bin(idGroundContactState)       << groundContactState;
  debugSender->bin(idCameraMatrix)             << cameraMatrix;
  debugSender->bin(idMotionInfo)               << motionInfo;
  debugSender->bin(idGameControllerData)       << gameControllerData;
  debugSender->bin(idGroundTruthWorldState)    << ws;
  debugSender->bin(idFrameFinished)            << "Cognition";
  debugSender->send(false);
}

DebugReceiver<MessageQueue>* PythonConsole::connectReceiverWithRobot(Debug* debug)
{
  ASSERT(!debug->debugSender);
  DebugReceiver<MessageQueue>* receiver = new DebugReceiver<MessageQueue>(this, debug->getName());
  debug->debugSender = new DebugSender<MessageQueue>(*receiver, "PythonConsole");
  return receiver;
}

DebugSender<MessageQueue>* PythonConsole::connectSenderWithRobot(Debug* debug) const
{
  ASSERT(!debug->debugReceiver);
  debug->debugReceiver = new DebugReceiver<MessageQueue>(debug, "PythonConsole");
  return new DebugSender<MessageQueue>(*debug->debugReceiver, debug->getName());
}
