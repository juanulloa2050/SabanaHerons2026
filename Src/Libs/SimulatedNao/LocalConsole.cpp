/**
 * @file SimulatedNao/LocalConsole.cpp
 *
 * Implementation of LocalConsole.
 *
 * @author Thomas Röfer
 * @author <A href="mailto:kspiess@tzi.de">Kai Spiess</A>
 */

#include "LocalConsole.h"
#include "SimulatedNao/ConsoleRoboCupCtrl.h"
#include "SimulatedNao/SimulatedRobot2D.h"
#include "SimulatedNao/SimulatedRobot3D.h"
#include "Python/Controller/RLSharedState.h"
#include "Platform/Time.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Sensing/FallDownState.h"
#include "Representations/Sensing/GroundContactState.h"
#include "Framework/Debug.h"

#include <cstdio>
#include <cstdlib>

namespace
{
bool usePyBHRLRootAssist()
{
  const char* value = std::getenv("PYBH_SIMROBOT_RL_ROOT_ASSIST");
  return value && value[0] != '\0' && value[0] != '0';
}
}

LocalConsole::LocalConsole(const Settings& settings, const std::string& robotName, ConsoleRoboCupCtrl* ctrl, const std::string& logFile, Debug* debug) :
  RobotConsole(settings, robotName, ctrl, logFile.empty() ? SystemCall::simulatedRobot : SystemCall::logFileReplay, connectReceiverWithRobot(debug), connectSenderWithRobot(debug)),
  updatedSignal(1),
  playerNumber(settings.playerNumber > 0 ? settings.playerNumber : 1),
  rlSharedStateEnabled(RLSharedStateBridge::isEnabledForTeam(settings.teamNumber))
{
  addPerRobotViews();

  if(mode == SystemCall::logFileReplay)
  {
    this->logFile = logFile;
    if(logPlayer.open(logFile))
    {
      updateAnnotationsFromLog();
      logPlayer.state = LogPlayer::playing;
      if(!ctrl->is2D)
      {
        SimRobot::Object* puppet = RoboCupCtrl::application->resolveObject("RoboCup.puppets." + QString::fromStdString(robotName), SimRobotCore2::body);
        if(puppet)
          simulatedRobot = std::make_unique<SimulatedRobot3D>(puppet);
      }
    }
    else
    {
      ctrl->printLn("Error: Cannot open log file " + logFile);
    }
  }
  else if(mode == SystemCall::simulatedRobot)
  {
    SimRobot::Object* robot = RoboCupCtrl::application->resolveObject("RoboCup.robots." + QString::fromStdString(robotName), ctrl->is2D ? static_cast<int>(SimRobotCore2D::body) : static_cast<int>(SimRobotCore2::body));
    ASSERT(robot);
    if(ctrl->is2D)
      simulatedRobot = std::make_unique<SimulatedRobot2D>(robot);
    else
      simulatedRobot = std::make_unique<SimulatedRobot3D>(robot);
    ctrl->gameController.registerSimulatedRobot(SimulatedRobot::getNumber(robot) - 1, *simulatedRobot);
  }
}

void LocalConsole::init()
{
  RobotConsole::init();
#ifdef MACOS
  setPriority(2);
#endif
}

