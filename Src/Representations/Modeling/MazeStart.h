/**
 * @file MazeStart.h
 *
 * Which corner the robot started in and which way it faced, provided by MazeMapper. The start is either given
 * (`start.cell` of `handleMaze.cfg`) or deduced from the AprilTags and the walls; until one of the 8 hypotheses
 * (4 corners times 2 open sides) is left, `cell` and `heading` are the working hypothesis: the start whose
 * maze frame the localizer and the mapper currently use. The planner and the skills need no special case,
 * because that frame is a valid maze whose goal block is the same as in every other hypothesis.
 *
 * When the working hypothesis changes (`revision` grows) the mapper rebuilds its map and the localizer moves its
 * pose into the new frame. The hypotheses are numbered like `Maze::StartDeduction::hypotheses()`: corners 0, 7,
 * 56 and 63, each with its two headings in `Heading` order; bit i of `aliveMask` is hypothesis i.
 */

#pragma once

#include "Representations/Modeling/MazeCellPose.h"
#include "Streaming/AutoStreamable.h"

STREAMABLE(MazeStart,
{,
  (bool)(false) given,                       /**< The start comes from the configuration, nothing is deduced. */
  (bool)(false) known,                       /**< One hypothesis is left (or the start is given): `cell` and `heading` are the start. */
  (int)(0) cell,                             /**< Start cell of the working hypothesis. */
  (MazeCellPose::Heading)(MazeCellPose::east) heading,     /**< Maze heading the robot faced at the start under the working hypothesis. */
  (int)(8) aliveCount,                       /**< Hypotheses that are left. */
  (unsigned)(255) aliveMask,                 /**< Bit i is set while hypothesis i is alive. */
  (int)(0) revision,                         /**< Grows whenever the working hypothesis changes. */
  (int)(0) conflicts,                        /**< Observations that contradicted the only hypothesis left (or the given one) and were not followed. */
  (bool)(false) logOverflowed,               /**< The observation log filled up before the start was known: a re-anchoring would miss the later observations. */
});
