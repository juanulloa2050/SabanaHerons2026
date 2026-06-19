#pragma once

#include "GKCommon.h"

#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/Communication/TeamData.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Modeling/BallModel.h"
#include "Representations/Modeling/ObstacleModel.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/Modeling/TeammatesBallModel.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Sensing/FallDownState.h"
#include "Representations/Sensing/GroundContactState.h"

namespace RLGK
{
  // Builds the 64-dim GK observation from B-Human representations, exactly matching
  // goalkeeper_teacher.encode_goalkeeper_obs (+ build_gk_cognition / _goalie_pose /
  // _time_to_own_y_axis). Cognition (goalie BOB-line pose, time-to-y-axis, rolling,
  // ball control, fresh/stale, posts, penalty area) is replicated here so no extra
  // representation is required and the feature distribution matches training.
  class GKObservationEncoder
  {
  public:
    void reset();

    GKRawObservation buildRawObservation(
      const FrameInfo& frameInfo,
      const RobotPose& robotPose,
      const FieldBall& fieldBall,
      const BallModel& ballModel,
      const BallPercept& ballPercept,
      const ObstacleModel& obstacleModel,
      const TeamData& teamData,
      const TeammatesBallModel& teammatesBallModel,
      const FallDownState& fallDownState,
      const GroundContactState& groundContactState,
      const FieldDimensions& fieldDimensions);

    GKObservation encode(const GKRawObservation& raw) const;

    GKObservation encode(
      const FrameInfo& frameInfo,
      const RobotPose& robotPose,
      const FieldBall& fieldBall,
      const BallModel& ballModel,
      const BallPercept& ballPercept,
      const ObstacleModel& obstacleModel,
      const TeamData& teamData,
      const TeammatesBallModel& teammatesBallModel,
      const FallDownState& fallDownState,
      const GroundContactState& groundContactState,
      const FieldDimensions& fieldDimensions);

  private:
    unsigned lastNaturalBallSeenTimestamp = 0;
    bool hasNaturalBallSeenTimestamp = false;
  };
}