bool LocalConsole::main()
{
  if(updateSignal.tryWait())
  {
    {
      // Only one thread can access *this now.
      SYNC;

      if(mode == SystemCall::simulatedRobot)
      {
        if(!ctrl->is2D)
        {
          if(jointLastTimestampSent != jointSensorData.timestamp)
          {
            debugSender->bin(idFrameBegin) << "Motion";
            debugSender->bin(idJointSensorData) << jointSensorData;
            debugSender->bin(idFsrSensorData) << fsrSensorData;
            debugSender->bin(idInertialSensorData) << inertialSensorData;
            debugSender->bin(idGroundTruthOdometryData) << odometryData;
            debugSender->bin(idFrameFinished) << "Motion";
            jointLastTimestampSent = jointSensorData.timestamp;
          }

          if(imageLastTimestampSent != cameraImage.timestamp)
          {
            std::string perception = cameraInfo.getThreadName();
            debugSender->bin(idFrameBegin) << perception;
            if(imageCalculated)
              debugSender->bin(idCameraImage) << cameraImage;
            else
            {
              FrameInfo frameInfo;
              frameInfo.time = cameraImage.timestamp;
              debugSender->bin(idFrameInfo) << frameInfo;
            }
            debugSender->bin(idCameraInfo) << cameraInfo;
            debugSender->bin(idGroundTruthWorldState) << worldState;
            debugSender->bin(idFrameFinished) << perception;

            debugSender->bin(idFrameBegin) << "Cognition";
            debugSender->bin(idGameControllerData) << gameControllerData;
            debugSender->bin(idGroundTruthWorldState) << worldState;
            debugSender->bin(idFrameFinished) << "Cognition";
            debugSender->bin(idFrameBegin) << "Audio";
            debugSender->bin(idWhistle) << whistle;
            debugSender->bin(idFrameFinished) << "Audio";
            imageLastTimestampSent = cameraImage.timestamp;
          }
        }
        else
        {
          debugSender->bin(idFrameBegin) << "Cognition";
          FrameInfo frameInfo;
          frameInfo.time = cameraImage.timestamp;
          debugSender->bin(idFrameInfo) << frameInfo;
          debugSender->bin(idCameraInfo) << cameraInfo;
          debugSender->bin(idGroundTruthOdometryData) << odometryData;
          FallDownState fallDownState;
          fallDownState.state = FallDownState::upright;
          debugSender->bin(idFallDownState) << fallDownState;
          GroundContactState groundContactState;
          groundContactState.contact = true;
          debugSender->bin(idGroundContactState) << groundContactState;
          CameraMatrix cameraMatrix;
          cameraMatrix.isValid = false;
          debugSender->bin(idCameraMatrix) << cameraMatrix;
          debugSender->bin(idMotionInfo) << motionInfo;
          debugSender->bin(idGameControllerData) << gameControllerData;
          debugSender->bin(idGroundTruthWorldState) << worldState;
          debugSender->bin(idFrameFinished) << "Cognition";
        }
      }
      debugSender->send(true);
    }

    updatedSignal.post();
  }
  return true;
}

