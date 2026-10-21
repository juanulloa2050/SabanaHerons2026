/**
 * @file MazeMapper.h
 *
 * Builds the believed wall map of the maze from the wall percept and the AprilTags, using the cell pose
 * of MazeLocalizer (Tools/Maze/MazeMapFusion.h holds the evidence model and its unit tests). The fusion
 * is a member of this module, so the map lives as long as the process: a pick-up and Standby do not
 * clear it. Only the sit gesture does (MZ-042), which is not part of this module.
 *
 * Attempts: the first transition into playing starts attempt 1; every later one starts the next
 * attempt. The map and the visited cells are kept across attempts.
 *
 * The oracle (and later the camera) describes the cell the robot is truly in, the mapper writes it on the estimated one. Near a cell
 * boundary these can differ, so the look-ahead is skipped there (tags are not: the id names the cell). Every edge also takes at most one
 * observation per `minObservationInterval`, so a few repeated frames cannot flip a decided wall.
 *
 * The map only learns while the robot is active (the game state is playing and the robot is neither penalized nor picked up, see
 * `Maze::isActive`). Outside of it the robot may be carried, so the localizer pose would not match what the camera sees.
 *
 * Start: `handleMaze.cfg` gives the start cell and heading (`start.cell` 0, 7, 56 or 63), or -1 to deduce them. The
 * deduction and the re-anchoring of the map are in `Maze::StartAnchoredMap`, shared with the offline simulation:
 * until the start is known the map and the localizer use the frame of the first alive hypothesis, `MazeStart` tells
 * which. When the working hypothesis changes the map is rebuilt from the logged observations and `changeCounter`
 * grows, so the planner replans.
 *
 * Blocked walls: when the robot walked into a wall the map did not know, the behaviour publishes the edge (`MazeBlockedEdge`, in the frame of
 * the working hypothesis) and the mapper makes it a wall that no open evidence can overturn (`StartAnchoredMap::observeBlocked`, logged
 * like a tag). A block published under another hypothesis than the current one is dropped with a warning.
 *
 * Wall percept: the look-ahead of `WallPercept` is relative to the gaze, which is `lookAheadTurns`
 * quarter turns counterclockwise from the robot's x-axis. The gaze heading is the robot's cell heading
 * turned that way (`Maze::gazeHeading`). Tags: the face a tag sits on is chosen like in the localizer
 * (`Maze::chooseFace` with the localized rotation); a decoded tag means that face is a wall.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/MazeBlockedEdge.h"
#include "Representations/Modeling/MazeCellPose.h"
#include "Representations/Modeling/MazeMap.h"
#include "Representations/Modeling/MazeStart.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/Perception/MazePercepts/AprilTagPercept.h"
#include "Representations/Perception/MazePercepts/WallPercept.h"
#include "Representations/Sensing/FallDownState.h"
#include "Tools/Maze/MazeBlockedEdgeChannel.h"
#include "Tools/Maze/MazeLocalization.h"
#include "Tools/Maze/MazeSession.h"
#include "Tools/Maze/MazeStartAnchoredMap.h"
#include "Tools/Maze/MazeStartConfig.h"
#include <vector>

MODULE(MazeMapper,
{,
  USES(MazeBlockedEdge), // Provided by the behaviour, which runs after the mapper: the block of the previous frame.
  REQUIRES(AprilTagPercept),
  REQUIRES(FallDownState),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(MazeCellPose),
  REQUIRES(RobotPose),
  REQUIRES(WallPercept),
  PROVIDES(MazeMap),
  PROVIDES(MazeStart),
  LOADS_PARAMETERS(
  {,
    (float) threshold,                                 /**< Evidence needed to decide an edge or to flip it; 3 observations of full weight. */
    (float) fullWeightDistance,                        /**< Wall observations up to this distance (mm) have full weight. */
    (float) zeroWeightDistance,                        /**< Wall observations from this distance (mm) have no weight; linear in between. */
    (float) tagWeight,                                 /**< Weight of a decoded tag at any distance. */
    (float) minObservationInterval,                    /**< An edge accepts at most one observation per this many ms; Cognition repeats the scene at 60 Hz, which is not independent evidence. */
    (float) maxOffsetFromCentre,                       /**< The wall look-ahead is used only while the estimated position is this close (mm, per axis) to the centre of its cell; nearer to a boundary the cell is ambiguous. */
    (Angle) maxFaceAngleError,                         /**< Largest angle between the measured and the expected tag normal; keep equal to the one of MazeLocalizer. */
    (RobotPose::LocalizationQuality) worstAcceptedQuality, /**< Observations are used only while the localizer quality is at least this good; poor accepts everything. */
  }),
});

/**
 * @class MazeMapper
 * The wall map of the maze.
 */
class MazeMapper : public MazeMapperBase
{
public:
  MazeMapper();

private:
  void update(MazeMap& mazeMap) override;
  void update(MazeStart& mazeStart) override;

  /** Runs the mapper once per frame; both `update` functions read its result. */
  void step();

  /** Adds the wall percept to the map. @return Number of edges that changed. */
  int observeWalls();

  /** Adds the tags to the map. @return Number of edges that changed. */
  int observeTags();

  /** Maps the edge that stopped a walk as a wall for good, once per block. @return Number of edges that changed. */
  int observeBlocked();

  /** Reads `start` of handleMaze.cfg and gives the start to the map if it is set. */
  void loadStart();

  Maze::StartAnchoredMap anchored;  /**< The map and the start deduction; persist for the process. */
  bool stepped = false;             /**< `step` ran for `lastStepTime`. */
  unsigned lastStepTime = 0;
  Maze::TimestampFilter wallFilter;
  Maze::TimestampFilter tagFilter;
  Maze::BlockedEdgeReceiver blockedReceiver; /**< Acts on each block of the behaviour once. */
  Maze::PlayingTracker session;     /**< Decides when a new attempt starts. */
  unsigned changeCounter = 0;
  unsigned lastChangeTime = 0;
  std::vector<int> lastObservedCells; /**< Cells of the last look-ahead, for the drawing. */
  bool lookAheadSkipped = false;      /**< The last new wall percept was skipped because the estimate is near a cell boundary. */
};
