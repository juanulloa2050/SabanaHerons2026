/**
 * @file WallPerceptEvaluator.h
 *
 * Simulation only: scores the percepts of the real WallPerceptor against the true maze. It requires the Upper percept
 * directly, so the perceptor is scored even when the mapper gets the oracle percept in `WallPercept`. The active range of a frame
 * is the one the perceptor reports in `lookAheadRange`: it reads `MotionInfo` in the Upper thread, which can differ from the
 * Cognition one in the frames around a start or a stop, and (c) must not count that. The metrics and
 * their definitions are in Tools/Maze/MazeWallScore.h.
 *
 * Every new Upper percept (a new timestamp, with at least one cell) is one frame: the ground truth robot pose is
 * converted to the maze frame like the oracle does (`mazePoseOnField`, the same values as
 * oracledMazePerceptsProvider.cfg) and compared with the maze file. The believed cell comes from `RobotPose`.
 * Output: every `summaryIntervalMs` and when the module is destroyed, as text (`OUTPUT_TEXT`) and, for headless
 * runs, on the standard error stream lines that start with "WallPerceptEvaluator:".
 *
 * It can also start the exploration (`startTapAfterMs` after the game state became playing): the head tap of HandleMaze
 * is a debug request that is consumed once, so a `dr` line in the scene comes too early, before the robot is ready.
 *
 * The scene switches it on with `for Cognition mr WallPerceptScore WallPerceptEvaluator`, see Config/Scenes/MazeEval.con.
 */

#pragma once

#include "Framework/Module.h"
#include "Modules/Infrastructure/InterThreadProviders/PerceptionProviders.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/MotionControl/MotionInfo.h"
#include "Representations/Perception/MazePercepts/WallPerceptScore.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeWallScore.h"
#include <memory>
#include <string>

MODULE(WallPerceptEvaluator,
{,
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(GroundTruthRobotPose),
  REQUIRES(MotionInfo),
  REQUIRES(RobotPose),
  REQUIRES(UpperWallPercept),
  PROVIDES(WallPerceptScore),
  LOADS_PARAMETERS(
  {,
    (std::string) mazeFile,        /**< The maze, relative to the repository root; the same as in oracledMazePerceptsProvider.cfg. */
    (Pose2f) mazePoseOnField,      /**< Pose of the maze frame in the frame of the ground truth robot pose; the same as in the oracle cfg. */
    (int) summaryIntervalMs,       /**< Time between two summaries. */
    (int) tapRepeatMs,             /**< Taps again every this long after the first tap (0: only once): the tap at the goal starts the return, the one at the start the speed run. */
    (int) startTapAfterMs,         /**< Taps the head front once this long after the game state became playing, to start the exploration (0: never). A tap in the .con is lost: it comes before the maze behaviour is ready. */
  }),
});

/**
 * @class WallPerceptEvaluator
 * Scores the wall percept in simulation.
 */
class WallPerceptEvaluator : public WallPerceptEvaluatorBase
{
public:
  ~WallPerceptEvaluator();

private:
  void update(WallPerceptScore& wallPerceptScore) override;

  /** Loads the maze, trying again every `summaryIntervalMs` after a failure. @return Whether it is available. */
  bool loadMaze();

  void print(const char* title);

  Maze::MazeDescription maze;
  std::unique_ptr<Maze::WallScore::Scorer> scorer;
  bool mazeLoaded = false;
  bool loadAttempted = false;
  unsigned lastLoadAttempt = 0;
  std::string loadError;  /**< Why the last load failed, empty after a success. */
  unsigned lastTimestamp = 0;
  unsigned lastSummaryTime = 0;
  int emptyPercepts = 0;  /**< New percepts without a cell (invalid image or camera), not scored. */
  int framesSeen = 0;
  unsigned playingSince = 0;
  bool wasPlaying = false;
  bool tapped = false;
  unsigned lastTapTime = 0;
};
