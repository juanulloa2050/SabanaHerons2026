#include "PPOSkillGate.h"

#include <algorithm>
#include <cmath>

namespace
{
  constexpr float pi = 3.14159265358979323846f;
  constexpr float goalX = 4500.f;
  constexpr float trustedMaxAgeMs = 600.f;
  constexpr float dribbleGraceEnterAgeMs = 1500.f;
  constexpr float dribbleGraceHoldAgeMs = 2500.f;
  constexpr float shootEnterBallMm = 380.f;
  constexpr float shootExitBallMm = 460.f;
  constexpr float shootEnterAngleRad = 25.f * pi / 180.f;
  constexpr float shootExitAngleRad = 35.f * pi / 180.f;
  constexpr float shootGoalEnterMm = 3000.f;
  constexpr float shootGoalExitMm = 3200.f;
  constexpr float shootOpenEnter = 0.6f;
  constexpr float shootOpenExit = 0.45f;
  constexpr float shootAlignEnterRad = 30.f * pi / 180.f;
  constexpr float shootAlignExitRad = 45.f * pi / 180.f;
  constexpr int shootArmFrames = 3;
  constexpr float dribbleEnterBallMm = 450.f;
  constexpr float dribbleExitBallMm = 600.f;
  constexpr float dribbleEnterAngleRad = 35.f * pi / 180.f;
  constexpr float dribbleExitAngleRad = 55.f * pi / 180.f;
  constexpr int dribbleArmFrames = 1;

  float wrapAngle(const float angle)
  {
    return std::atan2(std::sin(angle), std::cos(angle));
  }
}

void RL::PPOSkillGate::reset()
{
  shootArmed = false;
  dribbleArmed = false;
  shootStreak = 0;
  dribbleStreak = 0;
}

RL::PPOGateDecision RL::PPOSkillGate::step(const PPOGateObservation& observation)
{
  const float dBall = std::hypot(observation.ballRelX, observation.ballRelY);
  const float aBall = std::abs(std::atan2(observation.ballRelY, observation.ballRelX));
  const float natAgeMs = observation.naturalTimeSinceBallSeenMs;
  const bool fresh = natAgeMs <= trustedMaxAgeMs;
  const bool engageEnterFresh = natAgeMs <= dribbleGraceEnterAgeMs;
  const bool engageHoldFresh = natAgeMs <= dribbleGraceHoldAgeMs;
  const float goalDist = std::hypot(goalX - observation.ballX, observation.ballY);
  const float goalBearing = std::atan2(-observation.robotY, goalX - observation.robotX);
  const float align = std::abs(wrapAngle(observation.robotTheta - goalBearing));

  const bool onBallEnter = dBall <= shootEnterBallMm && aBall <= shootEnterAngleRad;
  const bool onBallExit = dBall <= shootExitBallMm && aBall <= shootExitAngleRad;
  const bool laneEnter = observation.shotOpeningWithObstacles >= shootOpenEnter || observation.canScoreNow;
  const bool laneExit = observation.shotOpeningWithObstacles >= shootOpenExit || observation.canScoreNow;
  const bool rangeEnter = goalDist <= shootGoalEnterMm;
  const bool rangeExit = goalDist <= shootGoalExitMm;
  const bool alignEnter = align <= shootAlignEnterRad;
  const bool alignExit = align <= shootAlignExitRad;
  const bool shootEnter = fresh && onBallEnter && laneEnter && rangeEnter && alignEnter;
  const bool shootHold = fresh && onBallExit && laneExit && rangeExit && alignExit;

  if(!shootArmed)
  {
    shootStreak = shootEnter ? shootStreak + 1 : 0;
    if(shootStreak >= shootArmFrames)
      shootArmed = true;
  }
  else if(!shootHold)
  {
    shootArmed = false;
    shootStreak = 0;
  }

  const bool dribbleEnter = engageEnterFresh && dBall <= dribbleEnterBallMm && aBall <= dribbleEnterAngleRad;
  const bool dribbleHold = engageHoldFresh && dBall <= dribbleExitBallMm && aBall <= dribbleExitAngleRad;

  if(!dribbleArmed)
  {
    dribbleStreak = dribbleEnter ? dribbleStreak + 1 : 0;
    if(dribbleStreak >= dribbleArmFrames)
      dribbleArmed = true;
  }
  else if(!dribbleHold)
  {
    dribbleArmed = false;
    dribbleStreak = 0;
  }

  PPOGateDecision decision;
  decision.shootArmed = shootArmed;
  decision.dribbleArmed = dribbleArmed;
  decision.shootArmProgress = shootArmed ? 1.f : std::min(1.f, static_cast<float>(shootStreak) / static_cast<float>(shootArmFrames));
  return decision;
}