void LocalConsole::update()
{
  RobotConsole::update();
  updatedSignal.wait();

  QString statusText;
  {
    // Only one thread can access *this now.
    SYNC;

    if(mode == SystemCall::logFileReplay)
    {
      if(logPlayer.state == LogPlayer::playing && (logPlayer.cycle || logPlayer.frame() + 1 < logPlayer.frames()))
      {
        const std::string threadName = logPlayer.threadOf(logPlayer.frame() +  1);
        if(threadName != "" && threadData[threadName].logAcknowledged)
        {
          logPlayer.playBack(logPlayer.frame() + 1);
          threadData[threadName].currentFrame = logPlayer.frame();
          threadData[threadName].logAcknowledged = false;
        }
      }
      if(simulatedRobot)
      {
        if(RobotConsole::jointSensorData.timestamp)
          simulatedRobot->setJointRequest(reinterpret_cast<JointRequest&>(RobotConsole::jointSensorData));
        else
          simulatedRobot->getAndSetJointData(jointRequest, jointSensorData);
        simulatedRobot->moveRobot({0, 0, 1000.f}, RobotConsole::inertialSensorData.angle.cast<float>(), true);
      }
    }
    if(mode == SystemCall::simulatedRobot)
    {
      applyPendingRLReset();
      applyPendingRLDynamicWorld();

      unsigned now = Time::getCurrentSystemTime();
      if(now >= nextImageTimestamp)
      {
        unsigned newNextImageTimestamp = ctrl->globalNextImageTimestamp;
        if(newNextImageTimestamp == nextImageTimestamp)
        {
          int imageDelay = (2000 / ctrl->calculateImageFps + 1) >> 1;
          int duration = now - ctrl->globalNextImageTimestamp;
          ctrl->globalNextImageTimestamp = (duration >= imageDelay ? now : ctrl->globalNextImageTimestamp) + imageDelay;
          newNextImageTimestamp = ctrl->globalNextImageTimestamp;
        }
        nextImageTimestamp = newNextImageTimestamp;

        if((imageCalculated = ctrl->calculateImage))
          simulatedRobot->getImage(cameraImage, cameraInfo);
        else
        {
          simulatedRobot->getCameraInfo(cameraInfo);
          cameraImage.timestamp = now;
        }
        simulatedRobot->getRobotPose(robotPose);
        simulatedRobot->getWorldState(worldState);
        simulatedRobot->toggleCamera();
      }
      else
        simulatedRobot->getRobotPose(robotPose);

      if(jointCalibrationChanged)
      {
        simulatedRobot->setJointCalibration(jointCalibration);
        jointCalibrationChanged = false;
      }
      simulatedRobot->getOdometryData(robotPose, odometryData);
      simulatedRobot->getSensorData(fsrSensorData, inertialSensorData);
      JointRequest requestForSimulation = jointRequest;
      if(rlSharedStateEnabled)
      {
        RLPlayerIO& io = RLSharedState::instance().player(playerNumber);
        io.lock();
        if(io.motionJointRequestValid)
        {
          for(std::size_t i = 0; i < io.motionJointAngles.size() && i < Joints::numOfJoints; ++i)
            requestForSimulation.angles[static_cast<Joints::Joint>(i)] = io.motionJointAngles[i];
        }
        io.unlock();
      }
      simulatedRobot->getAndSetJointData(requestForSimulation, jointSensorData);
      simulatedRobot->getAndSetMotionData(motionRequest, motionInfo);
      Pose2f assistedSpeed;
      bool appliedRootAssist = false;
      if(usePyBHRLRootAssist())
      {
        float targetX = 0.f;
        float targetY = 0.f;
        float targetTheta = 0.f;
        bool shouldMove = false;
        {
          RLPlayerIO& io = RLSharedState::instance().player(playerNumber);
          io.lock();
          shouldMove = io.debugMotionEnginePhase == static_cast<int>(MotionPhase::walk) &&
                       io.debugMotionEngineGroundContact &&
                       io.debugMotionEngineFallState >= static_cast<int>(FallDownState::upright) &&
                       io.debugMotionEngineFallState <= static_cast<int>(FallDownState::staggering) &&
                       io.debugProviderMotionRequest == static_cast<int>(MotionRequest::walkToPose);
          targetX = io.targetX;
          targetY = io.targetY;
          targetTheta = io.targetTheta;
          io.unlock();
        }
        if(shouldMove)
        {
          const float dt = ctrl->simStepLength * 0.001f;
          const float maxTranslation = 120.f * dt;
          const float maxRotation = 0.8f * dt;
          Vector2f fieldStep(targetX - robotPose.translation.x(), targetY - robotPose.translation.y());
          const float distance = fieldStep.norm();
          if(distance > maxTranslation && distance > 0.f)
            fieldStep *= maxTranslation / distance;
          const Angle rotationStep = Rangea(-maxRotation, maxRotation).limit(Angle::normalize(targetTheta - robotPose.rotation));
          const Vector2f nextTranslation = robotPose.translation + fieldStep;
          const Angle nextRotation = Angle::normalize(robotPose.rotation + rotationStep);
          simulatedRobot->moveRobotPerTeam(Vector3f(nextTranslation.x(), nextTranslation.y(), ctrl->is2D ? 0.f : 320.f),
                                           Vector3f(0.f, 0.f, nextRotation),
                                           true);
          assistedSpeed = Pose2f(rotationStep / dt, fieldStep / dt);
          appliedRootAssist = true;
        }
      }
      if(rlSharedStateEnabled)
      {
        RLPlayerIO& io = RLSharedState::instance().player(playerNumber);
        io.lock();
        io.debugMotionRequest = appliedRootAssist ? io.debugMotionEngineInputRequest : static_cast<int>(motionRequest.motion);
        io.debugExecutedPhase = static_cast<int>(motionInfo.executedPhase);
        io.debugMotionSpeedX = appliedRootAssist ? assistedSpeed.translation.x() : motionInfo.speed.translation.x();
        io.debugMotionSpeedY = appliedRootAssist ? assistedSpeed.translation.y() : motionInfo.speed.translation.y();
        io.debugMotionSpeedRot = appliedRootAssist ? static_cast<float>(assistedSpeed.rotation) : static_cast<float>(motionInfo.speed.rotation);
        io.debugLHipPitch = requestForSimulation.angles[Joints::lHipPitch];
        io.debugLKneePitch = requestForSimulation.angles[Joints::lKneePitch];
        io.debugRHipPitch = requestForSimulation.angles[Joints::rHipPitch];
        io.debugRKneePitch = requestForSimulation.angles[Joints::rKneePitch];
        io.unlock();
      }

      if(rlSharedStateEnabled)
      {
        Pose2f simRobotPose;
        simulatedRobot->getRobotPose(simRobotPose);
        RLPlayerIO& io = RLSharedState::instance().player(playerNumber);
        io.lock();
        io.simRobotX = simRobotPose.translation.x();
        io.simRobotY = simRobotPose.translation.y();
        io.simRobotTheta = static_cast<float>(simRobotPose.rotation);
        io.unlock();
      }

      ctrl->gameController.getGameControllerData(gameControllerData);
      ctrl->gameController.getWhistle(whistle);
    }

    if(mode == SystemCall::logFileReplay)
    {
      statusText = QString("replaying ") +
#ifdef WINDOWS
                   QString::fromLatin1(logFile.c_str())
#else
                   logFile.c_str()
#endif
                   + " ";
      if(logPlayer.frame() + 1 < logPlayer.frames())
        statusText += QString("%1").arg(static_cast<int>(logPlayer.frame()));
      else
        statusText += "finished";
    }
    else if(!logPlayer.empty())
      statusText += QString("recorded %1 mb").arg(static_cast<float>(logPlayer.size()) / 1000000.f, 0, 'f', 1);
  }

  updateSignal.post();
  trigger(); // invoke a call of main()

  if(pollingFor)
  {
    statusText += statusText != "" ? ", polling for " : "polling for ";
    statusText += pollingFor;
  }

  if(statusText.size() > 0)
    ctrl->printStatusText((QString::fromStdString(robotName) + ": " + statusText).toUtf8());
}

