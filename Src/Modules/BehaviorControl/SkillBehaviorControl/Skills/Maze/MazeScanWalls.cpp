/**
 * @file MazeScanWalls.cpp
 *
 * This file implements the MazeScanWalls skill: pan the head to the left, to the right and back to the front so that
 * the wall perceptor sees the walls of the cells around or ahead of the robot (roadmap 4.7). It is done at the end of the
 * scan and the head then looks forward. A new call after a frame without a call starts a new scan. Two sequences:
 *  - wide (the goal look of the return, MZ-024.1): +60 deg 550 ms, -60 deg 800 ms, 0 deg 450 ms (under 2 s). The head only
 *    passes +-30 deg on its way, so the look-ahead sees the side corridors of the cell the robot stands in.
 *  - not wide (the scan of the next cell, MZ-047): the head holds +30 deg, -30 deg and 0 deg for `holdTime` each. At +-30 deg
 *    the gaze snaps to the robot heading and the upper camera sees the near side walls of the next cell from the centre of
 *    this one, but the mapper takes one observation per edge every 250 ms and needs about 3, so the head has to stay.
 *    The hold counts from the moment the head should have reached the pan (the travel at `headSpeed` is added), not from
 *    the command, so that a pan of 60 deg between the two sides does not eat the hold of the second one.
 */

#include "Representations/BehaviorControl/Skills.h"
#include "Tools/BehaviorControl/Framework/Skill/CabslSkill.h"
#include <cmath>

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
    (Angle)(30_deg) holdPan, /**< Not wide: how far the head turns to each side and holds. */
    (Angle)(0.38f) holdTilt, /**< Not wide: the head tilt during the scan (the wall of the next cell is seen at 0.38 and +-30 deg). */
    (int)(1000) holdTime, /**< Not wide: how long the head holds each of the three gazes once it has arrived (ms). */
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

    /** The time the head needs to move `angle` (ms). */
    const auto travel = [this](const Angle angle)
    {
      return static_cast<int>(std::lround(std::abs(angle) / headSpeed * 1000.f));
    };
    const auto hold = [this](const Angle pan)
    {
      theLookAtAnglesSkill({.pan = pan, .tilt = holdTilt, .speed = headSpeed});
    };

    initial_state(start)
    {
      transition
      {
        if(p.wide)
          goto left;
        else
          goto holdLeft;
      }
    }

    state(left)
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

    // The held scan: +holdPan (from the front), -holdPan (twice the way) and 0 (back from -holdPan).
    state(holdLeft)
    {
      transition
      {
        if(state_time > travel(holdPan) + holdTime)
          goto holdRight;
      }
      action
      {
        hold(holdPan);
      }
    }

    state(holdRight)
    {
      transition
      {
        if(state_time > travel(2.f * holdPan) + holdTime)
          goto holdFront;
      }
      action
      {
        hold(-holdPan);
      }
    }

    state(holdFront)
    {
      transition
      {
        if(state_time > travel(holdPan) + holdTime)
          goto done;
      }
      action
      {
        hold(0_deg);
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
