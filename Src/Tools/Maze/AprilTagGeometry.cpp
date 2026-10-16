/**
 * @file Tools/Maze/AprilTagGeometry.cpp
 *
 * Implementation of the AprilTag detector geometry, see the header for the frames.
 */

#include "AprilTagGeometry.h"
#include <algorithm>
#include <cmath>

namespace Maze
{
  namespace AprilTagGeometry
  {
    Pose3f toRobotFrame(const Matrix3f& rotation, const Vector3f& translation, const Pose3f& cameraMatrix)
    {
      // Optical frame (x right, y down, z forward) -> B-Human camera frame (x forward, y left, z up).
      static const Matrix3f opticalToCamera = (Matrix3f() << 0.f, 0.f, 1.f,
                                                             -1.f, 0.f, 0.f,
                                                             0.f, -1.f, 0.f).finished();
      // apriltag tag frame (x right, y down, z into the wall) -> percept tag frame (x right, y up, z out of the wall).
      static const Matrix3f tagToPercept = Vector3f(1.f, -1.f, -1.f).asDiagonal();

      const Pose3f tagInCamera(RotationMatrix(opticalToCamera * rotation * tagToPercept), opticalToCamera * translation * 1000.f);
      return cameraMatrix * tagInCamera;
    }

    bool acceptsDecoding(int id, int hamming, float decisionMargin, const Filter& filter)
    {
      return id >= 0 && id <= filter.maxId && hamming <= filter.maxHamming && decisionMargin >= filter.minDecisionMargin;
    }

    bool acceptsDistance(float distance, float longestEdge, float focalLength, float tagSize, const Filter& filter)
    {
      const float expectedDistance = focalLength * tagSize / std::max(longestEdge, 1.f);
      return distance <= filter.maxDistance && std::abs(distance / expectedDistance - 1.f) <= filter.sizeTolerance;
    }
  }
}
