#include "GKActionDecoder.h"

#include "Representations/MotionControl/MotionRequest.h"
#include "Tools/BehaviorControl/Interception.h"

#include <algorithm>
#include <cmath>

namespace
{
  constexpr float fieldXHalf = 4500.f;
  constexpr float fieldYHalf = 3000.f;
  constexpr float gkPi = 3.14159265358979323846f;

  float clampUnit(const float v) { return std::clamp(v, -1.f, 1.f); }
  float clampFieldX(const float x) { return std::clamp(x, -fieldXHalf, fieldXHalf); }
  float clampFieldY(const float y) { return std::clamp(y, -fieldYHalf, fieldYHalf); }

  // Active params per skill (goalkeeper_action.GK_ACTION_PARAM_MASKS).
  std::array<float, RLGK::gkParamCount> paramMaskForSkill(const RLGK::GKSkillType s)
  {
    using S = RLGK::GKSkillType;
    switch(s)
    {
      case S::walkGoaliePose: return {1.f, 1.f, 1.f, 0.f};
      case S::observe:        return {1.f, 1.f, 0.f, 0.f};
      case S::interceptCenter:
      case S::interceptLateral:
      case S::interceptLowLeft:
      case S::interceptLowRight:
      case S::interceptJumpLeft:
      case S::interceptJumpRight: return {0.f, 1.f, 0.f, 0.f};
      case S::dribbleOut:     return {0.f, 1.f, 1.f, 0.f};
      default:                return {0.f, 0.f, 0.f, 0.f}; // stand / clear / pass
    }
  }

  unsigned methodBit(const Interception::Method m)
  {
    return 1u << static_cast<unsigned>(m);
  }
}

SkillRequest RLGK::GKActionDecoder::decode(const GKRawObservation& raw, int skillIndex,
                                           const std::array<float, gkParamCount>& rawParams, int passTarget) const
{
  using S = GKSkillType;
  S skill = S::stand;
  if(skillIndex >= static_cast<int>(S::stand) && skillIndex <= static_cast<int>(S::dribbleOut))
    skill = static_cast<S>(skillIndex);

  const auto mask = paramMaskForSkill(skill);
  const float targetX = clampFieldX(clampUnit(rawParams[0]) * mask[0] * fieldXHalf);
  const float targetY = clampFieldY(clampUnit(rawParams[1]) * mask[1] * fieldYHalf);
  const float targetTheta = clampUnit(rawParams[2]) * mask[2] * gkPi;

  switch(skill)
  {
    case S::stand:
      return SkillRequest::Builder::stand();

    case S::walkGoaliePose:
      // Reposition on/near the goal line at the policy's target field pose.
      return SkillRequest::Builder::walkTo(Pose2f(targetTheta, targetX, targetY));

    case S::observe:
      // Search / look toward the policy's target point.
      return SkillRequest::Builder::observe(Vector2f(targetX, targetY));

    case S::interceptCenter:
      // Block centrally: stand/step in the line, no dive.
      return SkillRequest::Builder::interceptBall(methodBit(Interception::stand) | methodBit(Interception::walk), false);

    case S::interceptLateral:
      // Small lateral step to block, no dive.
      return SkillRequest::Builder::interceptBall(methodBit(Interception::walk), false);

    case S::interceptLowLeft:
      // Genuflect / spread low to the left.
      return SkillRequest::Builder::keeperDive(MotionRequest::Dive::squatArmsBackLeft);
    case S::interceptLowRight:
      return SkillRequest::Builder::keeperDive(MotionRequest::Dive::squatArmsBackRight);

    case S::interceptJumpLeft:
      // Full dive to the left.
      return SkillRequest::Builder::keeperDive(MotionRequest::Dive::jumpLeft);
    case S::interceptJumpRight:
      return SkillRequest::Builder::keeperDive(MotionRequest::Dive::jumpRight);

    case S::clear:
      // Kick the controlled ball clear upfield.
      return SkillRequest::Builder::shoot();

    case S::pass:
      // Runtime-chosen teammate; fall back to a clear if none available.
      if(passTarget >= 1)
        return SkillRequest::Builder::passTo(passTarget);
      return SkillRequest::Builder::shoot();

    case S::dribbleOut:
    {
      // Dribble the controlled ball out toward the target point (heading from ball).
      const float heading = std::atan2(targetY - raw.ballY, targetX - raw.ballX);
      return SkillRequest::Builder::dribbleTo(Angle(heading));
    }

    default:
      return SkillRequest::Builder::stand();
  }
}
