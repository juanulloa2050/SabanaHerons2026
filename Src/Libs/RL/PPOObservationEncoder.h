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

    // Encode 47-dim observation for the v4.2+ team model.
    // obs[0:26] = same as encode(); obs[26:47] = team context + gate team bits + role.
    // The caller (StrategyBehaviorControl) builds teamContext from TeamData /
    // TeammatesBallModel each frame and sets passArmed / role bits inside it.
    std::array<float, ppoObsSize47> encode47(
      const PPOGateObservation& rawObservation,
      const PPOGateDecision& gateDecision,
      const PPOTeamContext& teamContext) const;

  private:
    unsigned lastNaturalBallSeenTimestamp = 0;
    bool hasNaturalBallSeenTimestamp = false;
  };
}
