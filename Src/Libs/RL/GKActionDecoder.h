#pragma once

#include "GKCommon.h"

#include "Representations/BehaviorControl/SkillRequest.h"

namespace RLGK
{
  // Maps a chosen GK skill + params to a B-Human SkillRequest using keeper
  // primitives (walkTo / observe / interceptBall / keeperDive / shoot / passTo /
  // dribbleTo). See RL/docs/39 §3 T.4 for the skill -> behavior table.
  class GKActionDecoder
  {
  public:
    // passTarget: teammate number chosen by the runtime for the `pass` skill
    // (the policy does not pick it). If < 0, `pass` falls back to a clear.
    SkillRequest decode(const GKRawObservation& raw, int skillIndex,
                        const std::array<float, gkParamCount>& paramMean, int passTarget) const;
  };
}
