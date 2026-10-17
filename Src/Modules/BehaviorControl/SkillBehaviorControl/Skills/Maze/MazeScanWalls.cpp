/**
 * @file MazeScanWalls.cpp
 *
 * This file implements the MazeScanWalls skill: pan the head to the left, to the right and back to the front so that
 * the wall perceptor sees the sides of a cell whose edges are still unknown (roadmap 4.7). The whole scan is shorter
 * than 2 s. The skill is only called while the robot stands in a cell with unknown edges; it is done at the end of the scan
 * and the head then looks forward. A new call after a frame without a call starts a new scan.
 */

#include "Representations/BehaviorControl/Skills.h"
#include "Tools/BehaviorControl/Framework/Skill/CabslSkill.h"

SKILL_IMPLEMENTATION(MazeScanWallsImpl,
{,
  IMPLEMENTS(MazeScanWalls),
  CALLS(LookAtAngles),
  DEFINES_PARAMETERS(
  {,
    (Angle)(60_deg) panAngle, /**< How far the head turns to each side. */
    (Angle)(0.38f) tilt, /**< The head tilt during the scan (the same as the LookForward skill). */
    (Angle)(200_deg) headSpeed, /**< The speed of the head (rad/s). */
    (int)(550) leftTime, /**< Time to reach the left side and look at it (ms). */
    (int)(800) rightTime, /**< Time to cross to the right side and look at it (ms). */
    (int)(450) frontTime, /**< Time to come back to the front (ms). */
  }),
});

class MazeScanWallsImpl : public MazeScanWallsImplBase
{
  option(MazeScanWalls)
  {
    const auto look = [this](const Angle pan)
    {
      theLookAtAnglesSkill({.pan = pan, .tilt = tilt, .speed = headSpeed});
    };

    initial_state(left)
    {
      transition
      {
        if(state_time > leftTime)
          goto right;
      }
      action
      {
        look(panAngle);
      }
    }

    state(right)
    {
      transition
      {
        if(state_time > rightTime)
          goto front;
      }
      action
      {
        look(-panAngle);
      }
    }

    state(front)
    {
      transition
      {
        if(state_time > frontTime)
          goto done;
      }
      action
      {
        look(0_deg);
      }
    }

    target_state(done)
    {
      action
      {
        look(0_deg);
      }
    }
  }
};

MAKE_SKILL_IMPLEMENTATION(MazeScanWallsImpl);
