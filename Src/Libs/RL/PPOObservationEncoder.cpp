#include "PPOObservationEncoder.h"

#include "Tools/BehaviorControl/Strategy/ActiveRole.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
  constexpr float ppoPi = 3.14159265358979323846f;
  constexpr float fieldXHalf = 4500.f;
  constexpr float fieldYHalf = 3000.f;
  constexpr float ballVelNorm = 3000.f;
  constexpr float timeNormMs = 5000.f;
  constexpr float passCountNorm = 5.f;

  // obs47 multi-agent constants — mirror RL/environment.py.
  constexpr float teammateStaleThresholdMs = 1000.f; // TEAMMATE_STALE_THRESHOLD_MS
  constexpr float teamBallConsensusMm = 800.f;       // TEAM_BALL_CONSENSUS_MM
  constexpr float engagingDistanceMm = 700.f;        // ENGAGING_DISTANCE_MM

  // Teammate activity thresholds — mirror encodeTeammateActivity in the rl-simrobot3d
  // SkillBehaviorControl RL observation export.
  constexpr int teammateBallFreshThresholdMs = 1000;
  constexpr float teammateWalkingSpeedThreshold = 80.f;
  constexpr float teammateWalkTargetNearBallMm = 450.f;
  constexpr float teammateBallControlDistanceMm = 260.f;
  constexpr float teammateBallEngageDistanceMm = 900.f;

  struct ObstacleSummary
  {
    float nearestTeammate = std::numeric_limits<float>::max();
    float nearestOpponent = std::numeric_limits<float>::max();
    float nearestUncertain = std::numeric_limits<float>::max();
    float nearestTeammateFront = std::numeric_limits<float>::max();
    float nearestOpponentFront = std::numeric_limits<float>::max();
    float nearestUncertainFront = std::numeric_limits<float>::max();
  };

  float clamp01(const float value)
  {
    return std::clamp(value, 0.f, 1.f);
  }

  float normalizeSigned(const float value, const float scale)
  {
    if(scale <= 0.f)
      return 0.f;
    return std::clamp(value / scale, -1.f, 1.f);
  }

  float normalizeDistance(const float value)
  {
    return clamp01(value / fieldXHalf);
  }

  float normalizeTime(const float value)
  {
    return clamp01(value / timeNormMs);
  }

  void updateNearest(const float distance, const bool isFront, float& nearest, float& nearestFront)
  {
    nearest = std::min(nearest, distance);
    if(isFront)
      nearestFront = std::min(nearestFront, distance);
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

  float boundedDistance(float value, const FieldDimensions& fieldDimensions)
  {
    if(value == std::numeric_limits<float>::max())
      return fieldDimensions.xPosOpponentGroundLine * 2.f;
    return value;
  }

  // Classify a teammate's current intent toward the ball — port of encodeTeammateActivity
  // from the rl-simrobot3d SkillBehaviorControl RL export.
  int encodeTeammateActivity(const Teammate& teammate)
  {
    if(teammate.theBehaviorStatus.shootingTo.has_value() || teammate.theBehaviorStatus.passTarget >= 0)
      return static_cast<int>(RL::TeammateActivity::kicking);

    const bool playBallRole = teammate.theStrategyStatus.role == ActiveRole::toRole(ActiveRole::playBall);
    const bool hasFreshBall = teammate.theBallModel.timeWhenLastSeen > 0 &&
                              teammate.theFrameInfo.getTimeSince(teammate.theBallModel.timeWhenLastSeen) <= teammateBallFreshThresholdMs;
    const float ballDistance = teammate.theBallModel.estimate.position.norm();
    const float walkTargetDistance = teammate.theBehaviorStatus.walkingTo.norm();
    const float walkTargetToBallDistance = hasFreshBall
                                             ? (teammate.theBehaviorStatus.walkingTo - teammate.theBallModel.estimate.position).norm()
                                             : std::numeric_limits<float>::max();
    const bool isWalking = teammate.theBehaviorStatus.speed >= teammateWalkingSpeedThreshold || walkTargetDistance > 10.f;

    if(hasFreshBall && ballDistance <= teammateBallControlDistanceMm)
      return static_cast<int>(RL::TeammateActivity::dribbling);

    if(playBallRole ||
       (hasFreshBall && ballDistance <= teammateBallEngageDistanceMm) ||
       (hasFreshBall && isWalking && walkTargetToBallDistance <= teammateWalkTargetNearBallMm))
      return static_cast<int>(RL::TeammateActivity::goingToBall);

    return static_cast<int>(RL::TeammateActivity::idle);
  }

  // Port of RL/environment.py:_teammate_activity_is_engaging.
  bool teammateActivityIsEngaging(
    const int activity,
    const float teammateX,
    const float teammateY,
    const float teamBallX,
    const float teamBallY,
    const bool teamBallValid)
  {
    if(activity == static_cast<int>(RL::TeammateActivity::dribbling) ||
       activity == static_cast<int>(RL::TeammateActivity::kicking))
      return true;
    if(activity != static_cast<int>(RL::TeammateActivity::goingToBall) || !teamBallValid)
      return false;
    return std::hypot(teammateX - teamBallX, teammateY - teamBallY) < engagingDistanceMm;
  }

  float computeNaturalTimeSinceBallSeen(
    const FrameInfo& frameInfo,
    const BallPercept& ballPercept,
    unsigned& lastNaturalBallSeenTimestamp,
    bool& hasNaturalBallSeenTimestamp)
  {
    if(ballPercept.status == BallPercept::seen)
    {
      lastNaturalBallSeenTimestamp = frameInfo.time;
      hasNaturalBallSeenTimestamp = true;
      return 0.f;
    }

    if(hasNaturalBallSeenTimestamp)
      return static_cast<float>(frameInfo.getTimeSince(lastNaturalBallSeenTimestamp));

    return timeNormMs;
  }
}

