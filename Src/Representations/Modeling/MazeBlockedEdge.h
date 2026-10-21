/**
 * @file MazeBlockedEdge.h
 *
 * The latest edge that stopped a walk of the robot (MZ-046), provided by the maze behaviour for MazeMapper, which turns it into a
 * wall that no wall percept can open again. `counter` grows with every block so that the mapper acts on each one once. The cell is in the
 * maze frame of the working hypothesis of the start, like `MazeCellPose` and `MazeMap`; `revision` is the `MazeStart::revision` it
 * was published in, and a block published in another frame is dropped. See Tools/Maze/MazeBlockedEdgeChannel.h.
 */

#pragma once

#include "Representations/Modeling/MazeCellPose.h"
#include "Streaming/AutoStreamable.h"

STREAMABLE(MazeBlockedEdge,
{,
  (unsigned)(0) counter,                         /**< Number of blocks published so far; 0 if none. */
  (int)(0) row,                                  /**< Row of the cell the blocked walk started in. */
  (int)(0) col,                                  /**< Column of that cell. */
  (MazeCellPose::Heading)(MazeCellPose::north) heading, /**< The side of that cell the walk was blocked by. */
  (int)(0) revision,                             /**< `MazeStart::revision` of the frame the cell is in. */
  (unsigned)(0) time,                            /**< When the block was published (ms). */
});
