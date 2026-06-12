#include "PPOActionDecoder.h"

#include <algorithm>
#include <cmath>

namespace
{
  constexpr float goalX = 4500.f;
  constexpr float fieldXHalf = 4500.f;
  constexpr float fieldYHalf = 3000.f;
  constexpr float residualXRange = 0.f;
  constexpr float residualYRange = 450.f;
  constexpr float residualThetaRange = 0.75f;
  constexpr float approachOffsetX = -210.f;
  constexpr float ownGoalX = -fieldXHalf;
  constexpr float defenderMinX = -3950.f;
  constexpr float defenderMaxX = -900.f;
  constexpr float defenderMaxAbsY = 2200.f;
  constexpr float defenderMinWalkStep = 550.f;
  const float repairTargetRadiusLimit = 1.05f * std::hypot(residualXRange, residualYRange);
  constexpr float repairThetaLimit = 1.05f * residualThetaRange;

  float wrapAngle(const float angle)
  {
    return std::atan2(std::sin(angle), std::cos(angle));
  }

  float clampFieldX(const float x)
  {
    return std::clamp(x, -fieldXHalf, fieldXHalf);
  }

  float clampFieldY(const float y)
  {
    return std::clamp(y, -fieldYHalf, fieldYHalf);
  }

  std::array<float, RL::ppoParamCount> paramMaskForSkill(const RL::SkillType skill)
  {
    switch(skill)
    {
      case RL::SkillType::walk:
        return {0.f, 1.f, 1.f, 0.f};
      case RL::SkillType::dribble:
        return {0.f, 1.f, 0.f, 0.f};
      case RL::SkillType::pass:
        return {0.f, 0.f, 0.f, 1.f};
      default:
        return {0.f, 0.f, 0.f, 0.f};
    }
  }

  Pose2f defenderGuardTarget(const RL::PPOGateObservation& observation)
  {
    const float threat = std::clamp((defenderMaxX - observation.ballX) / (defenderMaxX - ownGoalX), 0.f, 1.f);
    float x = defenderMinX + (defenderMaxX - defenderMinX) * (1.f - threat);
    if(observation.ballX > 600.f)
      x = -1700.f;
    float y = std::clamp(observation.ballY * 0.65f, -defenderMaxAbsY, defenderMaxAbsY);
    if(std::hypot(x - observation.robotX, y - observation.robotY) < defenderMinWalkStep)
    {
      const float side = observation.ballY >= 0.f ? -1.f : 1.f;
      x = clampFieldX(x - 450.f);
      y = clampFieldY(y + side * 650.f);
    }
    const float theta = std::atan2(observation.ballY - y, observation.ballX - x);
    return Pose2f(wrapAngle(theta), clampFieldX(x), clampFieldY(y));
  }

  Pose2f defenderObserveTarget(const RL::PPOGateObservation& observation)
  {
    const float x = clampFieldX(observation.ballX);
    const float y = clampFieldY(observation.ballY);
    const float theta = std::atan2(y - observation.robotY, x - observation.robotX);
    return Pose2f(wrapAngle(theta), x, y);
  }

  Pose2f defenderBlockTarget(const RL::PPOGateObservation& observation)
  {
    const float x = clampFieldX(observation.ballX - 250.f);
    const float y = clampFieldY(observation.ballY * 0.85f);
    const float theta = std::atan2(observation.ballY - y, observation.ballX - x);
    return Pose2f(wrapAngle(theta), x, y);
  }

  Pose2f defenderMarkTarget(const RL::PPOGateObservation& observation)
  {
    const float side = observation.ballY >= 0.f ? 1.f : -1.f;
    const float x = clampFieldX(observation.ballX - 350.f);
    const float y = clampFieldY(observation.ballY + side * 450.f);
    const float theta = std::atan2(observation.ballY - y, observation.ballX - x);
    return Pose2f(wrapAngle(theta), x, y);
  }
}

