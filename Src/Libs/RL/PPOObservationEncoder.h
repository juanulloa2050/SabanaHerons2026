#pragma once

#include "PPOCommon.h"

#include "Representations/BehaviorControl/ExpectedGoals.h"
#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/Communication/TeamData.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Modeling/BallModel.h"
#include "Representations/Modeling/ObstacleModel.h"
#include "Representations/Modeling/RobotPose.h"

namespace RL
{
  class PPOObservationEncoder
  {
  public:
    PPOGateObservation buildRawObservation(
      const RobotPose& robotPose,
      const FieldBall& fieldBall,
      const BallModel& ballModel,
      const ObstacleModel& obstacleModel,
      const ExpectedGoals& expectedGoals,
      const TeamData& teamData,
      const FieldDimensions& fieldDimensions) const;

    PPOObservation encode(const PPOGateObservation& rawObservation, const PPOGateDecision& gateDecision) const;

    PPOObservation encode(
      const RobotPose& robotPose,
      const FieldBall& fieldBall,
      const BallModel& ballModel,
      const ObstacleModel& obstacleModel,
      const ExpectedGoals& expectedGoals,
      const TeamData& teamData,
      const FieldDimensions& fieldDimensions,
      const PPOGateDecision& gateDecision) const;
  };
}
