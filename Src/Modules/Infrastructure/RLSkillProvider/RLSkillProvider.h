/**
 * @file RLSkillProvider.h
 *
 * Reads action from RLSharedState (set by Python via pybh.controller)
 * and provides SkillRequest + StrategyStatus for the normal skill pipeline.
 */

// Sabana Herons: turns Python RL commands into SkillRequests in the RL scenarios.

#pragma once

#include "Representations/BehaviorControl/SkillRequest.h"
#include "Representations/BehaviorControl/StrategyStatus.h"
#include "Representations/Infrastructure/GameState.h"
#include "Framework/Module.h"

MODULE(RLSkillProvider,
{,
  REQUIRES(GameState),
  PROVIDES(SkillRequest),
  PROVIDES(StrategyStatus),
});

class RLSkillProvider : public RLSkillProviderBase
{
private:
  void update(SkillRequest& skillRequest) override;
  void update(StrategyStatus& strategyStatus) override;
};
