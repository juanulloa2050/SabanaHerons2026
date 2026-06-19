#pragma once

// Goalkeeper RL policy contract (mirrors RL/docs/39 and goalkeeper_action.py /
// goalkeeper_teacher.py). This is a SEPARATE network from the striker/merged PPO
// (PPOCommon.h): 64-dim observation, 12 keeper skills, its own legal-action mask.
// Deployable model: Config/NeuralNets/RLPolicy/ppo_goalkeeper_hsl2026_gk_closedloop_elite.onnx
// (head-free; dribble defense is learned, not scripted).

#include <array>
#include <cstddef>

namespace RLGK
{
  constexpr std::size_t gkObsSize = 64;     // 38 active features + zero-pad to 64
  constexpr std::size_t gkSkillCount = 12;
  constexpr std::size_t gkParamCount = 4;

  // Logit index == enum value (goalkeeper_action.GKSkillType).
  enum class GKSkillType : int
  {
    stand = 0,
    walkGoaliePose = 1,
    observe = 2,
    interceptCenter = 3,
    interceptLateral = 4,
    interceptLowLeft = 5,
    interceptLowRight = 6,
    interceptJumpLeft = 7,
    interceptJumpRight = 8,
    clear = 9,
    pass = 10,
    dribbleOut = 11,
  };

  struct GKPolicyOutput
  {
    std::array<float, gkSkillCount> skillLogits{};
    std::array<float, gkParamCount> paramMean{};
    bool valid = false;
  };

  // Raw (un-normalized) features the encoder fills before normalization. Kept so
  // the action decoder can reuse field-frame geometry without re-deriving it.
  struct GKRawObservation
  {
    // Pose / ball (field + robot frame).
    float robotX = 0.f, robotY = 0.f, robotTheta = 0.f;
    float ballX = 0.f, ballY = 0.f;             // field
    float ballRelX = 0.f, ballRelY = 0.f;       // robot frame
    float ballEndRelX = 0.f, ballEndRelY = 0.f; // robot frame
    float ballVelX = 0.f, ballVelY = 0.f;       // robot frame (BallModel.estimate.velocity)
    float timeSinceBallSeenMs = 0.f;
    float naturalTimeSinceBallSeenMs = 0.f;
    float ballSeenPercentage = 0.f;
    bool ballConsistentWithGameState = false;
    // Cognition (replicates goalkeeper_teacher.build_gk_cognition).
    float goalieX = 0.f, goalieY = 0.f, goalieTheta = 0.f;          // BOB-line target pose (field)
    float goalieErrorX = 0.f, goalieErrorY = 0.f, goalieErrorTheta = 0.f; // robot frame
    bool rollingTowardsOwnGoal = false;
    float timeToYAxisS = 0.f;     // +inf encoded as a large number; see encoder
    bool timeToYAxisFinite = false;
    float intersectionY = 0.f;    // robot-frame predicted crossing y (mm)
    bool nearLeftPost = false, nearRightPost = false, insidePenaltyArea = false;
    bool ballControlled = false, freshBall = false, staleBall = false;
    // Team ball / obstacles / body.
    float teamBallX = 0.f, teamBallY = 0.f, teamBallAgeMs = 0.f;
    bool teamBallValid = false;
    float passOptionsCount = 0.f;
    float nearestOpponentDist = 0.f, nearestOpponentFrontDist = 0.f, nearestTeammateDist = 0.f;
    bool groundContact = false;
    bool upright = true;          // stable body (FallDownState::upright)
  };

  struct GKObservation
  {
    GKRawObservation raw;
    std::array<float, gkObsSize> values{};
  };
}
