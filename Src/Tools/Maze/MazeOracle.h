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
 *
 * AprilTags do not use the corridor: every wall face the layout tags is a candidate within a maximum
 * distance in mm, independent of `maxRange`, see `visibleTags`.
 */

#pragma once

#include "MazeGraph.h"
#include "MazeTagLayout.h"
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

    /**
     * Thickness of a wall in mm. A wall blocks the view as a segment of its 500 mm centre line that is
     * extended by half of this at both ends, so a ray that grazes the end of a wall (its corner post) is blocked.
     */
    constexpr float wallThicknessMm = 15.f;

    /** A tag the camera sees. */
    struct VisibleTag
    {
      int cell;               /**< Cell the tag belongs to, which is also the tag id. */
      Heading face;           /**< Face of that cell the tag is on. */
      Pose3f tagInRobot;      /**< Pose of the tag in the robot frame, see `observeTag`. */
    };

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
     * The robot pose in the maze frame.
     * @param groundTruthRobotPose The robot pose as the simulator reports it.
     * @param mazePoseOnField Pose of the maze frame (origin in its south-west corner, x east, y north)
     *                        in the frame of `groundTruthRobotPose`.
     */
    [[nodiscard]] Pose2f robotInMaze(const Pose2f& groundTruthRobotPose, const Pose2f& mazePoseOnField);

    /**
     * Fills `view` for a robot at `robotInMaze`.
     * @param maxRange Cells ahead of the robot's cell that are walked.
     * @return Whether the robot is inside the maze.
     */
    [[nodiscard]] bool computeView(const MazeGraph& maze, const Pose2f& robotInMaze, const Camera& camera, int maxRange, View& view);

    /** Maze frame to robot frame (rotation and translation only). */
    [[nodiscard]] Vector2f toRobot(const View& view, const Vector2f& mazePoint);

    /**
     * Whether the tag on `face` of `cell` is within reach: at most `maxTagDistance` mm from the camera, with the whole black square of
     * width `tagSize` (along the wall) inside the horizontal field of view, and facing the camera by at most
     * `maxTagViewAngle`. Whether the layout puts a tag there and whether a wall hides it is not checked.
     * @param maxTagDistance Largest distance in mm from the camera to the tag centre.
     * @param tagSize Edge of the black square in mm; 0 checks only the tag centre.
     * @param tagInRobot Receives the pose of the tag: x to the right and y up when looking at it, z along
     *                   the wall normal into the cell, at height `centreHeight` in mm.
     */
    [[nodiscard]] bool observeTag(const View& view, const Camera& camera, float maxTagDistance, float maxTagViewAngle,
                                  int cell, Heading face, float centreHeight, Pose3f& tagInRobot, float tagSize = 0.f);

    /**
     * Whether no wall of `maze` blocks the 2D line from `from` to `to` (maze frame). The edge `face` of
     * `cell` (the wall the target lies on) is ignored. See `wallThicknessMm` for grazing rays.
     */
    [[nodiscard]] bool lineOfSightClear(const MazeGraph& maze, const Vector2f& from, const Vector2f& to, int cell, Heading face);

    /**
     * Every tag the camera sees. A tag on a wall face is seen when the layout tags it and, all at once,
     * it is within reach (`observeTag`, the whole tag inside the field of view) and no wall blocks the line
     * from the camera to the tag centre, nor to either side edge of the black square. The look-ahead corridor
     * of `view` plays no role: tags beside or beyond the gaze corridor are seen if they are visible.
     */
    [[nodiscard]] std::vector<VisibleTag> visibleTags(const MazeGraph& maze, const MazeTagLayout& layout, const View& view,
                                                      const Camera& camera, float maxTagDistance, float maxTagViewAngle);
  }
}