void LocalConsole::applyPendingRLReset()
{
  if(!simulatedRobot || !rlSharedStateEnabled)
    return;

  RLPlayerIO& io = RLSharedState::instance().player(playerNumber);
  float ballX = 0.f;
  float ballY = 0.f;
  float robotX = 0.f;
  float robotY = 0.f;
  float robotTheta = 0.f;
  int teammateCount = 0;
  int opponentCount = 0;
  std::array<RLWorldPlayer, RLSharedStateBridge::maxWorldPlayersPerTeam> teammates{};
  std::array<RLWorldPlayer, RLSharedStateBridge::maxWorldPlayersPerTeam> opponents{};
  unsigned int requestId = 0;
  bool shouldApply = false;

  io.lock();
  if(io.resetPending && io.resetAppliedId != io.resetRequestId)
  {
    ballX = io.resetBallX;
    ballY = io.resetBallY;
    robotX = io.resetRobotX;
    robotY = io.resetRobotY;
    robotTheta = io.resetRobotTheta;
    teammateCount = io.resetTeammateCount;
    opponentCount = io.resetOpponentCount;
    teammates = io.resetTeammates;
    opponents = io.resetOpponents;
    requestId = io.resetRequestId;
    io.resetPending = false;
    io.obsReady = false;
    shouldApply = true;
  }
  io.unlock();

  if(!shouldApply)
    return;

  std::string error;
  for(int i = 0; i < teammateCount; ++i)
  {
    const RLWorldPlayer& player = teammates[static_cast<std::size_t>(i)];
    if(!simulatedRobot->hasRobotByNumberPerTeam(player.number, false))
    {
      error = std::string("RL reset could not find teammate robot ") + std::to_string(player.number);
      break;
    }
  }
  for(int i = 0; error.empty() && i < opponentCount; ++i)
  {
    const RLWorldPlayer& player = opponents[static_cast<std::size_t>(i)];
    if(!simulatedRobot->hasRobotByNumberPerTeam(player.number, true))
    {
      error = std::string("RL reset could not find opponent robot ") + std::to_string(player.number);
      break;
    }
  }

  if(error.empty())
  {
    simulatedRobot->enablePhysics(false);
    simulatedRobot->moveRobotPerTeam(
      Vector3f(robotX, robotY, ctrl->is2D ? 0.f : 320.f),
      Vector3f(0.f, 0.f, robotTheta),
      true
    );
    simulatedRobot->moveBallPerTeam(Vector3f(ballX, ballY, ctrl->is2D ? 0.f : 50.f), true);
    for(int i = 0; i < teammateCount; ++i)
    {
      const RLWorldPlayer& player = teammates[static_cast<std::size_t>(i)];
      simulatedRobot->moveRobotByNumberPerTeam(
        player.number,
        false,
        Vector3f(player.x, player.y, ctrl->is2D ? 0.f : 320.f),
        Vector3f(0.f, 0.f, player.theta),
        true);
    }
    for(int i = 0; i < opponentCount; ++i)
    {
      const RLWorldPlayer& player = opponents[static_cast<std::size_t>(i)];
      simulatedRobot->moveRobotByNumberPerTeam(
        player.number,
        true,
        Vector3f(player.x, player.y, ctrl->is2D ? 0.f : 320.f),
        Vector3f(0.f, 0.f, player.theta),
        true);
    }
    // The visible RL reset must leave the requested world state as the last
    // applied operation. For RL scenes, the console file already puts the game
    // into `playing`, and re-triggering GameController transitions here can
    // cause SetupPoses/ready logic to overwrite the requested teleport.
    simulatedRobot->enablePhysics(true);
  }

  io.lock();
  io.resetAppliedId = requestId;
  io.worldResultRequestId = requestId;
  io.worldResultOk = error.empty();
  const std::string finalError = error.empty() ? std::string() : error;
  std::snprintf(io.worldResultError, sizeof(io.worldResultError), "%s", finalError.c_str());
  io.unlock();

  if(!error.empty())
    ctrl->printLn(error);
}