SkillRequest RL::PPOActionDecoder::decode(const PPOGateObservation& observation, int skillIndex, const std::array<float, ppoParamCount>& rawParams) const
{
  SkillType skill = SkillType::walk;
  if(skillIndex >= static_cast<int>(SkillType::stand) && skillIndex <= static_cast<int>(SkillType::observe))
    skill = static_cast<SkillType>(skillIndex);

  if(skill != SkillType::stand && skill != SkillType::walk && skill != SkillType::shoot && skill != SkillType::dribble)
    skill = SkillType::walk;

  const auto mask = paramMaskForSkill(skill);
  const float residualX = std::clamp(rawParams[0], -1.f, 1.f) * mask[0];
  const float residualY = std::clamp(rawParams[1], -1.f, 1.f) * mask[1];
  const float residualTheta = std::clamp(rawParams[2], -1.f, 1.f) * mask[2];

  const float walkAnchorTheta = std::atan2(observation.ballY - observation.robotY, observation.ballX - observation.robotX);
  const float goalHeading = std::atan2(-observation.ballY, goalX - observation.ballX);

  float anchorX = 0.f;
  float anchorY = 0.f;
  float anchorTheta = goalHeading;
  switch(skill)
  {
    case SkillType::walk:
      anchorX = clampFieldX(observation.ballX + approachOffsetX);
      anchorY = clampFieldY(observation.ballY);
      anchorTheta = walkAnchorTheta;
      break;
    case SkillType::dribble:
      anchorX = clampFieldX(observation.ballX);
      anchorY = clampFieldY(observation.ballY);
      anchorTheta = goalHeading;
      break;
    default:
      break;
  }

  float targetX = clampFieldX(anchorX + residualX * residualXRange);
  float targetY = clampFieldY(anchorY + residualY * residualYRange);
  float targetTheta = wrapAngle(anchorTheta + residualTheta * residualThetaRange);

  if(skill == SkillType::walk || skill == SkillType::dribble)
  {
    const float targetErrorMm = std::hypot(targetX - anchorX, targetY - anchorY);
    const float thetaError = std::abs(wrapAngle(targetTheta - anchorTheta));
    if(targetErrorMm > repairTargetRadiusLimit || thetaError > repairThetaLimit)
    {
      targetX = anchorX;
      targetY = anchorY;
      targetTheta = anchorTheta;
    }
  }

  switch(skill)
  {
    case SkillType::stand:
      return SkillRequest::Builder::stand();
    case SkillType::shoot:
      return SkillRequest::Builder::shoot();
    case SkillType::dribble:
      return SkillRequest::Builder::dribbleTo(targetTheta);
    case SkillType::walk:
    default:
      return SkillRequest::Builder::walkTo(Pose2f(targetTheta, targetX, targetY));
  }
}

SkillRequest RL::PPOActionDecoder::decodeDefender(const PPOGateObservation& observation, int skillIndex, int passTarget) const
{
  SkillType skill = SkillType::walk;
  if(skillIndex >= static_cast<int>(SkillType::stand) && skillIndex <= static_cast<int>(SkillType::observe))
    skill = static_cast<SkillType>(skillIndex);

  switch(skill)
  {
    case SkillType::stand:
      return SkillRequest::Builder::stand();
    case SkillType::pass:
      return passTarget > 0 ? SkillRequest::Builder::passTo(passTarget) : SkillRequest::Builder::walkTo(defenderGuardTarget(observation));
    case SkillType::shoot:
      return SkillRequest::Builder::shoot();
    case SkillType::block:
      return SkillRequest::Builder::block(defenderBlockTarget(observation).translation);
    case SkillType::mark:
      return SkillRequest::Builder::mark(defenderMarkTarget(observation).translation);
    case SkillType::observe:
      return SkillRequest::Builder::observe(defenderObserveTarget(observation).translation);
    case SkillType::walk:
      return SkillRequest::Builder::walkTo(defenderGuardTarget(observation));
    default:
      return SkillRequest::Builder::walkTo(defenderGuardTarget(observation));
  }
}
