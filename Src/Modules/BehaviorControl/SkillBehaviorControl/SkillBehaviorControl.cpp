/**
 * @file SkillBehaviorControl.cpp
 *
 * This file implements the behavior control for the skill layer.
 *
 * @author Arne Hasselbring
 */

#include "SkillBehaviorControl.h"
#include "Python/Controller/RLSharedState.h"
#include "Streaming/TypeRegistry.h"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

MAKE_MODULE(SkillBehaviorControl, SkillBehaviorControl::getExtModuleInfo);

namespace
{
  struct ObstacleSummary
  {
    int count = 0;
    int teammateCount = 0;
    int opponentCount = 0;
    int uncertainCount = 0;
    int frontCount = 0;
    float nearestTeammate = std::numeric_limits<float>::max();
    float nearestOpponent = std::numeric_limits<float>::max();
    float nearestUncertain = std::numeric_limits<float>::max();
    float nearestTeammateFront = std::numeric_limits<float>::max();
    float nearestOpponentFront = std::numeric_limits<float>::max();
    float nearestUncertainFront = std::numeric_limits<float>::max();
    float nearestObstacle = std::numeric_limits<float>::max();
    float nearestObstacleAngle = 0.f;
    float nearestFrontObstacle = std::numeric_limits<float>::max();
    float nearestOpponentAngle = 0.f;
  };

  enum class RLObsExportMode
  {
    estimated = 0,
    corrected = 1,
  };

  RLObsExportMode getRLObsExportMode()
  {
    const char* value = std::getenv("PYBH_RL_OBS_MODE");
    if(!value || !value[0])
      return RLObsExportMode::corrected;
    const std::string mode(value);
    return mode == "estimated" ? RLObsExportMode::estimated : RLObsExportMode::corrected;
  }

  void updateNearest(float distance, bool isFront, float& nearest, float& nearestFront)
  {
    if(distance < nearest)
      nearest = distance;
    if(isFront && distance < nearestFront)
      nearestFront = distance;
  }

  ObstacleSummary summarizeObstacles(const ObstacleModel& obstacleModel)
  {
    ObstacleSummary summary;
    for(const Obstacle& obstacle : obstacleModel.obstacles)
    {
      ++summary.count;
      const float distance = obstacle.center.norm();
      const bool isFront = obstacle.center.x() > 0.f;
      const float angle = std::atan2(obstacle.center.y(), obstacle.center.x());
      if(distance < summary.nearestObstacle)
      {
        summary.nearestObstacle = distance;
        summary.nearestObstacleAngle = angle;
      }
      if(isFront)
      {
        ++summary.frontCount;
        if(distance < summary.nearestFrontObstacle)
          summary.nearestFrontObstacle = distance;
      }
      if(obstacle.isTeammate())
      {
        ++summary.teammateCount;
        updateNearest(distance, isFront, summary.nearestTeammate, summary.nearestTeammateFront);
      }
      else if(obstacle.isOpponent())
      {
        ++summary.opponentCount;
        if(distance < summary.nearestOpponent)
          summary.nearestOpponentAngle = angle;
        updateNearest(distance, isFront, summary.nearestOpponent, summary.nearestOpponentFront);
      }
      else
      {
        ++summary.uncertainCount;
        updateNearest(distance, isFront, summary.nearestUncertain, summary.nearestUncertainFront);
      }
    }
    return summary;
  }

  float boundedDistance(float value, const FieldDimensions& fieldDimensions)
  {
    if(value == std::numeric_limits<float>::max())
      return fieldDimensions.xPosOpponentGroundLine * 2.f;
    return value;
  }
}

SkillBehaviorControl::SkillBehaviorControl() :
  Cabsl<SkillBehaviorControl>(const_cast<ActivationGraph*>(&theActivationGraph)),
  theSkillRegistry("skills.cfg", const_cast<ActivationGraph&>(theActivationGraph), theArmMotionRequest, theBehaviorStatus, theCalibrationRequest, theHeadMotionRequest, theMotionRequest, theOptionalImageRequest)
{}

std::vector<ModuleBase::Info> SkillBehaviorControl::getExtModuleInfo()
{
  auto result = SkillBehaviorControl::getModuleInfo();
  SkillImplementationCreatorBase::addToModuleInfo(SkillImplementationCreatorList<Skill>::first, result);
  return result;
}

