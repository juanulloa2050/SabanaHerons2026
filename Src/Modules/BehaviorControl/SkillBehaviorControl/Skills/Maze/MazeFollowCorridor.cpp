/**
 * @file MazeFollowCorridor.cpp
 *
 * This file implements the MazeFollowCorridor skill: walk to the centre of a neighbouring maze cell while the robot
 * keeps the maze heading (roadmap 4.7). The target is a pose in the maze frame (the frame of the RobotPose), so
 * aiming at the cell centre also centres the robot in the corridor, and its rotation locks the heading. There is no
 * obstacle avoidance and no field-border avoidance: the walls of the maze are the planner's business.
 */

#include "Representations/BehaviorControl/Skills.h"
#include "Representations/Modeling/RobotPose.h"
#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeOracle.h"
#include <cmath>

SKILL_IMPLEMENTATION(MazeFollowCorridorImpl,
{,
  IMPLEMENTS(MazeFollowCorridor),
  CALLS(WalkToPose),
  REQUIRES(RobotPose),
  DEFINES_PARAMETERS(
  {,
    (float)(70.f) positionMargin, /**< The skill is done when the robot is this close to the cell centre (in mm). */
    (Angle)(15_deg) headingMargin, /**< ... and its heading differs from the target heading by less than this. */
    (float)(1.f) walkSpeed, /**< The walking speed as ratio of the maximum speed in [0, 1]. */
    (float)(1.f) turnSpeed, /**< The rotation speed as ratio of the maximum speed in [0, 1]. */
  }),
});

class MazeFollowCorridorImpl : public MazeFollowCorridorImplBase
{
  /** The target pose in the maze frame. */
  static Pose2f targetOf(const MazeFollowCorridor& p)
  {
    return Pose2f(Maze::angleOf(static_cast<Maze::Heading>(static_cast<int>(p.heading))),
                  Maze::Oracle::cellCentreOf(p.targetRow, p.targetCol));
  }

  void execute(const MazeFollowCorridor& p) override
  {
    theWalkToPoseSkill({.target = theRobotPose.inverse() * targetOf(p),
                        .speed = Pose2f(turnSpeed, walkSpeed, walkSpeed),
                        .keepTargetRotation = true});
  }

  bool isDone(const MazeFollowCorridor& p) const override
  {
    const Pose2f target = targetOf(p);
    return (target.translation - theRobotPose.translation).norm() < positionMargin &&
           std::abs(Angle::normalize(target.rotation - theRobotPose.rotation)) < headingMargin;
  }
};

MAKE_SKILL_IMPLEMENTATION(MazeFollowCorridorImpl);
