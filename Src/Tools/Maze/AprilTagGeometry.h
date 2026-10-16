/**
 * @file Tools/Maze/AprilTagGeometry.h
 *
 * The pure geometry of the AprilTag detector: the conversion of a tag pose from the apriltag
 * conventions to the AprilTagPercept conventions and the plausibility filters for a detection.
 * Free of representations and module parameters so that it can be unit tested.
 *
 * Frames. The apriltag library returns the pose of the tag in the camera optical frame
 * (x right, y down, z forward) and the tag frame has x right, y down and z into the wall.
 * B-Human's camera frame has x forward, y left and z up. The pose is converted in three steps:
 *   1. optical -> B-Human camera: (x, y, z)optical = (-y, -z, x)camera, i.e. the matrix
 *      [0 0 1; -1 0 0; 0 -1 0] applied to the optical vector;
 *   2. tag frame -> AprilTagPercept tag frame (z out of the wall, x right, y up when looking at the
 *      tag): a rotation of pi around the tag's x-axis, diag(1, -1, -1);
 *   3. camera -> robot with the CameraMatrix. Millimetres everywhere after the conversion.
 */

#pragma once

#include "Math/Eigen.h"
#include "Math/Pose3f.h"

namespace Maze
{
  namespace AprilTagGeometry
  {
    /** The limits of the detection filters. */
    struct Filter
    {
      float minDecisionMargin = 30.f; /**< Detections with a smaller decision margin are dropped. */
      int maxHamming = 0;             /**< Detections with more corrected bits are dropped. */
      float sizeTolerance = 0.5f;     /**< Largest relative difference between the distance from the pose and the distance from the apparent size. */
      float maxDistance = 3000.f;     /**< Tags farther away than this are dropped (mm). */
      int maxId = 63;                 /**< Tags with a larger id are dropped. */
    };

    /**
     * Converts a tag pose from the apriltag conventions to the AprilTagPercept conventions.
     * @param rotation Rotation of the tag in the camera optical frame (apriltag convention).
     * @param translation Position of the tag in the camera optical frame in metres.
     * @param cameraMatrix Pose of the B-Human camera frame in the robot frame (mm).
     * @return The pose of the tag in the robot frame in mm, with z the normal out of the wall.
     */
    [[nodiscard]] Pose3f toRobotFrame(const Matrix3f& rotation, const Vector3f& translation, const Pose3f& cameraMatrix);

    /**
     * Checks what the decoder reports about a tag, before its pose is estimated.
     * @return Whether id, corrected bits and decision margin are acceptable.
     */
    [[nodiscard]] bool acceptsDecoding(int id, int hamming, float decisionMargin, const Filter& filter);

    /**
     * Checks that the distance from the pose matches the distance that the apparent size of the tag implies.
     * A slanted tag has a shorter apparent size than a frontal one, hence the tolerance.
     * @param distance Distance of the tag from the camera according to the pose (mm).
     * @param longestEdge Longest edge of the tag in the image (pixels).
     * @param focalLength Focal length of the camera (pixels).
     * @param tagSize Edge of the tag (mm).
     * @return Whether the tag is near enough and its distance is consistent with its apparent size.
     */
    [[nodiscard]] bool acceptsDistance(float distance, float longestEdge, float focalLength, float tagSize, const Filter& filter);
  }
}
