#include "PPOObservationEncoder.h"

#include <algorithm>
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
  return raw;
}

RL::PPOObservation RL::PPOObservationEncoder::encode(const PPOGateObservation& rawObservation, const PPOGateDecision& gateDecision) const
{
  PPOObservation observation;
  observation.raw = rawObservation;
  const PPOGateObservation& raw = observation.raw;

  auto& values = observation.values;
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
  const FieldDimensions& fieldDimensions,
  const PPOGateDecision& gateDecision)
{
  return encode(buildRawObservation(frameInfo, robotPose, fieldBall, ballModel, ballPercept, obstacleModel, expectedGoals, teamData, fieldDimensions), gateDecision);
}
