/**
 * @file RLSkillProvider.h
 *
 * Replaces StrategyBehaviorControl for RL training.
 * Reads action from RLSharedState (set by Python via pybh.controller),
 * provides SkillRequest + StrategyStatus, and writes back the observation.
 */

#pragma once

#include "Representations/BehaviorControl/SkillRequest.h"
#include "Representations/BehaviorControl/ExpectedGoals.h"
#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/BehaviorControl/StrategyStatus.h"
#include "Representations/Communication/TeamData.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Infrastructure/GroundTruthWorldState.h"
#include "Representations/Modeling/BallModel.h"
#include "Representations/Modeling/ObstacleModel.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/MotionControl/MotionRequest.h"
#include "Framework/Module.h"

MODULE(RLSkillProvider,
{,
  REQUIRES(GroundTruthWorldState),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(FieldDimensions),
  USES(RobotPose),
  USES(BallModel),
  USES(FieldBall),
  USES(ObstacleModel),
  USES(ExpectedGoals),
  USES(TeamData),
  PROVIDES(MotionRequest),
  PROVIDES(SkillRequest),
  PROVIDES(StrategyStatus),
});

class RLSkillProvider : public RLSkillProviderBase
{
private:
  void update(MotionRequest& motionRequest) override;
  void update(SkillRequest& skillRequest) override;
  void update(StrategyStatus& strategyStatus) override;
};
