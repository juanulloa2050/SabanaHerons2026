#pragma once

#include "PPOCommon.h"

#include "Representations/BehaviorControl/SkillRequest.h"

namespace RL
{
  class PPOActionDecoder
  {
  public:
    SkillRequest decode(const PPOGateObservation& observation, int skillIndex, const std::array<float, ppoParamCount>& paramMean) const;
    SkillRequest decodeDefender(const PPOGateObservation& observation, int skillIndex, int passTarget) const;
    // Decode for the team model (v4.2+): striker skills + pass.
    // passTarget is the pre-selected player number (or -1 when no forward outlet).
    SkillRequest decodeTeam(const PPOGateObservation& observation, int skillIndex, const std::array<float, ppoParamCount>& paramMean, int passTarget) const;
    // Decode for merged brain support/defender roles: walk anchors on the explicit coordination
    // target instead of the ball. Block/mark use param[1] as a lateral offset (scale 900mm).
    // Dribble (escape hatch) falls back to ball anchor. All other skills identical to decodeTeam.
    SkillRequest decodeTeamWalkWithAnchor(const PPOGateObservation& observation, int skillIndex, const std::array<float, ppoParamCount>& paramMean, float anchorX, float anchorY, float anchorTheta, int passTarget) const;
  };
}