void LocalConsole::applyPendingRLDynamicWorld()
{
  if(!simulatedRobot || !rlSharedStateEnabled)
    return;

  RLPlayerIO& io = RLSharedState::instance().player(playerNumber);
  float ballX = 0.f;
  float ballY = 0.f;
  bool hasBall = false;
  float robotX = 0.f;
  float robotY = 0.f;
  float robotTheta = 0.f;
  bool hasRobotPose = false;
  int teammateCount = 0;
  int opponentCount = 0;
  bool applyTeammates = false;
  bool applyOpponents = false;
  std::array<RLWorldPlayer, RLSharedStateBridge::maxWorldPlayersPerTeam> teammates{};
  std::array<RLWorldPlayer, RLSharedStateBridge::maxWorldPlayersPerTeam> opponents{};
  unsigned int requestId = 0;
  bool shouldApply = false;

  io.lock();
  if(io.dynamicPending && io.dynamicAppliedId != io.dynamicRequestId)
  {
    ballX = io.dynamicBallX;
    ballY = io.dynamicBallY;
    hasBall = io.dynamicHasBall;
    robotX = io.dynamicRobotX;
    robotY = io.dynamicRobotY;
    robotTheta = io.dynamicRobotTheta;
    hasRobotPose = io.dynamicHasRobotPose;
    teammateCount = io.dynamicTeammateCount;
    opponentCount = io.dynamicOpponentCount;
    applyTeammates = io.dynamicApplyTeammates;
    applyOpponents = io.dynamicApplyOpponents;
    teammates = io.dynamicTeammates;
    opponents = io.dynamicOpponents;
    requestId = io.dynamicRequestId;
    io.dynamicPending = false;
    io.obsReady = false;
    shouldApply = true;
  }
  io.unlock();

  if(!shouldApply)
    return;

  std::string error;
  if(applyTeammates)
  {
    for(int i = 0; i < teammateCount; ++i)
    {
      const RLWorldPlayer& player = teammates[static_cast<std::size_t>(i)];
      if(!simulatedRobot->hasRobotByNumberPerTeam(player.number, false))
      {
        error = std::string("RL dynamic world could not find teammate robot ") + std::to_string(player.number);
        break;
      }
    }
  }
  if(error.empty() && applyOpponents)
  {
    for(int i = 0; i < opponentCount; ++i)
    {
      const RLWorldPlayer& player = opponents[static_cast<std::size_t>(i)];
      if(!simulatedRobot->hasRobotByNumberPerTeam(player.number, true))
      {
        error = std::string("RL dynamic world could not find opponent robot ") + std::to_string(player.number);
        break;
      }
    }
  }

  if(error.empty())
  {
    simulatedRobot->enablePhysics(false);
    if(hasRobotPose)
      simulatedRobot->moveRobotPerTeam(Vector3f(robotX, robotY, ctrl->is2D ? 0.f : 320.f), Vector3f(0.f, 0.f, robotTheta), true);
    if(hasBall)
      simulatedRobot->moveBallPerTeam(Vector3f(ballX, ballY, ctrl->is2D ? 0.f : 50.f), true);
    if(applyTeammates)
    {
      for(int i = 0; i < teammateCount; ++i)
      {
        const RLWorldPlayer& player = teammates[static_cast<std::size_t>(i)];
        if(simulatedRobot->isRobotUprightByNumberPerTeam(player.number, false))
          simulatedRobot->moveRobotByNumberPerTeam(
            player.number,
            false,
            Vector3f(player.x, player.y, ctrl->is2D ? 0.f : 320.f),
            Vector3f(0.f, 0.f, player.theta),
            true);
      }
    }
    if(applyOpponents)
    {
      for(int i = 0; i < opponentCount; ++i)
      {
        const RLWorldPlayer& player = opponents[static_cast<std::size_t>(i)];
        if(simulatedRobot->isRobotUprightByNumberPerTeam(player.number, true))
          simulatedRobot->moveRobotByNumberPerTeam(
            player.number,
            true,
            Vector3f(player.x, player.y, ctrl->is2D ? 0.f : 320.f),
            Vector3f(0.f, 0.f, player.theta),
            true);
      }
    }
    simulatedRobot->enablePhysics(true);
  }

  io.lock();
  io.dynamicAppliedId = requestId;
  io.worldResultRequestId = requestId;
  io.worldResultOk = error.empty();
  const std::string finalError = error.empty() ? std::string() : error;
  std::snprintf(io.worldResultError, sizeof(io.worldResultError), "%s", finalError.c_str());
  io.unlock();

  if(!error.empty())
    ctrl->printLn(error);
}

DebugReceiver<MessageQueue>* LocalConsole::connectReceiverWithRobot(Debug* debug)
{
  ASSERT(!debug->debugSender);
  DebugReceiver<MessageQueue>* receiver = new DebugReceiver<MessageQueue>(this, debug->getName());
  debug->debugSender = new DebugSender<MessageQueue>(*receiver, "LocalConsole");
  return receiver;
}

DebugSender<MessageQueue>* LocalConsole::connectSenderWithRobot(Debug* debug) const
{
  ASSERT(!debug->debugReceiver);
  debug->debugReceiver = new DebugReceiver<MessageQueue>(debug, "LocalConsole");
  return new DebugSender<MessageQueue>(*debug->debugReceiver, debug->getName());
}
