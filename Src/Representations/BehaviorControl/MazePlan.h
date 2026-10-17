/**
 * @file MazePlan.h
 *
 * What the behaviour should do next in the maze, provided by MazePlannerProvider for the current `MazeCellPose` and
 * `MazeMap`. Both routes are always computed, the behaviour picks the one it needs:
 *  - `exploration` (attempt 1): the optimal route to the goal if unknown edges are open. Its `nextAction` is what
 *    the robot does now; the plan changes whenever the map learns an edge, so only the first action is binding.
 *  - `speedRun` (attempt 2): the optimal route over edges that are known to be open. `found` is false if the known
 *    edges do not connect the pose to the goal. The whole action list is the route to drive.
 *
 * A route is expressed in cells of the maze frame (row 0 is the north row, heading clockwise from north), the same
 * convention as `MazeCellPose`. The plan describes the pose it was computed for (`row`, `col`, `heading`) and the
 * map it was computed from (`mapChangeCounter`); a consumer that wants a fresh plan compares them with the current
 * representations, or just reads `revision`, which grows on every recomputation.
 */

#pragma once

#include "Representations/Modeling/MazeCellPose.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include <vector>

/** One route to the goal. */
STREAMABLE(MazeRoute,
{
  /** The primitive motions of the planner, same order as `Maze::Action`. `none` when there is nothing to do. */
  ENUM(Action,
  {,
    forward,    /**< One cell ahead. */
    turnLeft,   /**< 90 degrees counterclockwise in place. */
    turnRight,  /**< 90 degrees clockwise in place. */
    turnAround, /**< 180 degrees in place. */
    none,       /**< No route, or the pose is already in a goal cell. */
  });

  /**
   * Number of 90 and 180 degree turns of the route.
   * @return Turns of either kind.
   */
  int turns() const { return turns90 + turns180; },

  (bool)(false) found,                          /**< Whether the goal can be reached. If false everything else is empty. */
  (Action)(none) nextAction,                    /**< First action of the route; `none` if not found or already in the goal. */
  (int)(0) targetRow,                           /**< Cell the robot is in after `nextAction`: the neighbour for `forward`, the current cell for a turn. */
  (int)(0) targetCol,
  (MazeCellPose::Heading)(MazeCellPose::north) targetHeading, /**< Heading after `nextAction`: unchanged for `forward`, the turned one otherwise. */
  (std::vector<int>) cells,                     /**< Cell indices (row * 8 + col) from the current cell to the first goal cell, both included. */
  (std::vector<Action>) actions,                /**< All actions of the route, in order. */
  (int)(0) estimatedTimeMs,                     /**< Estimated duration with the costs of `mazePlanner.cfg`. */
  (int)(0) forwardSteps,                        /**< Number of `forward` actions. */
  (int)(0) turns90,                             /**< Number of `turnLeft` and `turnRight` actions. */
  (int)(0) turns180,                            /**< Number of `turnAround` actions. */
});

STREAMABLE(MazePlan,
{
  /** Draws both routes over the maze in the maze frame. */
  void draw() const,

  (bool)(false) valid,                          /**< The pose was inside the maze; otherwise both routes are not found. */
  (bool)(false) atGoal,                         /**< The pose is in a goal cell; both routes are found and empty. */
  (MazeRoute) exploration,                      /**< Unknown edges count as open. */
  (MazeRoute) speedRun,                         /**< Only edges known to be open. */
  (int)(0) row,                                 /**< The pose the plan was computed for (row, column, heading of `MazeCellPose`). */
  (int)(0) col,
  (MazeCellPose::Heading)(MazeCellPose::north) heading,
  (unsigned)(0) mapChangeCounter,               /**< `MazeMap::changeCounter` of the map the plan was computed from. */
  (unsigned)(0) revision,                       /**< Grows on every recomputation. */
  (unsigned)(0) computedTime,                   /**< `FrameInfo::time` of the last recomputation in ms. */
});
