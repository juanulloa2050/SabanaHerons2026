/**
 * @file AprilTagPercept.h
 *
 * The AprilTags (tag36h11) that the robot sees in the current frame. The id of a tag is the index
 * of the maze cell it marks (row * 8 + column). In simulation the percept comes from
 * OracledMazePerceptsProvider, on the robot from the AprilTag detector.
 */

#pragma once

#include "Math/Eigen.h"
#include "Math/Pose3f.h"
#include "Streaming/AutoStreamable.h"
#include <vector>

STREAMABLE(AprilTagPercept,
{
  /** One detected tag. */
  STREAMABLE(Detection,
  {,
    (int)(-1) id,                        /**< Tag id, which is the cell index; -1 if invalid. */
    (std::vector<Vector2f>) corners,     /**< The four corners in the image in pixels; empty for the oracle. */
    (Pose3f) tagInRobot,                 /**< Tag centre in the robot frame (mm). The z-axis is the tag normal pointing out of the wall, the x-axis points right and the y-axis up when looking at the tag. */
    (float)(0.f) decisionMargin,         /**< Confidence of the detector; larger is better. */
  });

  /** The method draws the percept to the field view (relative to the robot). */
  void draw() const;

  /** Verifies that this percept contains valid values. */
  void verify() const,

  (std::vector<Detection>) detections,   /**< The tags seen in this frame; empty if none. */
  (unsigned)(0) timestamp,               /**< Time of the frame in ms. */
});
