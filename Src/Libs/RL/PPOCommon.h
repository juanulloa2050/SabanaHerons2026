#pragma once

#include <array>
#include <cstddef>

namespace RL
{
  // obs47 (multi-agent / role-conditioned defender) contract. The first 26 entries are
  // bit-identical to the legacy obs26 striker contract; entries [26:47] add the
  // multi-agent context (teammates, team ball, engaging flags, goal vector, extended
  // gate bits, role one-hot). See RL/observation.py:ObsIndex and RL/environment.py:_encode_obs.
  constexpr std::size_t ppoObsSize = 47;
  constexpr std::size_t ppoSkillCount = 8;
  constexpr std::size_t ppoParamCount = 4;
  constexpr std::size_t ppoMaxTeammates = 7;

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

  // Role one-hot played into obs[44:46]. Exactly one role is active per inference step.
  // The validated defender ran as offBallSupport (RL/HANDOFF_BHUMAN_TRANSPLANT.md).
  enum class PPORole : int
  {
    striker = 0,      // obs[44] = is_striker
    openSupport = 1,  // obs[45] = is_open_support
    offBallSupport = 2,// obs[46] = is_off_ball_support
  };

  // Teammate activity codes, mirrors RL/environment.py TEAMMATE_ACTIVITY_* and the
  // encodeTeammateActivity helper in the rl-simrobot3d SkillBehaviorControl export.
  enum class TeammateActivity : int
  {
    idle = 0,
    goingToBall = 1,
    dribbling = 2,
    kicking = 3,
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

    // ---- obs47 multi-agent context (field coordinates; encoded into obs[26:41]) ----
    // Raw teammate field poses + freshness + activity, in arbitrary order. The encoder
    // filters fresh ones, sorts by distance to this robot and slots the nearest three,
    // exactly like RL/environment.py:_encode_obs.
    int teammateCount = 0;
    std::array<float, ppoMaxTeammates> teammateFieldX{};
    std::array<float, ppoMaxTeammates> teammateFieldY{};
    std::array<float, ppoMaxTeammates> teammateAgeMs{};
    std::array<int, ppoMaxTeammates> teammateActivity{};
    float globalTeamBallX = 0.f;
    float globalTeamBallY = 0.f;
    float globalTeamBallAgeMs = 0.f;
    bool globalTeamBallValid = false;
    float goalOpponentX = 0.f;
    float goalOpponentY = 0.f;
  };

  struct PPOGateDecision
  {
    bool shootArmed = false;
    bool dribbleArmed = false;
    bool passArmed = false;
    bool engageArmed = false;
    float shootArmProgress = 0.f;
    bool observeArmed = false;
    float passArmProgress = 0.f;

    // ---- obs47 role one-hot, encoded into obs[44:46]. Set by the caller before encode. ----
    bool isStriker = false;
    bool isOpenSupport = false;
    bool isOffBallSupport = false;

    bool finishArmed() const
    {
      return shootArmed || dribbleArmed;
    }

    bool hasRole() const
    {
      return isStriker || isOpenSupport || isOffBallSupport;
    }

    void setRole(const PPORole role)
    {
      isStriker = role == PPORole::striker;
      isOpenSupport = role == PPORole::openSupport;
      isOffBallSupport = role == PPORole::offBallSupport;
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
