/**
 * @file RLSkillProvider.cpp
 *
 * Python ↔ BHuman in-memory bridge via RLSharedState.
 *
 * Frame protocol (N = GameState.playerNumber):
 *   1. Python: pybh.rl_set_action(N, skill, x, y, theta)
 *   2. Python: controller.update()
 *   3. update(SkillRequest) reads action → provides SkillRequest
 *   4. update(SkillRequest) writes ball+robot obs → shared state
 *   5. Python: pybh.rl_get_obs(N)
 */

#include "RLSkillProvider.h"
#include "Python/Controller/RLSharedState.h"

#include <limits>

MAKE_MODULE(RLSkillProvider);

namespace
{
  struct ObstacleSummary
  {
    float nearestTeammate = std::numeric_limits<float>::max();
    float nearestOpponent = std::numeric_limits<float>::max();
    float nearestUncertain = std::numeric_limits<float>::max();
    float nearestTeammateFront = std::numeric_limits<float>::max();
    float nearestOpponentFront = std::numeric_limits<float>::max();
    float nearestUncertainFront = std::numeric_limits<float>::max();
  };

  float boundedDistance(float value, const FieldDimensions& fieldDimensions)
  {
    if(value == std::numeric_limits<float>::max())
      return fieldDimensions.xPosOpponentGroundLine * 2.f;
    return value;
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
      const float distance = obstacle.center.norm();
      const bool isFront = obstacle.center.x() > 0.f;
      if(obstacle.isTeammate())
        updateNearest(distance, isFront, summary.nearestTeammate, summary.nearestTeammateFront);
      else if(obstacle.isOpponent())
        updateNearest(distance, isFront, summary.nearestOpponent, summary.nearestOpponentFront);
      else
        updateNearest(distance, isFront, summary.nearestUncertain, summary.nearestUncertainFront);
    }
    return summary;
  }

  MotionRequest buildMotionRequest(const std::string& skill,
                                   float tx, float ty, float tt,
                                   const GroundTruthWorldState& worldState,
                                   const FieldDimensions& fieldDimensions)
  {
    MotionRequest request;
    request.motion = MotionRequest::stand;
    request.walkSpeed = Pose2f(1.f, 1.f, 1.f);
    request.keepTargetRotation = false;

    const Pose2f& ownPose = worldState.ownPose;
    Vector2f ballOnField = Vector2f::Zero();
    if(!worldState.balls.empty())
      ballOnField = worldState.balls[0].position.head<2>();

    request.ballEstimate.position = ownPose.inverse() * ballOnField;
    request.ballEstimate.velocity = Vector2f::Zero();
    request.ballEstimateTimestamp = 0;
    request.ballTimeWhenLastSeen = 0;

    if(skill == "walkTo" || skill == "walk" || skill == "block" || skill == "mark" || skill == "observe")
    {
      const Pose2f targetOnField(tt, tx, ty);
      const Pose2f targetRelative = ownPose.inverse() * targetOnField;
      if(targetRelative.translation.norm() < 40.f && std::abs(static_cast<float>(targetRelative.rotation)) < 5_deg)
        return request;

      request.motion = MotionRequest::walkToPose;
      request.walkTarget = targetRelative;
      return request;
    }

    if(skill == "dribble")
    {
      request.motion = MotionRequest::dribble;
      request.targetDirection = tt;
      request.kickLength = 1000.f;
      request.alignPrecisely = KickPrecision::notPrecise;
      return request;
    }

    if(skill == "shoot" || skill == "pass")
    {
      const Vector2f goalCenterOnField(fieldDimensions.xPosOpponentGroundLine, 0.f);
      const Vector2f goalInRobot = ownPose.inverse() * goalCenterOnField;
      request.motion = MotionRequest::walkToBallAndKick;
      request.targetDirection = goalInRobot.angle();
      request.kickType = request.ballEstimate.position.y() >= 0.f ? KickInfo::forwardFastLeft : KickInfo::forwardFastRight;
      request.kickLength = skill == "pass" ? 2500.f : 6000.f;
      request.alignPrecisely = KickPrecision::notPrecise;
      return request;
    }

    return request;
  }
}

void RLSkillProvider::update(MotionRequest& motionRequest)
{
  const int n = theGameState.playerNumber > 0 ? theGameState.playerNumber : 1;
  RLPlayerIO& io = RLSharedState::instance().player(n);

  std::string skill;
  float tx;
  float ty;
  float tt;
  io.lock();
  skill = io.getSkill();
  tx = io.targetX;
  ty = io.targetY;
  tt = io.targetTheta;
  io.unlock();

  motionRequest = buildMotionRequest(skill, tx, ty, tt, theGroundTruthWorldState, theFieldDimensions);

  io.lock();
  io.debugProviderMotionRequest = static_cast<int>(motionRequest.motion);
  ++io.debugProviderCallCount;
  io.debugProviderTargetX = motionRequest.walkTarget.translation.x();
  io.debugProviderTargetY = motionRequest.walkTarget.translation.y();
  io.debugProviderTargetTheta = static_cast<float>(motionRequest.walkTarget.rotation);
  io.unlock();
}

