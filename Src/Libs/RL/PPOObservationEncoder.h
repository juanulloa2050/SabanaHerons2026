#pragma once

#include "PPOCommon.h"

#include "Representations/BehaviorControl/ExpectedGoals.h"
#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/Communication/TeamData.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Modeling/BallModel.h"
#include "Representations/Modeling/ObstacleModel.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"

namespace RL
{
  class PPOObservationEncoder
  {
  public:
    void reset();

    PPOGateObservation buildRawObservation(
      const FrameInfo& frameInfo,
      const RobotPose& robotPose,
      const FieldBall& fieldBall,
      const BallModel& ballModel,
      const BallPercept& ballPercept,
      const ObstacleModel& obstacleModel,
      const ExpectedGoals& expectedGoals,
      const TeamData& teamData,
      const FieldDimensions& fieldDimensions);

    PPOObservation encode(const PPOGateObservation& rawObservation, const PPOGateDecision& gateDecision) const;
    PPOObservation encodeDefender(const PPOGateObservation& rawObservation, const PPOGateDecision& gateDecision) const;

    PPOObservation encode(
      const FrameInfo& frameInfo,
      const RobotPose& robotPose,
      const FieldBall& fieldBall,
      const BallModel& ballModel,
      const BallPercept& ballPercept,
      const ObstacleModel& obstacleModel,
      const ExpectedGoals& expectedGoals,
      const TeamData& teamData,
      const FieldDimensions& fieldDimensions,
      const PPOGateDecision& gateDecision);

  private:
    unsigned lastNaturalBallSeenTimestamp = 0;
    bool hasNaturalBallSeenTimestamp = false;
  };
}
