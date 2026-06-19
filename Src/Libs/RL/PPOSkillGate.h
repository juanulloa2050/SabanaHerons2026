#pragma once

#include "PPOCommon.h"

namespace RL
{
  class PPOSkillGate
  {
  public:
    // Shoot lane thresholds (shot_opening_with_obstacles). v4.2+ uses 0.30/0.20;
    // the legacy 26-dim model uses the original 0.6/0.45.
    float shootOpenEnter = 0.6f;
    float shootOpenExit = 0.45f;

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
