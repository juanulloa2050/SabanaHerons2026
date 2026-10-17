/**
 * @file MazeTurn.cpp
 *
 * This file implements the MazeTurn skill: turn in place, at a cell centre, to a maze heading (roadmap 4.7).
 * The remaining angle is read from the RobotPose, which follows the gyro heading of the maze localizer (and its
 * tag fixes), not from the odometry rotation alone, so a drifting odometry does not leave the robot off the heading.
 */

#include "Representations/BehaviorControl/Skills.h"
#include "Representations/Modeling/RobotPose.h"
#include "Tools/Maze/MazeHeading.h"
#include <cmath>

SKILL_IMPLEMENTATION(MazeTurnImpl,
{,
  IMPLEMENTS(MazeTurn),
  CALLS(WalkToPose),
  REQUIRES(RobotPose),
  DEFINES_PARAMETERS(
  {,
    (Angle)(6_deg) margin, /**< The skill is done when the remaining angle is smaller than this. */
    (float)(1.f) turnSpeed, /**< The rotation speed as ratio of the maximum speed in [0, 1]. */
  }),
});

class MazeTurnImpl : public MazeTurnImplBase
{
  /** The angle that is left to turn (counterclockwise positive). */
  Angle remaining(const MazeTurn& p) const
  {
    return Angle::normalize(Maze::angleOf(static_cast<Maze::Heading>(static_cast<int>(p.heading))) - theRobotPose.rotation);
  }

  void execute(const MazeTurn& p) override
  {
    theWalkToPoseSkill({.target = Pose2f(remaining(p), 0.f, 0.f),
                        .speed = Pose2f(turnSpeed, 1.f, 1.f),
                        .keepTargetRotation = true});
  }

  bool isDone(const MazeTurn& p) const override
  {
    return std::abs(remaining(p)) < margin;
  }
};

MAKE_SKILL_IMPLEMENTATION(MazeTurnImpl);
