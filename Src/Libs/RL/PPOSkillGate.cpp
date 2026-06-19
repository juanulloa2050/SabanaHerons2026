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
  constexpr float passEnterAgeMs = 1000.f;
  constexpr float passHoldAgeMs = 1600.f;
  constexpr float passEnterBallMm = 720.f;
  constexpr float passHoldBallMm = 900.f;
  constexpr float passEnterAngleRad = 70.f * pi / 180.f;
  constexpr float passHoldAngleRad = 85.f * pi / 180.f;
  constexpr float passMinBallX = -3600.f;
  constexpr float passOpponentEnterMm = 850.f;
  constexpr float passOpponentHoldMm = 650.f;
  constexpr float passFrontOpponentEnterMm = 1050.f;
  constexpr float passFrontOpponentHoldMm = 800.f;
  constexpr int passArmFrames = 1;
  constexpr float engageEnterAgeMs = 1000.f;
  constexpr float engageHoldAgeMs = 1800.f;
  constexpr float engageEnterBallX = -800.f;
  constexpr float engageHoldBallX = -500.f;
  constexpr int engageArmFrames = 2;

  float wrapAngle(const float angle)
  {
    return std::atan2(std::sin(angle), std::cos(angle));
  }
}

void RL::PPOSkillGate::reset()
{
  shootArmed = false;
  dribbleArmed = false;
  passArmed = false;
  engageArmed = false;
  shootStreak = 0;
  dribbleStreak = 0;
  passStreak = 0;
  engageStreak = 0;
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

RL::PPOGateDecision RL::PPOSkillGate::stepDefender(const PPOGateObservation& observation, const bool hasPassTarget, const bool engageAllowed)
{
  const float dBall = std::hypot(observation.ballRelX, observation.ballRelY);
  const float aBall = std::abs(std::atan2(observation.ballRelY, observation.ballRelX));
  const float ageMs = observation.timeSinceBallSeenMs;
  const bool opponentClearEnter = (observation.nearestOpponentDist <= 0.f || observation.nearestOpponentDist >= passOpponentEnterMm) &&
                                  (observation.nearestOpponentFrontDist <= 0.f || observation.nearestOpponentFrontDist >= passFrontOpponentEnterMm);
  const bool opponentClearHold = (observation.nearestOpponentDist <= 0.f || observation.nearestOpponentDist >= passOpponentHoldMm) &&
                                 (observation.nearestOpponentFrontDist <= 0.f || observation.nearestOpponentFrontDist >= passFrontOpponentHoldMm);
  const bool ballPlayableEnter = ageMs <= passEnterAgeMs && dBall <= passEnterBallMm && aBall <= passEnterAngleRad;
  const bool ballPlayableHold = ageMs <= passHoldAgeMs && dBall <= passHoldBallMm && aBall <= passHoldAngleRad;
  const bool forwardEnough = observation.ballX >= passMinBallX;
  const bool enter = hasPassTarget && ballPlayableEnter && opponentClearEnter && forwardEnough;
  const bool hold = hasPassTarget && ballPlayableHold && opponentClearHold && forwardEnough;

  if(!passArmed)
  {
    passStreak = enter ? passStreak + 1 : 0;
    if(passStreak >= passArmFrames)
      passArmed = true;
  }
  else if(!hold)
  {
    passArmed = false;
    passStreak = 0;
  }

  const bool engageEnter = engageAllowed && ageMs <= engageEnterAgeMs && observation.ballX <= engageEnterBallX;
  const bool engageHold = engageAllowed && ageMs <= engageHoldAgeMs && observation.ballX <= engageHoldBallX;

  if(!engageArmed)
  {
    engageStreak = engageEnter ? engageStreak + 1 : 0;
    if(engageStreak >= engageArmFrames)
      engageArmed = true;
  }
  else if(!engageHold)
  {
    engageArmed = false;
    engageStreak = 0;
  }

  PPOGateDecision decision;
  decision.passArmed = passArmed;
  decision.engageArmed = engageArmed;
  decision.passArmProgress = passArmed ? 1.f : std::min(1.f, static_cast<float>(passStreak) / static_cast<float>(passArmFrames));
  return decision;
}
