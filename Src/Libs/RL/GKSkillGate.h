#pragma once

#include "GKCommon.h"

namespace RLGK
{
  // Legal-action mask for the goalkeeper, computed from the ENCODED 64-dim obs.
  // Exact port of goalkeeper_teacher.goalkeeper_skill_mask_np (Design-A). The caller
  // MUST set illegal logits to -inf BEFORE argmax (inference = mask THEN argmax).
  class GKSkillGate
  {
  public:
    // Returns a 0/1 mask over the 12 GK skills.
    std::array<float, gkSkillCount> mask(const std::array<float, gkObsSize>& obs) const;
  };
}
