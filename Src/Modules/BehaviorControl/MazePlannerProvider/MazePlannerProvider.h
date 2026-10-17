/**
 * @file MazePlannerProvider.h
 *
 * Provides the `MazePlan`: for the current cell pose and wall map, the exploration route (unknown edges are open,
 * attempt 1) and the speed-run route (only edges known to be open, attempt 2), both computed with the time-weighted
 * planner of Tools/Maze/MazePlanner.h. The behaviour decides which one it follows, so the provider needs no mode.
 *
 * The plan is recomputed only when the `MazeMap::changeCounter`, the cell or the heading of the `MazeCellPose` or the costs change
 * (a planning takes well under a millisecond on the 8x8 grid on a development PC; MazeRoutesTest prints the time and only
 * guards against a gross regression, it is no real-time guarantee on the robot). A pose outside the maze gives
 * a plan that is not valid and has no route.
 *
 * The motion durations come from `mazePlanner.cfg`; the keys are the fields of `Maze::MazeCosts`:
 * `forwardMs` (t_fwd), `startMs` (t_start), `turn90Ms` (t_turn90), `turn180Ms` (t_turn180). Invalid values (a step that
 * takes no time) are reported and replaced by the defaults of `Maze::MazeCosts`.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/BehaviorControl/MazePlan.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Modeling/MazeCellPose.h"
#include "Representations/Modeling/MazeMap.h"
#include "Tools/Maze/MazeRoutes.h"

MODULE(MazePlannerProvider,
{,
  REQUIRES(FrameInfo),
  REQUIRES(MazeCellPose),
  REQUIRES(MazeMap),
  PROVIDES(MazePlan),
  LOADS_PARAMETERS(
  {,
    (int) forwardMs,   /**< One cell forward while already walking (ms). */
    (int) startMs,     /**< Extra time for the first step after standing, after a turn or at the start (ms). */
    (int) turn90Ms,    /**< A 90 degree turn in place (ms). */
    (int) turn180Ms,   /**< A 180 degree turn in place (ms). */
  }),
});

/**
 * @class MazePlannerProvider
 * The routes to the goal of the maze.
 */
class MazePlannerProvider : public MazePlannerProviderBase
{
public:
  MazePlannerProvider();

private:
  void update(MazePlan& mazePlan) override;

  /** The durations of the primitive motions as the planner takes them. */
  Maze::MazeCosts costs() const;

  /** Replaces invalid costs by the defaults (with an error), at start-up and whenever the parameters change. */
  void ensureValidCosts();

  /** Whether the cached plan was computed for the current inputs (map, pose and costs). */
  bool upToDate() const;

  /** Copies a route of the planner into the representation. */
  static void fill(MazeRoute& route, const Maze::RouteResult& result);

  MazePlan cached;
  Maze::MazeCosts plannedCosts; /**< The costs `cached` was computed with. */
  bool computed = false;
};
