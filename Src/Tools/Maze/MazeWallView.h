/**
 * @file Tools/Maze/MazeWallView.h
 *
 * Turns the floor points of the wall boundary into the look-ahead of the wall percept, with the semantics of
 * `Oracle::computeView`: the cells from the robot's cell along the gaze, `ahead` 0 is the robot's cell, the
 * edges relative to the gaze, and the walk stops after the first front wall or after `range` cells.
 * Free of representations and modules so that it can be unit tested. The output is a plain copy of the
 * fields of `WallPercept` (same order and values of the edge states as `WallPercept::Edge`).
 *
 * Frames: the boundary points are floor points in the robot frame in mm, the pose is the robot in the maze
 * frame (x east, y north, a rotation of 0 looks east, as `Oracle`). The gaze is the camera yaw relative to
 * the robot, snapped to a maze heading like the oracle does.
 *
 * Walls can only sit on the 500 mm grid. Each point is snapped to the nearest grid line (the nearer of the
 * nearest vertical and the nearest horizontal one) and counts as a support of the edge on that line it lies
 * along, but only within `snapToleranceMm` of the line; farther points are dropped. Near a grid vertex it is
 * unclear which line a point belongs to, so points closer than `vertexMarginMm` to a vertex are dropped, which
 * keeps a position error of up to 50 mm (the default margin leaves 10 mm) from crediting an edge with the
 * points of a perpendicular or a collinear wall. The edge states are:
 *  - wall: at least `minSupportPoints` points support the edge.
 *  - open: not a wall and the floor is visible past the edge: the line from the camera to some boundary point
 *    or clear point crosses the edge (not closer than `vertexMarginMm` to its ends) and the point lies at least
 *    `openClearanceMm` farther than the crossing. A point that far behind the crossing cannot lie on the
 *    wall of this edge, whatever the noise. A clear point is the farthest floor point of an image column
 *    without a wall boundary; it gives open evidence only, never wall support. The look-ahead never reaches
 *    beyond `range`, whatever the points: edges of farther cells are not evaluated.
 *  - unknown: otherwise, in particular for edges out of the field of view.
 * The edges between two cells of the goal block (`Maze::isGoal`) are never walls with `noGoalBlockWalls`, whatever the
 * points: the goal is marked on the floor and its edges give false wall points. They are open or unknown.
 */

#pragma once

#include "MazeGraph.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include <vector>

namespace Maze
{
  namespace WallView
  {
    struct Params
    {
      float snapToleranceMm = 150.f;  /**< Largest distance of a point to the grid line it is snapped to. */
      int minSupportPoints = 3;       /**< Points needed for a wall. */
      float openClearanceMm = 150.f;  /**< How much farther than the edge the floor has to be visible for an open edge. */
      float vertexMarginMm = 60.f;    /**< Points and crossings closer than this to a grid vertex are ignored, see above. */
      bool noGoalBlockWalls = true;   /**< The rules give no walls between the cells of the goal block: those edges are never walls. */
    };

    /** A wall segment in the robot frame (mm). */
    struct Segment
    {
      Vector2f start = Vector2f::Zero();
      Vector2f end = Vector2f::Zero();
    };

    /** The edges of one cell along the gaze. */
    struct Cell
    {
      int ahead = 0;                          /**< Cells between the robot's cell and this one. */
      EdgeState left = EdgeState::unknown;    /**< Left of the gaze. */
      EdgeState right = EdgeState::unknown;   /**< Right of the gaze. */
      EdgeState front = EdgeState::unknown;   /**< In front, in the gaze direction. */
      float distance = 0.f;                   /**< Robot to cell centre in mm. */
    };

    struct Result
    {
      std::vector<Segment> segments;  /**< Walls of the cells ahead (left, right, front per cell) in the robot frame. */
      float frontDistance = -1.f;     /**< Distance to the front wall of the last cell in mm, -1 if none. */
      float leftDistance = -1.f;      /**< Distance to the left wall of the robot's cell in mm, -1 if none. */
      float rightDistance = -1.f;     /**< Distance to the right wall of the robot's cell in mm, -1 if none. */
      int lookAheadTurns = 0;         /**< Gaze in quarter turns counterclockwise from the robot's x-axis, in [-1, 2]. */
      std::vector<Cell> cellsAhead;   /**< Nearest first. */
    };

    /**
     * Builds the look-ahead.
     * @param boundaryPointsInRobot Floor points of the wall boundary in the robot frame in mm.
     * @param clearPointsInRobot Farthest visible floor points of the columns without a boundary, robot frame in mm.
     * @param robotInMaze Robot pose in the maze frame (mm, rad).
     * @param cameraInRobot Camera position in the robot frame in mm (x, y).
     * @param gazeYawInRobot Direction the camera looks in, relative to the robot, in rad.
     * @param range Largest `ahead`; no cell beyond it is reported.
     * @return Whether the robot is inside the maze; otherwise `result` is empty.
     */
    [[nodiscard]] bool computeWallView(const std::vector<Vector2f>& boundaryPointsInRobot,
                                       const std::vector<Vector2f>& clearPointsInRobot, const Pose2f& robotInMaze,
                                       const Vector2f& cameraInRobot, float gazeYawInRobot, int range,
                                       const Params& params, Result& result);

    /** The look-ahead range: the standing one while the robot stands, otherwise the walking one (the torso pitches while walking). */
    [[nodiscard]] constexpr int lookAheadRange(bool standing, int rangeWalking, int rangeStanding)
    {
      return standing ? rangeStanding : rangeWalking;
    }
  }
}
