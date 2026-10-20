/**
 * @file WallPercept.h
 *
 * The maze walls that the robot sees in the current frame: the wall segments, the distances to the
 * nearest walls and a look-ahead along the gaze, cell by cell. In simulation the percept comes from
 * OracledMazePerceptsProvider, on the robot from the wall perceptor.
 *
 * Everything is relative to the robot. The perceptor cannot know absolute maze headings, so the
 * look-ahead is expressed in the direction of the gaze: "left", "right" and "front" are relative to
 * that direction, and the mapper turns them into maze headings with the robot heading it estimates.
 */

#pragma once

#include "Math/Eigen.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include <vector>

STREAMABLE(WallPercept,
{
  /** What is known about one wall slot. */
  ENUM(Edge,
  {,
    unknown, /**< Not observed. */
    open,    /**< Observed to be free. */
    wall,    /**< Observed to be a wall. */
  });

  /** A wall segment in the robot frame (mm). */
  STREAMABLE(Segment,
  {,
    (Vector2f)(Vector2f::Zero()) start,
    (Vector2f)(Vector2f::Zero()) end,
  });

  /** The walls of one cell along the gaze. */
  STREAMABLE(CellAhead,
  {,
    (int)(0) ahead,               /**< Cells between the robot's cell and this one; 0 is the cell the robot stands in. */
    (Edge)(unknown) left,         /**< Wall on the left of the gaze direction. */
    (Edge)(unknown) right,        /**< Wall on the right of the gaze direction. */
    (Edge)(unknown) front,        /**< Wall in front of the cell, in the gaze direction. */
    (float)(0.f) distance,        /**< Distance from the robot to the centre of the cell in mm. */
  });

  /** The method draws the percept to the field view (relative to the robot). */
  void draw() const;

  /** Verifies that this percept contains valid values. */
  void verify() const,

  (std::vector<Segment>) segments,   /**< Seen wall segments in the robot frame. */
  (float)(-1.f) frontDistance,       /**< Distance to the wall in front of the gaze in mm; -1 if no wall is seen. */
  (float)(-1.f) leftDistance,        /**< Distance to the wall on the left of the gaze in mm; -1 if no wall is seen. */
  (float)(-1.f) rightDistance,       /**< Distance to the wall on the right of the gaze in mm; -1 if no wall is seen. */
  (int)(0) lookAheadTurns,           /**< Gaze direction of `cellsAhead` in quarter turns counterclockwise from the robot's x-axis. */
  (std::vector<CellAhead>) cellsAhead, /**< The look-ahead, nearest cell first; ends at the first front wall or the maximum range. */
  (std::vector<Vector2f>) boundaryPoints, /**< Floor points of the wall boundary in the robot frame in mm; filled by the wall perceptor for the evaluation, empty for the oracle. */
  (unsigned)(0) timestamp,           /**< Time of the frame in ms. */
});
