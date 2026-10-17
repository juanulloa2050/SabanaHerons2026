/**
 * @file Tools/Maze/MazeRoutes.h
 *
 * The two routes that the behaviour needs from the planner, computed together for one pose and one map:
 *  - exploration: the optimal route to the goal when unknown edges count as open (attempt 1; its first action is
 *    what the robot does next, and it is replanned whenever the map or the pose changes),
 *  - speed run: the optimal route over the edges that are known to be open (attempt 2; unknown edges are walls).
 * Each route also carries the planning states it passes through, so that a consumer can read the cell path and the
 * target of the next action without replaying the actions. Pure standard C++ so that `MazePlannerProvider` and the
 * unit tests use the same code.
 */

#pragma once

#include "MazePlanner.h"
#include <vector>

namespace Maze
{
  /** A route from the start state to a goal cell. */
  struct RouteResult
  {
    bool found = false;
    std::vector<Action> actions;
    std::vector<MazeState> states; /**< `states[0]` is the start, `states[i + 1]` the state after `actions[i]`. Empty if not found. */
    int cells = 0;                 /**< Forward steps. */
    int turns90 = 0;
    int turns180 = 0;
    int timeMs = 0;                /**< Estimated duration with the costs of the planner. */

    /** The state after the first action; the start itself if the route has no action (already at the goal). @pre found */
    [[nodiscard]] const MazeState& firstTarget() const { return states.size() > 1 ? states[1] : states.front(); }
  };

  struct RoutePair
  {
    bool atGoal = false;     /**< The start is a goal cell: both routes are found and empty. */
    RouteResult exploration;
    RouteResult speedRun;
  };

  /** Whether the planner can use `costs`: a step must take time, otherwise the shortest-route search is not well defined. */
  [[nodiscard]] bool costsValid(const MazeCosts& costs);

  /**
   * Both routes from `start` over `map`. `start.moving` is taken as given: the robot stands in a cell when it plans, so it
   * should be false, which charges the start-up time of the first step.
   * A start that is invalid (outside the grid) gives two routes that are not found.
   */
  [[nodiscard]] RoutePair planRoutes(const MazeGraph& map, const MazeState& start, const MazeCosts& costs);
}
