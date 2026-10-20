/**
 * @file MazeRunMetricsProvider.h
 *
 * Simulation only (MZ-021): measures the maze attempts of a SimRobot run from the ground truth robot pose and the maze file, and
 * starts them. Without a ground truth it does not exist on the robot, so the scene switches it on:
 *
 *   for Cognition mr MazeRunMetrics MazeRunMetricsProvider
 *
 * It taps the head front through the debug request (as WallPerceptEvaluator does): a `dr` line in the scene comes before the
 * behaviour is ready and is lost. The first tap comes `startTapAfterMs` after the game state became playing; with
 * `startTapAfterMs = 0` the module does nothing. The second attempt (`attempts = 2`) starts when the robot has walked back to the
 * start cell on its own (`returnToStart` of handleMaze.cfg, trigger = timer), stands still and believes to be at the start (`MazePlan.atStart`), see Tools/Maze/MazeRunTracker.h.
 *
 * For every attempt that ended one line goes to the standard error stream, which is all a headless SimRobot shows:
 *
 *   MZ021_METRICS {"attempt":1,"reached_goal":true,"time_s":93.4,"wall_contacts":0,"max_stall_s":4.2,"speed_run":false,"status":"goal"}
 *
 * (`status` is goal, timeout or not_started; `speed_run` says whether the robot's map held a route to the goal at the tap), and `MZ021_DONE` after the last requested attempt. Util/MazeTools/run_sim_campaign.py
 * reads these lines.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/BehaviorControl/MazePlan.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/MazeRunMetrics.h"
#include "Representations/Modeling/RobotPose.h"
#include "Tools/Maze/MazeRunTracker.h"
#include <memory>
#include <string>

MODULE(MazeRunMetricsProvider,
{,
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(GroundTruthRobotPose),
  REQUIRES(MazePlan),
  PROVIDES(MazeRunMetrics),
  LOADS_PARAMETERS(
  {,
    (std::string) mazeFile,        /**< The maze, relative to the repository root; the same as in oracledMazePerceptsProvider.cfg. */
    (Pose2f) mazePoseOnField,      /**< Pose of the maze frame (mm) in the frame of the ground truth robot pose; the same as in the oracle cfg. */
    (int) attempts,                /**< Attempts to run: 1 or 2. */
    (int) startTapAfterMs,         /**< First head tap this long after the game state became playing (0: do nothing). */
    (int) attemptTimeoutMs,        /**< An attempt that has not reached the goal after this long is over. */
    (int) returnTimeoutMs,         /**< Longest wait between the goal of attempt 1 and the robot standing at the start. */
    (int) settleMs,                /**< The robot stands still this long in the start cell before the second tap. */
    (float) contactRadius,         /**< Distance in mm from the robot centre to a wall centre line below which it is in contact. */
    (float) hysteresis,            /**< Distance in mm above contactRadius that ends a contact. */
    (float) stallDistance,         /**< Translation in mm below which the robot made no progress. */
    (Angle) stallAngle,            /**< Rotation below which the robot made no progress. */
  }),
});

/**
 * @class MazeRunMetricsProvider
 * Starts and measures the maze attempts of a simulation run.
 */
class MazeRunMetricsProvider : public MazeRunMetricsProviderBase
{
private:
  void update(MazeRunMetrics& mazeRunMetrics) override;

  /** Loads the maze and creates the tracker, once. @return Whether the tracker exists. */
  bool loadMaze();

  void print(const Maze::AttemptResult& result) const;

  std::unique_ptr<Maze::RunTracker> tracker;
  bool loadAttempted = false;
  bool doneAnnounced = false;
};
