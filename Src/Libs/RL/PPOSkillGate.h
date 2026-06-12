#pragma once

#include "PPOCommon.h"

namespace RL
{
  class PPOSkillGate
  {
  public:
    void reset();
    PPOGateDecision step(const PPOGateObservation& observation);
    PPOGateDecision stepDefender(const PPOGateObservation& observation, bool hasPassTarget, bool engageAllowed);

  private:
    bool shootArmed = false;
    bool dribbleArmed = false;
    bool passArmed = false;
    bool engageArmed = false;
    int shootStreak = 0;
    int dribbleStreak = 0;
    int passStreak = 0;
    int engageStreak = 0;
  };
}
