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

namespace
{
  // Transform a field-frame point into robot-relative frame.
  // theta_robot is the robot heading in radians.
  void toRobotFrame(
    const float fieldX, const float fieldY,
    const float robotX, const float robotY, const float robotTheta,
    float& outRelX, float& outRelY)
  {
    const float dx = fieldX - robotX;
    const float dy = fieldY - robotY;
    const float cosA = std::cos(-robotTheta);
    const float sinA = std::sin(-robotTheta);
    outRelX = dx * cosA - dy * sinA;
    outRelY = dx * sinA + dy * cosA;
  }

  float clip47(const float v, const float lo, const float hi)
  {
    return v < lo ? lo : (v > hi ? hi : v);
  }
}

// Produce a 47-dim observation for the multi-agent (v4.2+) team model.
// Layout (matches RL/observation.py:ObsIndex exactly):
//   [0:26]  base obs (same as encode())
//   [26:32] 3 nearest-teammate positions in robot frame (x,y each, clipped ±1)
//   [32:35] team ball relative (x,y clamped ±1, time normalised)
//   [35:38] teammate engaging flags (one per slot)
//   [38]    team ball consensus with own ball estimate
//   [39:41] opponent goal vector in robot frame (x,y clamped ±1)
//   [41]    pass_armed (from teamContext.passArmed)
//   [42]    observe_armed (from teamContext.observeArmed)
//   [43]    pass_arm_progress
//   [44]    is_striker
//   [45]    is_open_support
//   [46]    is_off_ball_support
std::array<float, RL::ppoObsSize47> RL::PPOObservationEncoder::encode47(
  const PPOGateObservation& rawObservation,
  const PPOGateDecision& gateDecision,
  const PPOTeamContext& teamContext) const
{
  // Build the 26-dim base obs first.
  const PPOObservation base26 = encode(rawObservation, gateDecision);

  std::array<float, ppoObsSize47> obs{};
  for(std::size_t i = 0; i < ppoObsSize; ++i)
    obs[i] = base26.values[i];

  const float robotX = rawObservation.robotX;
  const float robotY = rawObservation.robotY;
  const float robotTheta = rawObservation.robotTheta;

  // --- dims 26–31: 3 nearest teammate positions in robot frame ---
  for(int slot = 0; slot < PPOTeamContext::maxTeammates; ++slot)
  {
    const std::size_t base = 26u + static_cast<std::size_t>(slot) * 2u;
    if(slot < teamContext.teammateCount)
    {
      const PPOTeamContext::Teammate& tm = teamContext.teammates[slot];
      float rx, ry;
      toRobotFrame(tm.fieldX, tm.fieldY, robotX, robotY, robotTheta, rx, ry);
      obs[base]     = clip47(rx / fieldXHalf, -1.f, 1.f);
      obs[base + 1] = clip47(ry / fieldYHalf, -1.f, 1.f);
    }
    else
    {
      obs[base]     = 0.f;
      obs[base + 1] = 0.f;
    }
  }

  // --- dims 32–34: team ball relative position + age ---
  {
    float tbx, tby;
    if(teamContext.teamBallValid && std::isfinite(teamContext.teamBallX) && std::isfinite(teamContext.teamBallY))
    {
      toRobotFrame(teamContext.teamBallX, teamContext.teamBallY, robotX, robotY, robotTheta, tbx, tby);
    }
    else
    {
      tbx = 0.f;
      tby = 0.f;
    }
    obs[32] = clip47(tbx / fieldXHalf, -1.f, 1.f);
    obs[33] = clip47(tby / fieldYHalf, -1.f, 1.f);
    obs[34] = normalizeTime(teamContext.teamBallAgeMs);
  }

  // --- dims 35–37: teammate engaging flags (same slot order as 26–31) ---
  for(int slot = 0; slot < PPOTeamContext::maxTeammates; ++slot)
  {
    obs[35u + static_cast<std::size_t>(slot)] =
      (slot < teamContext.teammateCount && teamContext.teammates[slot].isEngaging) ? 1.f : 0.f;
  }

  // --- dim 38: team ball consensus ---
  {
    const float ownBallFieldX = rawObservation.robotX + rawObservation.ballRelX;
    const float ownBallFieldY = rawObservation.robotY + rawObservation.ballRelY;
    const bool consensus = teamContext.teamBallValid &&
                           std::isfinite(teamContext.teamBallX) &&
                           std::isfinite(teamContext.teamBallY) &&
                           std::hypot(ownBallFieldX - teamContext.teamBallX,
                                      ownBallFieldY - teamContext.teamBallY) < PPOTeamContext::teamBallConsensusMm;
    obs[38] = consensus ? 1.f : 0.f;
  }

  // --- dims 39–40: opponent goal vector in robot frame ---
  {
    float gx, gy;
    toRobotFrame(teamContext.goalX, teamContext.goalY, robotX, robotY, robotTheta, gx, gy);
    obs[39] = clip47(gx / fieldXHalf, -1.f, 1.f);
    obs[40] = clip47(gy / fieldYHalf, -1.f, 1.f);
  }

  // --- dims 41–43: team gate bits ---
  obs[41] = teamContext.passArmed ? 1.f : 0.f;
  obs[42] = teamContext.observeArmed ? 1.f : 0.f;
  obs[43] = clamp01(teamContext.passArmProgress);

  // --- dims 44–46: role one-hot ---
  obs[44] = teamContext.isStriker ? 1.f : 0.f;
  obs[45] = teamContext.isOpenSupport ? 1.f : 0.f;
  obs[46] = teamContext.isOffBallSupport ? 1.f : 0.f;

  return obs;
}