void SkillBehaviorControl::update(ActivationGraph&)
{
  DECLARE_DEBUG_RESPONSE("option:HandleRefereeSignal:now");

  theBehaviorStatus.calibrationFinished = false;
  theBehaviorStatus.passTarget = -1;
  theBehaviorStatus.passOrigin = -1;
  theBehaviorStatus.walkingTo = Vector2f::Zero();
  theBehaviorStatus.speed = 0.f;
  theBehaviorStatus.shootingTo.reset();

  theArmMotionRequest.armMotion[Arms::left] = ArmMotionRequest::none;
  theArmMotionRequest.armMotion[Arms::right] = ArmMotionRequest::none;

  theHeadMotionRequest.mode = HeadMotionRequest::panTiltMode;
  theHeadMotionRequest.cameraControlMode = HeadMotionRequest::autoCamera;
  theHeadMotionRequest.pan = JointAngles::off;
  theHeadMotionRequest.tilt = JointAngles::off;
  theHeadMotionRequest.speed = 150_deg;
  theHeadMotionRequest.stopAndGoMode = false;

  theMotionRequest.shouldInterceptBall = false;
  theMotionRequest.odometryData = theOdometryData;
  theMotionRequest.ballEstimate = theBallModel.estimate;
  theMotionRequest.ballEstimateTimestamp = theFrameInfo.time;
  theMotionRequest.ballTimeWhenLastSeen = theBallModel.timeWhenLastSeen;
  theMotionRequest.shouldInterceptBall = useNewHandleCatchBallBehavior && theFieldBall.interceptBall;

  theOptionalImageRequest.sendImage = false;

  // On request, tell the user whether a USB drive is mounted.
  if(theEnhancedKeyStates.hitStreak[KeyStates::headMiddle] == 3)
  {
    if(SystemCall::usbIsMounted())
      SystemCall::say("USB mounted");
    else
      SystemCall::say("USB not mounted");
  }

  // On request, tell the user the temperature of the hottest joint.
  if((theEnhancedKeyStates.hitStreak[KeyStates::headRear] == 3 && theEnhancedKeyStates.pressedDuration[KeyStates::headFront] > 0) ||
     (theEnhancedKeyStates.hitStreak[KeyStates::headFront] == 3 && theEnhancedKeyStates.pressedDuration[KeyStates::headRear] > 0))
    SystemCall::say((std::string(TypeRegistry::getEnumName(theGameState.color())) + " " + std::to_string(theGameState.playerNumber) + " " +
                     std::regex_replace(TypeRegistry::getEnumName(theRobotHealth.jointWithMaxTemperature), std::regex("([A-Z])"), " $1") + " " +
                     std::to_string(theJointSensorData.temperatures[theRobotHealth.jointWithMaxTemperature])).c_str(), true);
  else if(theEnhancedKeyStates.hitStreak[KeyStates::headFront] == 3)
  {
    // On request, tell the user the deployed configuration of this robot.
    std::string wavName = Global::getSettings().headName.c_str();
    wavName.append(".wav");
    SystemCall::playSound(wavName.c_str());
    SystemCall::say(("  my name is  " + theRobotHealth.robotName+ " im wearing  " +
                     std::string(TypeRegistry::getEnumName(theGameState.color())) +
                     " with the number " +
                     std::to_string(theGameState.playerNumber)).c_str(), true, 0.7f);
  }

  theSkillRegistry.modifyAllParameters();

  theSkillRegistry.preProcess(theFrameInfo.time);
  beginFrame(theFrameInfo.time);

  if(RLSharedStateBridge::isEnabledForTeam(theGameState.ownTeam.number))
  {
    const int n = theGameState.playerNumber > 0 ? theGameState.playerNumber : 1;
    RLPlayerIO& io = RLSharedState::instance().player(n);
    io.lock();
    io.debugZweikampfActive = false;
    io.unlock();
  }

  PlaySoccer();

  endFrame();
  theSkillRegistry.postProcess();

  if(RLSharedStateBridge::isEnabledForTeam(theGameState.ownTeam.number))
  {
    const int n = theGameState.playerNumber > 0 ? theGameState.playerNumber : 1;
    RLPlayerIO& io = RLSharedState::instance().player(n);
    bool postObs = false;
    io.lock();
    io.debugSkillBehaviorSkillRequest = static_cast<int>(theSkillRequest.skill);
    io.debugSkillBehaviorMotionRequest = static_cast<int>(theMotionRequest.motion);
    ++io.debugSkillBehaviorCallCount;
    io.debugSkillBehaviorWalkTargetX = theMotionRequest.walkTarget.translation.x();
    io.debugSkillBehaviorWalkTargetY = theMotionRequest.walkTarget.translation.y();
    io.debugSkillBehaviorWalkTargetTheta = static_cast<float>(theMotionRequest.walkTarget.rotation);
    io.debugMotionObstacleAvoidanceX = theMotionRequest.obstacleAvoidance.avoidance.x();
    io.debugMotionObstacleAvoidanceY = theMotionRequest.obstacleAvoidance.avoidance.y();
    io.debugMotionObstaclePathCount = static_cast<int>(theMotionRequest.obstacleAvoidance.path.size());
    if(!theMotionRequest.obstacleAvoidance.path.empty())
    {
      const auto& firstSegment = theMotionRequest.obstacleAvoidance.path.front();
      io.debugMotionObstacleFirstX = firstSegment.obstacle.center.x();
      io.debugMotionObstacleFirstY = firstSegment.obstacle.center.y();
      io.debugMotionObstacleFirstRadius = firstSegment.obstacle.radius;
      io.debugMotionObstacleFirstClockwise = firstSegment.clockwise;
    }
    else
    {
      io.debugMotionObstacleFirstX = 0.f;
      io.debugMotionObstacleFirstY = 0.f;
      io.debugMotionObstacleFirstRadius = 0.f;
      io.debugMotionObstacleFirstClockwise = false;
    }
    const Vector2f ballOnField = theFieldBall.positionOnField;
    const Vector2f ballRelative = theFieldBall.positionRelative;
    const Vector2f ballEndOnField = theFieldBall.endPositionOnField;
    const Vector2f ballEndRelative = theFieldBall.endPositionRelative;
    const Vector2f ballVelocity = theBallModel.estimate.velocity;
    const ObstacleSummary obstacleSummary = summarizeObstacles(theObstacleModel);

    const RLObsExportMode obsExportMode = getRLObsExportMode();
    const float estimatedRobotX = theRobotPose.translation.x();
    const float estimatedRobotY = theRobotPose.translation.y();
    const float estimatedRobotTheta = static_cast<float>(theRobotPose.rotation);
    const float estimatedBallX = ballOnField.x();
    const float estimatedBallY = ballOnField.y();
    float exportedRobotX = estimatedRobotX;
    float exportedRobotY = estimatedRobotY;
    float exportedRobotTheta = estimatedRobotTheta;
    float exportedBallX = estimatedBallX;
    float exportedBallY = estimatedBallY;
    Vector2f exportedBallRelative = ballRelative;
    Vector2f exportedBallEndRelative = ballEndRelative;
    float exportedTimeSinceBallSeen = static_cast<float>(theFieldBall.timeSinceBallWasSeen);
    float exportedTimeSinceBallDisappeared = static_cast<float>(theFieldBall.timeSinceBallDisappeared);
    float exportedBallSeenPercentage = static_cast<float>(theBallModel.seenPercentage);
    bool exportedBallConsistent = theFieldBall.ballPositionConsistentWithGameState;
    const float naturalTimeSinceBallSeen = exportedTimeSinceBallSeen;
    const float naturalBallSeenPercentage = exportedBallSeenPercentage;
    const bool naturalBallConsistent = exportedBallConsistent;
    bool correctedRobotPose = false;
    bool correctedBall = false;
    int ballExportSource = 0; // 0=estimated, 1=reset fallback, 2=dynamic fallback

    const float simPoseError = std::hypot(estimatedRobotX - io.simRobotX, estimatedRobotY - io.simRobotY);
    if(obsExportMode == RLObsExportMode::corrected && simPoseError > 500.f)
    {
      exportedRobotX = io.simRobotX;
      exportedRobotY = io.simRobotY;
      exportedRobotTheta = io.simRobotTheta;
      correctedRobotPose = true;
    }

    const bool staleBall = theBallModel.seenPercentage <= 0.f && theFieldBall.timeSinceBallWasSeen > 1000;
    if(obsExportMode == RLObsExportMode::corrected && staleBall)
    {
      exportedBallX = io.dynamicHasBall ? io.dynamicBallX : io.resetBallX;
      exportedBallY = io.dynamicHasBall ? io.dynamicBallY : io.resetBallY;
      exportedTimeSinceBallSeen = 0.f;
      exportedTimeSinceBallDisappeared = 0.f;
      exportedBallSeenPercentage = 100.f;
      exportedBallConsistent = true;
      correctedBall = true;
      ballExportSource = io.dynamicHasBall ? 2 : 1;
    }

    const Pose2f exportedRobotPose(exportedRobotTheta, exportedRobotX, exportedRobotY);
    if(correctedRobotPose || correctedBall)
    {
      const Vector2f exportedBallOnField(exportedBallX, exportedBallY);
      exportedBallRelative = exportedRobotPose.inverse() * exportedBallOnField;
      exportedBallEndRelative = correctedBall
                                   ? exportedBallRelative
                                   : exportedRobotPose.inverse() * ballEndOnField;
    }

    const Vector2f exportedBallOnField(exportedBallX, exportedBallY);
    const float exportedShotQualityNoObstacles = theExpectedGoals.xG ? theExpectedGoals.xG(exportedBallOnField) : 0.f;
    const float exportedShotOpeningWithObstacles = theExpectedGoals.getRating ? theExpectedGoals.getRating(exportedBallOnField) : 0.f;

    io.ballX = exportedBallX;
    io.ballY = exportedBallY;
    io.robotX = exportedRobotX;
    io.robotY = exportedRobotY;
    io.robotTheta = exportedRobotTheta;
    io.estimatedBallX = estimatedBallX;
    io.estimatedBallY = estimatedBallY;
    io.estimatedRobotX = estimatedRobotX;
    io.estimatedRobotY = estimatedRobotY;
    io.estimatedRobotTheta = estimatedRobotTheta;
    io.ballRelX = exportedBallRelative.x();
    io.ballRelY = exportedBallRelative.y();
    io.ballEndRelX = exportedBallEndRelative.x();
    io.ballEndRelY = exportedBallEndRelative.y();
    io.obsExportMode = static_cast<int>(obsExportMode);
    io.correctedExportedRobotPose = correctedRobotPose;
    io.correctedExportedBall = correctedBall;
    io.ballExportSource = ballExportSource;
    io.ballExportFresh = !correctedBall;
    io.naturalTimeSinceBallSeen = naturalTimeSinceBallSeen;
    io.naturalBallSeenPercentage = naturalBallSeenPercentage;
    io.naturalBallConsistentWithGameState = naturalBallConsistent;
    io.ballVelX = ballVelocity.x();
    io.ballVelY = ballVelocity.y();
    io.timeSinceBallSeen = exportedTimeSinceBallSeen;
    io.timeSinceBallDisappeared = exportedTimeSinceBallDisappeared;
    io.ballSeenPercentage = exportedBallSeenPercentage;
    io.ballConsistentWithGameState = exportedBallConsistent;
    io.canScoreNow = exportedShotOpeningWithObstacles > 0.8f;
    io.shotQualityNoObstacles = exportedShotQualityNoObstacles;
    io.shotOpeningWithObstacles = exportedShotOpeningWithObstacles;
    io.passOptionsCount = static_cast<float>(theTeamData.teammates.size());
    io.nearestTeammateDist = boundedDistance(obstacleSummary.nearestTeammate, theFieldDimensions);
    io.nearestOpponentDist = boundedDistance(obstacleSummary.nearestOpponent, theFieldDimensions);
    io.nearestUncertainObstacleDist = boundedDistance(obstacleSummary.nearestUncertain, theFieldDimensions);
    io.nearestTeammateFrontDist = boundedDistance(obstacleSummary.nearestTeammateFront, theFieldDimensions);
    io.nearestOpponentFrontDist = boundedDistance(obstacleSummary.nearestOpponentFront, theFieldDimensions);
    io.nearestUncertainFrontDist = boundedDistance(obstacleSummary.nearestUncertainFront, theFieldDimensions);
    io.debugObstacleCount = obstacleSummary.count;
    io.debugObstacleTeammateCount = obstacleSummary.teammateCount;
    io.debugObstacleOpponentCount = obstacleSummary.opponentCount;
    io.debugObstacleUncertainCount = obstacleSummary.uncertainCount;
    io.debugObstacleFrontCount = obstacleSummary.frontCount;
    io.debugNearestObstacleDist = boundedDistance(obstacleSummary.nearestObstacle, theFieldDimensions);
    io.debugNearestObstacleAngle = obstacleSummary.nearestObstacleAngle;
    io.debugNearestFrontObstacleDist = boundedDistance(obstacleSummary.nearestFrontObstacle, theFieldDimensions);
    io.debugNearestOpponentAngle = obstacleSummary.nearestOpponentAngle;
    io.frame = theFrameInfo.time;
    io.obsReady = true;
    postObs = true;
    io.unlock();
    if(postObs)
      io.postObs();
  }

  theLibCheck.performCheck();
}

