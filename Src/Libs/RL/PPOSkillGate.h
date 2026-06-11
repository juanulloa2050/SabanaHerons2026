#pragma once

#include "PPOCommon.h"

namespace RL
{
  class PPOSkillGate
  {
  public:
    void reset();
    PPOGateDecision step(const PPOGateObservation& observation);

  private:
    bool shootArmed = false;
    bool dribbleArmed = false;
    int shootStreak = 0;
    int dribbleStreak = 0;
  };
}
