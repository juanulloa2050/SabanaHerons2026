/* SabanaHerons fork extension (B-Human 2023 base).
 * Translate a selected field-player action into a B-Human SkillRequest, using tactical
 * anchors and valid pass targets rather than joint commands.
 * Release overview and commit references: README.md.
 */

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

    // Decode a walk skill anchored on an explicit coordination target instead of the ball.
    // Used for open_support and off_ball_support roles in the merged v5 model.
    SkillRequest decodeTeamWalkWithAnchor(float anchorX, float anchorY, float anchorTheta,
                                          const std::array<float, ppoParamCount>& rawParams) const;
  };
}