void SkillBehaviorControl::executeRequest()
{
  if(theSkillRequest.skill == SkillRequest::none)
  {
    if(theFrameInfo.getTimeSince(timeWhenAnnouncedEmptySkillRequest) > 5000)
    {
      ANNOTATION("Behavior", "Skill request is empty");
      SystemCall::say("Skill request is empty");
      timeWhenAnnouncedEmptySkillRequest = theFrameInfo.time;
    }
  }
  else if(theSkillRequest.skill == SkillRequest::shoot ||
          theSkillRequest.skill == SkillRequest::pass ||
          theSkillRequest.skill == SkillRequest::dribble)
  {
    thePlayBallSkill();
  }
  else
  {
    for(const Teammate& teammate : theTeamData.teammates)
    {
      if(teammate.theBehaviorStatus.passTarget == theGameState.playerNumber)
      {
        theBehaviorStatus.passOrigin = teammate.number;
        theReceivePassSkill({.playerNumber = teammate.number});
        return;
      }
    }

    switch(theSkillRequest.skill)
    {
      case SkillRequest::stand:
        theStandSkill();
        theLookActiveSkill({.withBall = true});
        break;
      case SkillRequest::walk:
      {
        const Pose2f targetPose = theRobotPose.inverse() * theSkillRequest.target;
        const bool rlTeamUsesObstacleAvoidance = RLSharedStateBridge::isEnabledForTeam(theGameState.ownTeam.number);
        const bool goalkeeperMode = theGameState.isGoalkeeper() && !rlTeamUsesObstacleAvoidance;
        if((theFieldBall.ballWasSeen(7000) || theTeammatesBallModel.isValid) && theFieldBall.isBallPositionConsistentWithGameState())
        {
          theWalkToPointObstacleSkill({.target = theRobotPose.inverse() * theSkillRequest.target,
                                       .rough = goalkeeperMode,
                                       .disableObstacleAvoidance = goalkeeperMode,
                                       .targetOfInterest = theFieldBall.recentBallPositionRelative()}); // TODO: set the right parameters and occasionally use WalkPotentialField
          if(theMotionInfo.isMotion(MotionPhase::stand))
            theLookActiveSkill({.withBall = true});
          else
            theLookAtBallAndTargetSkill({.startTarget = false,
                                         .walkingDirection = targetPose.translation,
                                         .ballPositionAngle = theFieldBall.recentBallPositionRelative().angle()});
        }
        else
        {
          theWalkToPointObstacleSkill({.target = theRobotPose.inverse() * theSkillRequest.target,
                                       .rough = goalkeeperMode,
                                       .disableObstacleAvoidance = goalkeeperMode}); // TODO: set the right parameters and occasionally use WalkPotentialField
          theLookActiveSkill({.withBall = true});
        }
        break;
      }
      case SkillRequest::block:
        theBlockSkill({.target = theRobotPose.inverse() * theSkillRequest.target.translation});
        break;
      case SkillRequest::mark:
        theMarkSkill({.target = theRobotPose.inverse() * theSkillRequest.target.translation});
        break;
      case SkillRequest::observe:
        theObservePointSkill({.target = theRobotPose.inverse() * theSkillRequest.target.translation});
        break;
      default:
        FAIL("Unknown skill request.");
    }
  }
}