void RL::PPOObservationEncoder::reset()
{
  lastNaturalBallSeenTimestamp = 0;
  hasNaturalBallSeenTimestamp = false;
}

RL::PPOGateObservation RL::PPOObservationEncoder::buildRawObservation(
  const FrameInfo& frameInfo,
  const RobotPose& robotPose,
  const FieldBall& fieldBall,
  const BallModel& ballModel,
  const BallPercept& ballPercept,
  const ObstacleModel& obstacleModel,
  const ExpectedGoals& expectedGoals,
  const TeamData& teamData,
  const TeammatesBallModel& teammatesBallModel,
  const FieldDimensions& fieldDimensions)
{
  PPOGateObservation raw;
  raw.robotX = robotPose.translation.x();
  raw.robotY = robotPose.translation.y();
  raw.robotTheta = static_cast<float>(robotPose.rotation);
  raw.ballX = fieldBall.positionOnField.x();
  raw.ballY = fieldBall.positionOnField.y();
  raw.ballRelX = fieldBall.positionRelative.x();
  raw.ballRelY = fieldBall.positionRelative.y();
  raw.ballEndRelX = fieldBall.endPositionRelative.x();
  raw.ballEndRelY = fieldBall.endPositionRelative.y();
  raw.ballVelX = ballModel.estimate.velocity.x();
  raw.ballVelY = ballModel.estimate.velocity.y();
  raw.timeSinceBallSeenMs = static_cast<float>(fieldBall.timeSinceBallWasSeen);
  raw.timeSinceBallDisappearedMs = static_cast<float>(fieldBall.timeSinceBallDisappeared);
  raw.ballSeenPercentage = static_cast<float>(ballModel.seenPercentage);
  raw.naturalTimeSinceBallSeenMs = computeNaturalTimeSinceBallSeen(frameInfo, ballPercept, lastNaturalBallSeenTimestamp, hasNaturalBallSeenTimestamp);
  raw.ballConsistentWithGameState = fieldBall.ballPositionConsistentWithGameState;

  const Vector2f ballOnField(raw.ballX, raw.ballY);
  raw.shotQualityNoObstacles = expectedGoals.xG ? expectedGoals.xG(ballOnField) : 0.f;
  raw.shotOpeningWithObstacles = expectedGoals.getRating ? expectedGoals.getRating(ballOnField) : 0.f;
  raw.canScoreNow = raw.shotOpeningWithObstacles > 0.8f;
  raw.passOptionsCount = static_cast<float>(teamData.teammates.size());

  const ObstacleSummary obstacleSummary = summarizeObstacles(obstacleModel);
  raw.nearestTeammateDist = boundedDistance(obstacleSummary.nearestTeammate, fieldDimensions);
  raw.nearestOpponentDist = boundedDistance(obstacleSummary.nearestOpponent, fieldDimensions);
  raw.nearestUncertainObstacleDist = boundedDistance(obstacleSummary.nearestUncertain, fieldDimensions);
  raw.nearestTeammateFrontDist = boundedDistance(obstacleSummary.nearestTeammateFront, fieldDimensions);
  raw.nearestOpponentFrontDist = boundedDistance(obstacleSummary.nearestOpponentFront, fieldDimensions);
  raw.nearestUncertainFrontDist = boundedDistance(obstacleSummary.nearestUncertainFront, fieldDimensions);

  // ---- obs47 multi-agent context (field coordinates) ----
  raw.teammateCount = 0;
  unsigned newestTeammateBallTime = 0;
  for(const Teammate& teammate : teamData.teammates)
  {
    if(raw.teammateCount >= static_cast<int>(RL::ppoMaxTeammates))
      break;
    const std::size_t idx = static_cast<std::size_t>(raw.teammateCount++);
    raw.teammateFieldX[idx] = teammate.theRobotPose.translation.x();
    raw.teammateFieldY[idx] = teammate.theRobotPose.translation.y();
    raw.teammateAgeMs[idx] = static_cast<float>(frameInfo.getTimeSince(teammate.theFrameInfo.time));
    raw.teammateActivity[idx] = encodeTeammateActivity(teammate);
    if(teammate.theBallModel.timeWhenLastSeen > newestTeammateBallTime)
      newestTeammateBallTime = teammate.theBallModel.timeWhenLastSeen;
  }

  raw.globalTeamBallX = teammatesBallModel.position.x();
  raw.globalTeamBallY = teammatesBallModel.position.y();
  raw.globalTeamBallValid = teammatesBallModel.isValid;
  raw.globalTeamBallAgeMs = (teammatesBallModel.isValid && newestTeammateBallTime > 0)
                              ? static_cast<float>(frameInfo.getTimeSince(newestTeammateBallTime))
                              : 0.f;
  raw.goalOpponentX = fieldDimensions.xPosOpponentGroundLine;
  raw.goalOpponentY = 0.5f * (fieldDimensions.yPosLeftGoal + fieldDimensions.yPosRightGoal);
  return raw;
}

