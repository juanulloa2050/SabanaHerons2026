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
 * The map only learns while the game state is playing. Outside of it the robot may be carried, so the
 * localizer pose would not match what the camera sees.
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
#include "Representations/Modeling/MazeCellPose.h"
#include "Representations/Modeling/MazeMap.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/Perception/MazePercepts/AprilTagPercept.h"
#include "Representations/Perception/MazePercepts/WallPercept.h"
#include "Tools/Maze/MazeLocalization.h"
#include "Tools/Maze/MazeMapFusion.h"
#include "Tools/Maze/MazeSession.h"
#include "Tools/Maze/MazeTagLayout.h"
#include <vector>

MODULE(MazeMapper,
{,
  REQUIRES(AprilTagPercept),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(MazeCellPose),
  REQUIRES(RobotPose),
  REQUIRES(WallPercept),
  PROVIDES(MazeMap),
  LOADS_PARAMETERS(
  {,
    (int) startCell,                                   /**< Cell the attempts start in. Replaced by `start.cell` of MZ-024. */
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

  /** Adds the wall percept to the map. @return Number of edges that changed. */
  int observeWalls();

  /** Adds the tags to the map. @return Number of edges that changed. */
  int observeTags();

  Maze::MazeMapFusion fusion;       /**< The map; persists for the process. */
  Maze::MazeTagLayout layout;
  Maze::TimestampFilter wallFilter;
  Maze::TimestampFilter tagFilter;
  Maze::PlayingTracker session;     /**< Decides when a new attempt starts. */
  unsigned changeCounter = 0;
  unsigned lastChangeTime = 0;
  std::vector<int> lastObservedCells; /**< Cells of the last look-ahead, for the drawing. */
  bool lookAheadSkipped = false;      /**< The last new wall percept was skipped because the estimate is near a cell boundary. */
};
