#pragma once

#include <array>
#include <cstddef>

namespace RL
{
  constexpr std::size_t ppoObsSize = 26;
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
}