RL::PPOObservation RL::PPOObservationEncoder::encode(const PPOGateObservation& rawObservation, const PPOGateDecision& gateDecision) const
{
  PPOObservation observation;
  observation.raw = rawObservation;
  const PPOGateObservation& raw = observation.raw;

  // Transform a field point into this robot's frame (rotation by -robotTheta). Mirrors
  // transform_to_robot_frame() inside RL/environment.py:_encode_obs.
  const float cosNegTheta = std::cos(-raw.robotTheta);
  const float sinNegTheta = std::sin(-raw.robotTheta);
  const auto toRobotFrame = [&](const float fieldX, const float fieldY, float& relX, float& relY)
  {
    const float dx = fieldX - raw.robotX;
    const float dy = fieldY - raw.robotY;
    relX = dx * cosNegTheta - dy * sinNegTheta;
    relY = dx * sinNegTheta + dy * cosNegTheta;
  };

  auto& values = observation.values;
  // ---- base features [0:26] (bit-identical to the legacy obs26 contract) ----
  values[0] = normalizeSigned(raw.robotX, fieldXHalf);
  values[1] = normalizeSigned(raw.robotY, fieldYHalf);
  values[2] = std::clamp(raw.robotTheta / ppoPi, -1.f, 1.f);
  values[3] = normalizeSigned(raw.ballRelX, fieldXHalf);
  values[4] = normalizeSigned(raw.ballRelY, fieldYHalf);
  values[5] = normalizeSigned(raw.ballEndRelX, fieldXHalf);
  values[6] = normalizeSigned(raw.ballEndRelY, fieldYHalf);
  values[7] = std::clamp(raw.ballVelX / ballVelNorm, -1.f, 1.f);
  values[8] = std::clamp(raw.ballVelY / ballVelNorm, -1.f, 1.f);
  values[9] = normalizeTime(raw.timeSinceBallSeenMs);
  values[10] = normalizeTime(raw.timeSinceBallDisappearedMs);
  values[11] = clamp01(raw.ballSeenPercentage / 100.f);
  values[12] = raw.ballConsistentWithGameState ? 1.f : 0.f;
  values[13] = raw.canScoreNow ? 1.f : 0.f;
  values[14] = clamp01(raw.shotQualityNoObstacles);
  values[15] = clamp01(raw.shotOpeningWithObstacles);
  values[16] = clamp01(raw.passOptionsCount / passCountNorm);
  values[17] = normalizeDistance(raw.nearestTeammateDist);
  values[18] = normalizeDistance(raw.nearestOpponentDist);
  values[19] = normalizeDistance(raw.nearestUncertainObstacleDist);
  values[20] = normalizeDistance(raw.nearestTeammateFrontDist);
  values[21] = normalizeDistance(raw.nearestOpponentFrontDist);
  values[22] = normalizeDistance(raw.nearestUncertainFrontDist);
  values[23] = gateDecision.shootArmed ? 1.f : 0.f;
  values[24] = gateDecision.dribbleArmed ? 1.f : 0.f;
  values[25] = clamp01(gateDecision.shootArmProgress);

  // ---- teammate context [26:32] + engaging flags [35:38] ----
  // Keep only fresh teammates, sort by distance to this robot, slot the nearest three
  // (RL/environment.py:_encode_obs).
  struct FreshTeammate
  {
    float x;
    float y;
    int activity;
    float distSq;
  };
  std::array<FreshTeammate, RL::ppoMaxTeammates> fresh{};
  int freshCount = 0;
  for(int i = 0; i < raw.teammateCount; ++i)
  {
    if(raw.teammateAgeMs[static_cast<std::size_t>(i)] > teammateStaleThresholdMs)
      continue;
    const float px = raw.teammateFieldX[static_cast<std::size_t>(i)];
    const float py = raw.teammateFieldY[static_cast<std::size_t>(i)];
    const float dx = px - raw.robotX;
    const float dy = py - raw.robotY;
    fresh[static_cast<std::size_t>(freshCount++)] = {px, py, raw.teammateActivity[static_cast<std::size_t>(i)], dx * dx + dy * dy};
  }
  std::stable_sort(fresh.begin(), fresh.begin() + freshCount,
                   [](const FreshTeammate& a, const FreshTeammate& b) { return a.distSq < b.distSq; });

  for(int slot = 0; slot < 3; ++slot)
  {
    const std::size_t base = 26 + static_cast<std::size_t>(slot) * 2;
    const std::size_t engageIdx = 35 + static_cast<std::size_t>(slot);
    if(slot >= freshCount)
    {
      values[base] = 0.f;
      values[base + 1] = 0.f;
      values[engageIdx] = 0.f;
      continue;
    }
    const FreshTeammate& t = fresh[static_cast<std::size_t>(slot)];
    float rx, ry;
    toRobotFrame(t.x, t.y, rx, ry);
    values[base] = normalizeSigned(rx, fieldXHalf);
    values[base + 1] = normalizeSigned(ry, fieldYHalf);
    values[engageIdx] = teammateActivityIsEngaging(t.activity, t.x, t.y, raw.globalTeamBallX, raw.globalTeamBallY, raw.globalTeamBallValid) ? 1.f : 0.f;
  }

  // ---- team ball [32:35] ----
  float tbx, tby;
  toRobotFrame(raw.globalTeamBallX, raw.globalTeamBallY, tbx, tby);
  values[32] = normalizeSigned(tbx, fieldXHalf);
  values[33] = normalizeSigned(tby, fieldYHalf);
  values[34] = normalizeTime(raw.globalTeamBallAgeMs);

  // ---- consensus [38]: does the team ball agree with my own ball? ----
  const float ownBallFieldX = raw.robotX + raw.ballRelX;
  const float ownBallFieldY = raw.robotY + raw.ballRelY;
  const float distanceToTeamBall = std::hypot(ownBallFieldX - raw.globalTeamBallX, ownBallFieldY - raw.globalTeamBallY);
  values[38] = (raw.globalTeamBallValid && distanceToTeamBall < teamBallConsensusMm) ? 1.f : 0.f;

  // ---- opponent goal vector [39:41] ----
  float gx, gy;
  toRobotFrame(raw.goalOpponentX, raw.goalOpponentY, gx, gy);
  values[39] = normalizeSigned(gx, fieldXHalf);
  values[40] = normalizeSigned(gy, fieldYHalf);

  // ---- extended gate bits [41:43] ----
  values[41] = gateDecision.passArmed ? 1.f : 0.f;
  values[42] = gateDecision.observeArmed ? 1.f : 0.f;
  values[43] = clamp01(gateDecision.passArmProgress);

  // ---- role one-hot [44:46] ----
  values[44] = gateDecision.isStriker ? 1.f : 0.f;
  values[45] = gateDecision.isOpenSupport ? 1.f : 0.f;
  values[46] = gateDecision.isOffBallSupport ? 1.f : 0.f;
  return observation;
}

RL::PPOObservation RL::PPOObservationEncoder::encodeDefender(const PPOGateObservation& rawObservation, const PPOGateDecision& gateDecision) const
{
  PPOObservation observation = encode(rawObservation, PPOGateDecision{});
  observation.values[23] = gateDecision.passArmed ? 1.f : 0.f;
  observation.values[24] = gateDecision.engageArmed ? 1.f : 0.f;
  observation.values[25] = clamp01(gateDecision.passArmProgress);
  return observation;
}

RL::PPOObservation RL::PPOObservationEncoder::encode(
  const FrameInfo& frameInfo,
  const RobotPose& robotPose,
  const FieldBall& fieldBall,
  const BallModel& ballModel,
  const BallPercept& ballPercept,
  const ObstacleModel& obstacleModel,
  const ExpectedGoals& expectedGoals,
  const TeamData& teamData,
  const TeammatesBallModel& teammatesBallModel,
  const FieldDimensions& fieldDimensions,
  const PPOGateDecision& gateDecision)
{
  return encode(buildRawObservation(frameInfo, robotPose, fieldBall, ballModel, ballPercept, obstacleModel, expectedGoals, teamData, teammatesBallModel, fieldDimensions), gateDecision);
}
