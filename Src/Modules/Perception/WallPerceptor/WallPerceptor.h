/**
 * @file WallPerceptor.h
 *
 * Sees the maze walls in the grey image of the upper camera and publishes them as WallPercept, with the
 * semantics of the simulation oracle: near walls and a look-ahead along the gaze, cell by cell.
 *
 * Steps (the rules are in Tools/Maze/MazeWallScan.h and Tools/Maze/MazeWallView.h, both unit tested):
 *  1. The image columns are scanned for the floor-to-wall boundary, a brightness step and not a colour.
 *  2. Each boundary pixel is projected to the floor with `Transformation::imageToRobot`. A column without a
 *     boundary sees floor up to the horizon row, whose floor point gives open evidence (a clear point).
 *  3. The points are snapped to the 500 mm grid with the robot pose and become wall and open edges.
 *
 * Frames. `RobotPose` comes from the MazeLocalizer in the Cognition thread and is in the maze frame (mm, x east,
 * y north, origin at the outer bottom-left corner), the frame that `Tools/Maze/MazeOracle.h` and the mapper use,
 * so no conversion is needed. It is read across threads and may be one frame old, which is far below the snap
 * tolerance. The gaze yaw is the direction of the camera x-axis in the robot frame, as in the oracle.
 *
 * Clear columns. A column without a boundary only counts as free floor if its bottom rows look like the floor, see
 * `Maze::WallScan::FloorModel` (learning, tracking, ageing). The model is reset exactly when the MazeLocalizer
 * resets its pose: on the first frame and whenever the robot becomes active again (playing, not penalized, not
 * picked up, `Maze::isActive`); until a lesson no column is clear.
 *
 * The boundary has no fixed polarity: a step in either direction counts, see Tools/Maze/MazeWallScan.h.
 *
 * The camera has to be valid (stable motion); otherwise the percept is empty. The look-ahead range is
 * `lookAheadRangeStanding` while the robot stands and `lookAheadRangeWalking` otherwise, because the torso
 * pitches while walking.
 *
 * Debugging in SimRobot: the image drawing `module:WallPerceptor:boundary` (green: boundary, red: clear
 * column at the horizon row, yellow: column without a boundary that is not taken as clear) and the field drawing `module:WallPerceptor:points`.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/MotionControl/MotionInfo.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/ImagePreprocessing/ECImage.h"
#include "Representations/Perception/MazePercepts/WallPercept.h"
#include "Representations/Sensing/FallDownState.h"
#include "Tools/Maze/MazeSession.h"
#include "Tools/Maze/MazeWallScan.h"

MODULE(WallPerceptor,
{,
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  REQUIRES(ECImage),
  REQUIRES(FallDownState),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(MotionInfo),
  REQUIRES(RobotPose),
  PROVIDES(WallPercept),
  LOADS_PARAMETERS(
  {,
    // Boundary scan, see Tools/Maze/MazeWallScan.h
    (int) columnStep,                 /**< Distance between the scanned columns in pixels. */
    (int) smoothRows,                 /**< Rows averaged on each side of a candidate boundary. */
    (int) columnHalfWidth,            /**< Columns on each side of the scanned one that are averaged. */
    (float) minGradient,              /**< Least brightness step of a boundary in grey levels. */
    (float) maxFloorDeviation,        /**< Largest difference in grey levels between the bottom rows of a column, or the rows below a boundary, and the floor reference. */
    (float) minWallContrast,          /**< Least brightness of the wall above the floor in grey levels. */
    (float) minWallCheckHeightMm,     /**< Height up to which the column has to stay wall. */
    (float) minWallRowFraction,       /**< Part of the rows up to that height that keep the wall contrast. */
    (int) maxFloorRunRows,            /**< Longest run of floor-like rows allowed above a boundary within the check height. */
    (int) minCheckRows,               /**< Rows that have to remain for the check after cutting at the image top. */
    (int) floorReferenceRows,         /**< Bottom rows that give the floor reference. */
    (float) maxScanDistanceMm,        /**< Floor farther than this from the camera is not searched; sets the horizon row. */
    (float) floorTolerance,           /**< Largest difference in grey levels between the bottom of a clear column and the learned floor. */
    (float) floorTrackingRate,        /**< Part of the difference to a new floor reference that the learned floor follows. */
    (int) floorMaxAgeMs,              /**< Time without a lesson after which the learned floor is forgotten. */
    // Grid snap, see Tools/Maze/MazeWallView.h
    (float) snapToleranceMm,          /**< Largest distance of a point to the grid line it is snapped to. */
    (int) minSupportPoints,           /**< Points needed for a wall. */
    (float) openClearanceMm,          /**< How much farther than an edge the floor has to be visible for an open edge. */
    (float) vertexMarginMm,           /**< Points closer than this to a grid vertex are ignored. */
    (int) lookAheadRangeWalking,      /**< Cells ahead that are reported while walking. */
    (int) lookAheadRangeStanding,     /**< Cells ahead that are reported while standing. */
  }),
});

/**
 * @class WallPerceptor
 * Finds the maze walls in the image.
 */
class WallPerceptor : public WallPerceptorBase
{
  void update(WallPercept& wallPercept) override;

  /** The topmost image row that sees floor within `maxScanDistanceMm` of the camera; -1 if the floor is not visible. */
  int horizonRow() const;

  Maze::WallScan::FloorModel floorModel; /**< Learned floor brightness, see `Maze::WallScan::FloorModel`. */
  Maze::PlayingTracker tracker;          /**< The same reset signal as the MazeLocalizer: becoming active again. */
};
