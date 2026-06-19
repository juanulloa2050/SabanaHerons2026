#include "GKObservationEncoder.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
  // ---- GK constants (defaults from goalkeeper_teacher.py / config; RL_GK_* env in Python). ----
  constexpr float gkPi = 3.14159265358979323846f;
  constexpr float xRange = 4500.f;   // ACTION.x_range (half field x)
  constexpr float yRange = 3000.f;   // ACTION.y_range (half field y)
  constexpr float velNorm = 2500.f;  // ball velocity normalizer
  constexpr float timeNormMs = 5000.f;
  constexpr float ttiNormS = 4.0f;
  constexpr float isectNorm = 1200.f; // BALL_CATCH_MAX_WALK_DISTANCE_MM
  constexpr float fallNorm = 5.0f;

  constexpr float ownGoalX = -4500.f;          // OWN_GOAL_X_MM
  constexpr float goalieLineX = -4300.f;       // GOALIE_LINE_X_MM
  constexpr float ownGoalHalfY = 800.f;        // OWN_GOAL_HALF_Y_MM
  constexpr float goalieLineYLimit = 700.f;    // GOALIE_LINE_Y_LIMIT_MM
  constexpr float penaltyXLimit = -3900.f;     // PENALTY_X_LIMIT_MM
  constexpr float penaltyHalfY = 1100.f;       // PENALTY_HALF_Y_MM
  constexpr float controlRadius = 450.f;       // CONTROL_RADIUS_MM
  constexpr float freshBallMaxMs = 150.f;      // FRESH_BALL_MAX_MS
  constexpr float staleBallMs = 1000.f;        // STALE_BALL_MS
  constexpr float postRadius = 450.f;          // near-post radius
  constexpr float rollingSpeedMin = 80.f;

  float clampUnit(const float v) { return std::clamp(v, -1.f, 1.f); }
  float clamp01(const float v) { return std::clamp(v, 0.f, 1.f); }
  float normX(const float v) { return clampUnit(v / xRange); }
  float normY(const float v) { return clampUnit(v / yRange); }
  float normTime(const float v) { return clamp01(v / timeNormMs); }
  float wrapAngle(const float a) { return std::atan2(std::sin(a), std::cos(a)); }

  float computeNaturalTimeSinceBallSeen(const FrameInfo& frameInfo, const BallPercept& ballPercept,
                                        unsigned& lastTs, bool& hasTs)
  {
    if(ballPercept.status == BallPercept::seen)
    {
      lastTs = frameInfo.time;
      hasTs = true;
      return 0.f;
    }
    if(hasTs)
      return static_cast<float>(frameInfo.getTimeSince(lastTs));
    return timeNormMs;
  }
}

void RLGK::GKObservationEncoder::reset()
{
  lastNaturalBallSeenTimestamp = 0;
  hasNaturalBallSeenTimestamp = false;
}

