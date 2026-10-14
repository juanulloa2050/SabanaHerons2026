/**
 * @file Tools/Maze/MazeOracle.h
 *
 * The geometry behind the simulated maze perception: where the robot is in the maze, which cells it
 * looks along and which wall tags it sees. Free of representations, modules and randomness (noise and
 * dropout stay in OracledMazePerceptsProvider) so that it can be unit tested.
 *
 * Frames: maze frame in mm, x east, y north, a rotation of 0 looks east. The robot frame is the usual
 * one (x forward, y left). Occlusion is approximated by a look-ahead corridor: from the robot's cell,
 * along the gaze snapped to the nearest maze heading, up to `maxRange` cells and not beyond a front
 * wall. The vertical field of view is ignored.
 */

#pragma once

#include "MazeGraph.h"
#include "Math/Angle.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include "Math/Pose3f.h"
#include <vector>

namespace Maze
{
  namespace Oracle
  {
    constexpr float halfCellMm = cellSizeMm / 2.f;

    /** The camera as far as the oracle needs it. */
    struct Camera
    {
      Vector2f positionInRobot;   /**< Camera position in the robot frame in mm (x, y). */
      float yawInRobot;           /**< Direction the camera looks in, in the robot frame in rad. */
      float openingAngleWidth;    /**< Horizontal opening angle in rad. */
    };

    /** One cell of the look-ahead. */
    struct LookAheadCell
    {
      int ahead;                                /**< Cells between the robot's cell and this one. */
      int row, col;                             /**< Position of the cell in the maze. */
      EdgeState left, right, front;             /**< Edges relative to the gaze direction. */
    };

    /** The robot pose in the maze frame and the gaze it looks along. */
    struct View
    {
      Vector2f robot;                  /**< Robot position in the maze frame in mm. */
      float robotRotation;             /**< Robot heading in the maze frame in rad, 0 is east. */
      Vector2f camera;                 /**< Camera position in the maze frame in mm. */
      int row, col;                    /**< The robot's cell. */
      Heading gaze;                    /**< The gaze snapped to a maze heading. */
      std::vector<LookAheadCell> cells; /**< From the robot's cell along the gaze. */
    };

    /** Unit vector of a heading in the maze frame (x east, y north). */
    [[nodiscard]] Vector2f directionOf(Heading heading);

    /** The heading closest to an angle in the maze frame (0 is east, counterclockwise). */
    [[nodiscard]] Heading snapToHeading(float angle);

    /** The angle of a heading in the maze frame. */
    [[nodiscard]] float angleOf(Heading heading);

    /** Centre of a cell in the maze frame in mm. */
    [[nodiscard]] Vector2f cellCentreOf(int row, int col);

    /** `vector` rotated counterclockwise by `angle`. */
    [[nodiscard]] Vector2f rotated(const Vector2f& vector, float angle);

    /**
     * The cell that contains a point of the maze frame.
     * @return Whether the point is inside the maze (points on the south or west border count as outside).
     */
    [[nodiscard]] bool cellContaining(const Vector2f& mazePoint, int& row, int& col);

    /**
     * Fills `view` for a robot at `robotInMaze`.
     * @param maxRange Cells ahead of the robot's cell that are walked.
     * @return Whether the robot is inside the maze.
     */
    [[nodiscard]] bool computeView(const MazeGraph& maze, const Pose2f& robotInMaze, const Camera& camera, int maxRange, View& view);

    /** Maze frame to robot frame (rotation and translation only). */
    [[nodiscard]] Vector2f toRobot(const View& view, const Vector2f& mazePoint);

    /**
     * Whether the tag on `face` of `cell` is seen: close enough, inside the horizontal field of view and
     * facing the camera by at most `maxTagViewAngle`. Whether the layout puts a tag there is not checked.
     * @param tagInRobot Receives the pose of the tag: x to the right and y up when looking at it, z along
     *                   the wall normal into the cell, at height `centreHeight` in mm.
     */
    [[nodiscard]] bool observeTag(const View& view, const Camera& camera, int maxRange, float maxTagViewAngle,
                                  int cell, Heading face, float centreHeight, Pose3f& tagInRobot);
  }
}
