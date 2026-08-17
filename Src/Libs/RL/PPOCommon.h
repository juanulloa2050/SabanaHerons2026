#pragma once

#include <array>
#include <cstddef>

namespace RL
{
  constexpr std::size_t ppoObsSize = 26;
  constexpr std::size_t ppoObsSize47 = 47;   // multi-agent obs (v4.2+): adds team context + role one-hot
  constexpr std::size_t ppoSkillCount = 8;
  constexpr std::size_t ppoParamCount = 4;

  enum class SkillType : int
  {
    stand = 0,
    walk = 1,
    shoot = 2,
    pass = 3,
    dribble = 4,
    block = 5,
    mark = 6,
    observe = 7,
  };

  struct PPOGateObservation
  {
    // Positions are millimetres and angles are radians. Absolute coordinates use the
    // field frame; members ending in Rel are expressed in the robot frame.
    float robotX = 0.f;
    float robotY = 0.f;
    float robotTheta = 0.f;
    float ballX = 0.f;
    float ballY = 0.f;
    float ballRelX = 0.f;
    float ballRelY = 0.f;
    float ballEndRelX = 0.f;
    float ballEndRelY = 0.f;
    float ballVelX = 0.f;
    float ballVelY = 0.f;
    float timeSinceBallSeenMs = 0.f;
    float timeSinceBallDisappearedMs = 0.f;
    float ballSeenPercentage = 0.f;
    float naturalTimeSinceBallSeenMs = 0.f;
    bool ballConsistentWithGameState = false;
    bool canScoreNow = false;
    float shotQualityNoObstacles = 0.f;
    float shotOpeningWithObstacles = 0.f;
    float passOptionsCount = 0.f;
    float nearestTeammateDist = 0.f;
    float nearestOpponentDist = 0.f;
    float nearestUncertainObstacleDist = 0.f;
    float nearestTeammateFrontDist = 0.f;
    float nearestOpponentFrontDist = 0.f;
    float nearestUncertainFrontDist = 0.f;
  };

  struct PPOGateDecision
  {
    // Gates are deliberately stateful: the policy sees both the permission bit and
    // the progress towards it, avoiding actions that flicker around a threshold.
    bool shootArmed = false;
    bool dribbleArmed = false;
    bool passArmed = false;
    bool engageArmed = false;
    float shootArmProgress = 0.f;
    float passArmProgress = 0.f;

    bool finishArmed() const
    {
      return shootArmed || dribbleArmed;
    }
  };

  struct PPOPolicyOutput
  {
    std::array<float, ppoSkillCount> skillLogits{};
    std::array<float, ppoParamCount> paramMean{};
    bool valid = false;
  };

  struct PPOObservation
  {
    PPOGateObservation raw;
    std::array<float, ppoObsSize> values{};
  };

  // Multi-agent team context for the 47-dim observation (v4.2+).
  // Built by StrategyBehaviorControl from TeamData + TeammatesBallModel each frame,
  // then passed to PPOObservationEncoder::encode47().
  struct PPOTeamContext
  {
    static constexpr int maxTeammates = 3;
    static constexpr float teammateStaleMsThreshold = 1000.f;
    static constexpr float engagingDistanceMm = 700.f;
    static constexpr float teamBallConsensusMm = 800.f;

    struct Teammate
    {
      float fieldX = 0.f;
      float fieldY = 0.f;
      float ageMs = 5000.f;  // sentinel = 5000 (TIME_NORM_MS) → normalises to 1.0
      bool isEngaging = false;
    };

    Teammate teammates[maxTeammates];
    int teammateCount = 0;

    // Team ball in field frame (NaN-guarded: use (0,0,5000ms,false) when invalid)
    float teamBallX = 0.f;
    float teamBallY = 0.f;
    float teamBallAgeMs = 5000.f;
    bool teamBallValid = false;

    // Opponent goal centre in field frame (constant 4500, 0 — stored for transform)
    float goalX = 4500.f;
    float goalY = 0.f;

    // Gate bits baked into obs[41:44]: pass_armed, observe_armed, pass_arm_progress
    bool passArmed = false;
    bool observeArmed = false;
    float passArmProgress = 0.f;

    // Role one-hot baked into obs[44:47]
    bool isStriker = false;
    bool isOpenSupport = false;
    bool isOffBallSupport = false;
  };
}