RLGK::GKRawObservation RLGK::GKObservationEncoder::buildRawObservation(
  const FrameInfo& frameInfo,
  const RobotPose& robotPose,
  const FieldBall& fieldBall,
  const BallModel& ballModel,
  const BallPercept& ballPercept,
  const ObstacleModel& obstacleModel,
  const TeamData& teamData,
  const TeammatesBallModel& teammatesBallModel,
  const FallDownState& fallDownState,
  const GroundContactState& groundContactState,
  const FieldDimensions& fieldDimensions)
{
  GKRawObservation raw;
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
  raw.naturalTimeSinceBallSeenMs = computeNaturalTimeSinceBallSeen(frameInfo, ballPercept, lastNaturalBallSeenTimestamp, hasNaturalBallSeenTimestamp);
  raw.ballSeenPercentage = static_cast<float>(ballModel.seenPercentage);
  raw.ballConsistentWithGameState = fieldBall.ballPositionConsistentWithGameState;

  // ---- _goalie_pose (BOB-line fallback, replicates goalkeeper_teacher._goalie_pose). ----
  const float bx = raw.ballX;
  const float by = raw.ballY;
  const float dxGoal = std::max(1.f, bx - ownGoalX);
  const float gy = std::clamp(by * (std::abs(goalieLineX - ownGoalX) / dxGoal), -goalieLineYLimit, goalieLineYLimit);
  const float gx = goalieLineX;
  const float gtheta = std::atan2(by - gy, bx - goalieLineX);
  raw.goalieX = gx;
  raw.goalieY = gy;
  raw.goalieTheta = gtheta;
  // field-to-robot of the goalie target pose
  {
    const float ddx = gx - raw.robotX;
    const float ddy = gy - raw.robotY;
    const float c = std::cos(-raw.robotTheta);
    const float s = std::sin(-raw.robotTheta);
    raw.goalieErrorX = ddx * c - ddy * s;
    raw.goalieErrorY = ddx * s + ddy * c;
  }
  raw.goalieErrorTheta = wrapAngle(gtheta - raw.robotTheta);

  // ---- _time_to_own_y_axis (robot frame). ----
  {
    const float rbx = raw.ballRelX;
    const float rby = raw.ballRelY;
    const float vx = raw.ballVelX;
    const float vy = raw.ballVelY;
    if(vx >= -1e-3f || rbx < 0.f)
    {
      raw.timeToYAxisFinite = false;
      raw.timeToYAxisS = 0.f;
      raw.intersectionY = 0.f;
    }
    else
    {
      const float t = -rbx / vx;
      if(t < 0.f)
      {
        raw.timeToYAxisFinite = false;
        raw.timeToYAxisS = 0.f;
        raw.intersectionY = 0.f;
      }
      else
      {
        raw.timeToYAxisFinite = true;
        raw.timeToYAxisS = t;
        raw.intersectionY = rby + vy * t;
      }
    }
  }

  const float ballSpeed = std::hypot(raw.ballVelX, raw.ballVelY);
  raw.rollingTowardsOwnGoal = ballSpeed > rollingSpeedMin && raw.ballVelX < 0.f &&
                              std::abs(raw.intersectionY) <= ownGoalHalfY + 250.f && raw.timeToYAxisFinite;

  raw.nearLeftPost = std::hypot(raw.robotX - ownGoalX, raw.robotY - ownGoalHalfY) < postRadius;
  raw.nearRightPost = std::hypot(raw.robotX - ownGoalX, raw.robotY + ownGoalHalfY) < postRadius;
  raw.insidePenaltyArea = raw.robotX < penaltyXLimit && std::abs(raw.robotY) < penaltyHalfY;
  raw.ballControlled = std::hypot(raw.ballRelX, raw.ballRelY) < controlRadius;
  raw.freshBall = raw.naturalTimeSinceBallSeenMs <= freshBallMaxMs;
  raw.staleBall = raw.naturalTimeSinceBallSeenMs > staleBallMs;

  raw.teamBallX = teammatesBallModel.position.x();
  raw.teamBallY = teammatesBallModel.position.y();
  raw.teamBallValid = teammatesBallModel.isValid;
  raw.teamBallAgeMs = teammatesBallModel.isValid ? static_cast<float>(frameInfo.getTimeSince(teammatesBallModel.timeWhenLastSeen)) : timeNormMs;
  raw.passOptionsCount = static_cast<float>(teamData.teammates.size());

  // ---- nearest opponent / teammate (robot frame, ObstacleModel). ----
  float nearestOpp = std::numeric_limits<float>::max();
  float nearestOppFront = std::numeric_limits<float>::max();
  float nearestMate = std::numeric_limits<float>::max();
  for(const Obstacle& o : obstacleModel.obstacles)
  {
    const float d = o.center.norm();
    if(o.isOpponent())
    {
      nearestOpp = std::min(nearestOpp, d);
      if(o.center.x() > 0.f)
        nearestOppFront = std::min(nearestOppFront, d);
    }
    else if(o.isTeammate())
      nearestMate = std::min(nearestMate, d);
  }
  const float farSentinel = 2.f * xRange;
  raw.nearestOpponentDist = nearestOpp == std::numeric_limits<float>::max() ? farSentinel : nearestOpp;
  raw.nearestOpponentFrontDist = nearestOppFront == std::numeric_limits<float>::max() ? farSentinel : nearestOppFront;
  raw.nearestTeammateDist = nearestMate == std::numeric_limits<float>::max() ? farSentinel : nearestMate;

  raw.groundContact = groundContactState.contact;
  raw.upright = fallDownState.state == FallDownState::upright;

  (void)fieldDimensions; // GK uses fixed goal geometry constants (matches training); kept for signature parity.
  return raw;
}

