/**
 * @file MazeCellPose.h
 *
 * Where the robot is in the maze, as a cell: row, column and the maze heading it faces, together with
 * how the heading was obtained and how much the tags had to correct it. Provided by MazeLocalizer
 * next to the metric `RobotPose`, which lives in the same maze frame (mm, x east, y north, origin at
 * the outer bottom-left corner, rotation counterclockwise from east).
 *
 * The heading is the pose rotation snapped to the nearest multiple of 90 degrees; the cell is the cell
 * that contains the pose (clamped to the nearest cell if the pose is outside the maze).
 */

#pragma once

#include "Representations/Modeling/RobotPose.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"

STREAMABLE(MazeCellPose,
{
  /** Clockwise from north, same order as `Maze::Heading`. */
  ENUM(Heading,
  {,
    north,
    east,
    south,
    west,
  });

  /** Where the rotation of the pose comes from; the values match `Maze::HeadingSource`. */
  ENUM(HeadingSource,
  {,
    odometry, /**< Integrated odometry rotation. */
    gyro,     /**< Gyro yaw minus the gyro offset. */
    tag,      /**< Corrected by a tag since the last prediction. */
  });

  /** The cell index (row * 8 + column), which is also the AprilTag id of the cell. */
  int cell() const { return row * 8 + col; }

  /** Draws the cell and the heading in the maze frame. */
  void draw() const,

  (int)(0) row,                                       /**< Row of the cell, 0 is the top (north) row. */
  (int)(0) col,                                       /**< Column of the cell, 0 is the left (west) column. */
  (Heading)(south) heading,                           /**< The maze heading the robot faces. */
  (bool)(false) insideMaze,                           /**< Whether the pose is inside the maze; if not, row and col are clamped. */
  (RobotPose::LocalizationQuality)(RobotPose::poor) quality,     /**< How much the pose can be trusted; the same value as `RobotPose::quality`. */
  (HeadingSource)(odometry) headingSource,            /**< Where the heading comes from. */
  (float)(0.f) headingDrift,                          /**< Sum of the absolute heading corrections by tags since the last reset (rad); a measure of the gyro drift. */
  (unsigned)(0) lastTagFixTime,                       /**< Time of the last accepted tag fix, or of the last reset (ms). */
  (unsigned)(0) tagFixes,                             /**< Accepted tag fixes since the last reset. */
  (unsigned)(0) tagRejections,                        /**< Detections since the last reset that were not used (invalid id, no matching face, wrong normal). */
  (unsigned)(0) resets,                               /**< How often the pose was put back to the start since the process began. */
});