void RLSkillProvider::update(SkillRequest& skillRequest)
{
  const int n = theGameState.playerNumber > 0 ? theGameState.playerNumber : 1;
  RLPlayerIO& io = RLSharedState::instance().player(n);

  std::string skill;
  float tx, ty, tt;
  int passTarget;
  {
    io.lock();
    skill = io.getSkill();
    tx    = io.targetX;
    ty    = io.targetY;
    tt    = io.targetTheta;
    passTarget = io.passTarget;
    io.unlock();
  }

  if(skill == "walkTo" || skill == "walk")
    skillRequest = SkillRequest::Builder::walkTo(Pose2f(tt, tx, ty));
  else if(skill == "shoot")
    skillRequest = SkillRequest::Builder::shoot();
  else if(skill == "pass")
    skillRequest = SkillRequest::Builder::passTo(passTarget);
  else if(skill == "dribble")
    skillRequest = SkillRequest::Builder::dribbleTo(tt);
  else if(skill == "block")
    skillRequest = SkillRequest::Builder::block(Vector2f(tx, ty));
  else if(skill == "mark")
    skillRequest = SkillRequest::Builder::mark(Vector2f(tx, ty));
  else if(skill == "observe")
    skillRequest = SkillRequest::Builder::observe(Vector2f(tx, ty));
  else
    skillRequest = SkillRequest::Builder::stand();

  {
    io.lock();
    const bool sim2DActive = io.sim2D.enabled && io.sim2D.initialized;
    io.unlock();
    if(sim2DActive)
      return;
  }

  float bx = 0.f;
  float by = 0.f;
  float rx = 0.f;
  float ry = 0.f;
  float rt = 0.f;
  const Vector2f ballOnField = theFieldBall.positionOnField;
  const Vector2f ballRelative = theFieldBall.positionRelative;
  const Vector2f ballEndRelative = theFieldBall.endPositionRelative;
  const Vector2f ballVelocity = theBallModel.estimate.velocity;
  const float shotQualityNoObstacles = theExpectedGoals.xG ? theExpectedGoals.xG(ballOnField) : 0.f;
  const float shotOpeningWithObstacles = theExpectedGoals.getRating ? theExpectedGoals.getRating(ballOnField) : 0.f;
  const ObstacleSummary obstacleSummary = summarizeObstacles(theObstacleModel);

  {
    io.lock();
    bx = ballOnField.x();
    by = ballOnField.y();
    const Pose2f& rp = theRobotPose;
    rx = rp.translation.x();
    ry = rp.translation.y();
    rt = static_cast<float>(rp.rotation);

    io.ballX      = bx;
    io.ballY      = by;
    io.robotX     = rx;
    io.robotY     = ry;
    io.robotTheta = rt;
    io.frame      = theFrameInfo.time;
    io.ballRelX = ballRelative.x();
    io.ballRelY = ballRelative.y();
    io.ballEndRelX = ballEndRelative.x();
    io.ballEndRelY = ballEndRelative.y();
    io.ballVelX = ballVelocity.x();
    io.ballVelY = ballVelocity.y();
    io.timeSinceBallSeen = static_cast<float>(theFieldBall.timeSinceBallWasSeen);
    io.timeSinceBallDisappeared = static_cast<float>(theFieldBall.timeSinceBallDisappeared);
    io.ballSeenPercentage = static_cast<float>(theBallModel.seenPercentage);
    io.ballConsistentWithGameState = theFieldBall.ballPositionConsistentWithGameState;
    io.canScoreNow = shotOpeningWithObstacles > 0.8f;
    io.shotQualityNoObstacles = shotQualityNoObstacles;
    io.shotOpeningWithObstacles = shotOpeningWithObstacles;
    io.passOptionsCount = static_cast<float>(theTeamData.teammates.size());
    io.nearestTeammateDist = boundedDistance(obstacleSummary.nearestTeammate, theFieldDimensions);
    io.nearestOpponentDist = boundedDistance(obstacleSummary.nearestOpponent, theFieldDimensions);
    io.nearestUncertainObstacleDist = boundedDistance(obstacleSummary.nearestUncertain, theFieldDimensions);
    io.nearestTeammateFrontDist = boundedDistance(obstacleSummary.nearestTeammateFront, theFieldDimensions);
    io.nearestOpponentFrontDist = boundedDistance(obstacleSummary.nearestOpponentFront, theFieldDimensions);
    io.nearestUncertainFrontDist = boundedDistance(obstacleSummary.nearestUncertainFront, theFieldDimensions);
    io.obsReady   = true;
    io.unlock();
  }
  io.postObs();  // wake up any Python thread waiting for this observation
}

void RLSkillProvider::update(StrategyStatus& strategyStatus)
{
  strategyStatus = StrategyStatus();
}