RLGK::GKObservation RLGK::GKObservationEncoder::encode(const GKRawObservation& raw) const
{
  GKObservation obs;
  obs.raw = raw;
  auto& v = obs.values;
  v.fill(0.f); // indices 38..63 are reserved zeros

  v[0] = normX(raw.robotX);
  v[1] = normY(raw.robotY);
  v[2] = clampUnit(raw.robotTheta / gkPi);
  v[3] = normX(raw.ballRelX);
  v[4] = normY(raw.ballRelY);
  v[5] = normX(raw.ballEndRelX);
  v[6] = normY(raw.ballEndRelY);
  v[7] = clampUnit(raw.ballVelX / velNorm);
  v[8] = clampUnit(raw.ballVelY / velNorm);
  v[9] = normTime(raw.timeSinceBallSeenMs);
  v[10] = normTime(raw.naturalTimeSinceBallSeenMs);
  v[11] = clamp01(raw.ballSeenPercentage / 100.f);
  v[12] = raw.ballConsistentWithGameState ? 1.f : 0.f;
  v[13] = normX(raw.goalieErrorX);
  v[14] = normY(raw.goalieErrorY);
  v[15] = clampUnit(raw.goalieErrorTheta / gkPi);
  v[16] = normX(raw.goalieX);
  v[17] = normY(raw.goalieY);
  v[18] = clampUnit(raw.goalieTheta / gkPi);
  v[19] = raw.rollingTowardsOwnGoal ? 1.f : 0.f;
  v[20] = raw.timeToYAxisFinite ? clamp01(raw.timeToYAxisS / ttiNormS) : 1.f;
  v[21] = clampUnit(raw.intersectionY / isectNorm);
  v[22] = raw.nearLeftPost ? 1.f : 0.f;
  v[23] = raw.nearRightPost ? 1.f : 0.f;
  v[24] = raw.insidePenaltyArea ? 1.f : 0.f;
  v[25] = raw.ballControlled ? 1.f : 0.f;
  v[26] = clamp01(raw.passOptionsCount / 4.f);
  v[27] = raw.freshBall ? 1.f : 0.f;
  v[28] = raw.staleBall ? 1.f : 0.f;
  v[29] = normX(raw.teamBallX - raw.robotX);
  v[30] = normY(raw.teamBallY - raw.robotY);
  v[31] = normTime(raw.teamBallAgeMs);
  v[32] = raw.teamBallValid ? 1.f : 0.f;
  v[33] = clamp01(raw.nearestOpponentDist / xRange);
  v[34] = clamp01(raw.nearestOpponentFrontDist / xRange);
  v[35] = clamp01(raw.nearestTeammateDist / xRange);
  v[36] = raw.groundContact ? 1.f : 0.f;
  // Stability gate in the mask is obs[37] <= 0.4. upright -> 0.0 (stable);
  // any non-upright -> 1.0 (unstable) — the conservative, correct semantic.
  v[37] = raw.upright ? 0.f : 1.f;
  return obs;
}

RLGK::GKObservation RLGK::GKObservationEncoder::encode(
  const FrameInfo& frameInfo,
  const RobotPose& robotPose,
  const FieldBall& fieldBall,
  const BallModel& ballModel,
  const BallPercept& ballPercept,
  const ObstacleModel& obstacleModel,
  const TeamData& teamData,
  const TeammatesBallModel& teammatesBallModel,
  const FallDownState& fallDownState,
  const GroundContactState& groundContactState,
  const FieldDimensions& fieldDimensions)
{
  return encode(buildRawObservation(frameInfo, robotPose, fieldBall, ballModel, ballPercept,
                                    obstacleModel, teamData, teammatesBallModel, fallDownState,
                                    groundContactState, fieldDimensions));
}
