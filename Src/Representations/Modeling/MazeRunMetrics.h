/**
 * @file MazeRunMetrics.h
 *
 * The measurement of a simulated maze run, provided by MazeRunMetricsProvider (simulation only, from the ground truth, never on
 * the robot). It holds the attempt that runs or the last one that ended; the metrics are defined in Tools/Maze/MazeRunTracker.h.
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"

STREAMABLE(MazeRunMetrics,
{
  /** Where the measured sequence is; the values match `Maze::RunPhase`. */
  ENUM(State,
  {,
    waitingForStart, /**< No attempt yet: waiting for the game to play and for the first tap. */
    running,         /**< An attempt runs. */
    returning,       /**< Attempt 1 is over, waiting for the robot to stand at the start. */
    done,            /**< Every requested attempt has a result. */
  });

  /** Why the attempt ended; the values match `Maze::RunStatus`. */
  ENUM(Status,
  {,
    goal,       /**< A goal cell was entered. */
    timeout,    /**< The attempt timeout passed. */
    notStarted, /**< The attempt did not run. */
  });

  ,
  (State)(waitingForStart) state,   /**< Where the sequence is. */
  (int)(0) attempt,                 /**< The attempt that runs, or the last one; 0 before the first tap. */
  (bool)(false) reachedGoal,        /**< The robot centre entered a goal cell in this attempt. */
  (unsigned)(0) timeMs,             /**< Simulated time since the tap (so far, or until the goal or the timeout). */
  (int)(0) wallContacts,            /**< Entries into contact with a wall (proxy, see MazeRunTracker.h). */
  (unsigned)(0) maxStallMs,         /**< Longest interval without progress in this attempt. */
  (Status)(goal) status,            /**< Why the attempt ended; only meaningful when `state` is not running. */
});
