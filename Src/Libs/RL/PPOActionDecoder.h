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
  };
}
