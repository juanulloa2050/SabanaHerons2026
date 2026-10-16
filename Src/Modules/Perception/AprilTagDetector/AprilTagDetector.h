/**
 * @file AprilTagDetector.h
 *
 * Detects the maze's tag36h11 AprilTags in the grayscale image of the upper camera and publishes
 * them as AprilTagPercept: id, the four corners in the image, the decision margin and the pose of
 * the tag in the robot frame.
 *
 * Frames. The apriltag library returns the pose of the tag in the camera optical frame
 * (x right, y down, z forward) and the tag frame has x right, y down and z into the wall.
 * B-Human's camera frame has x forward, y left and z up. The pose is converted in three steps:
 *   1. optical -> B-Human camera: (x, y, z)optical = (-y, -z, x)camera, i.e. the matrix
 *      [0 0 1; -1 0 0; 0 -1 0] applied to the optical vector;
 *   2. tag frame -> AprilTagPercept tag frame (z out of the wall, x right, y up when looking at the
 *      tag): a rotation of pi around the tag's x-axis, diag(1, -1, -1);
 *   3. camera -> robot with the CameraMatrix. Millimetres everywhere after the conversion.
 *
 * The image has the resolution of CameraInfo (the ECImage is full size), so the intrinsics of
 * CameraInfo are used unscaled. Lens distortion and the rolling shutter are not corrected.
 *
 * Ids: tag36h11 has 587 codes but the maze only uses 0..63 (cell index = row * 8 + column). A
 * detection with a larger id is a false positive and is dropped (`maxId`).
 *
 * Debugging in SimRobot (RemoteRobot scene): `call Includes/AprilTagViews`.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/ImagePreprocessing/ECImage.h"
#include "Representations/Perception/MazePercepts/AprilTagPercept.h"
#include "Tools/Maze/MazeTagLayout.h"

extern "C"
{
  struct apriltag_detector;
  struct apriltag_family;
}

MODULE(AprilTagDetector,
{,
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  REQUIRES(ECImage),
  REQUIRES(FrameInfo),
  PROVIDES(AprilTagPercept),
  LOADS_PARAMETERS(
  {,
    (float) minDecisionMargin,       /**< Detections with a smaller decision margin are dropped. */
    (int) maxHamming,                /**< Detections with more corrected bits are dropped. */
    (float) sizeTolerance,           /**< Largest relative difference between the distance from the pose and the distance from the apparent size. */
    (int) frameSkip,                 /**< The detector runs on every n-th image only; 1 runs on every image. The percept is empty for the images in between. */
    (float) maxDistance,             /**< Tags farther away than this are dropped (mm). */
    (int) maxId,                     /**< Tags with a larger id are dropped. */
    (float) quadDecimate,            /**< Downscale factor of the quad detection. */
  }),
});

/**
 * @class AprilTagDetector
 * Runs the apriltag library on the grayscale image.
 */
class AprilTagDetector : public AprilTagDetectorBase
{
public:
  AprilTagDetector();
  ~AprilTagDetector();

  /**
   * Converts a tag pose from the apriltag conventions to the AprilTagPercept conventions.
   * @param rotation Rotation of the tag in the camera optical frame (apriltag convention).
   * @param translation Position of the tag in the camera optical frame in metres.
   * @param cameraMatrix Pose of the B-Human camera frame in the robot frame (mm).
   * @return The pose of the tag in the robot frame in mm, with z the normal out of the wall.
   */
  static Pose3f toRobotFrame(const Matrix3f& rotation, const Vector3f& translation, const Pose3f& cameraMatrix);

private:
  void update(AprilTagPercept& aprilTagPercept) override;

  apriltag_detector* detector = nullptr; /**< The library's detector. */
  apriltag_family* family = nullptr;     /**< The tag36h11 family. */
  float tagSize = 0.15f;                 /**< Edge of the black square in metres, from the tag layout. */
  unsigned frameCounter = 0;             /**< Counts the images for `frameSkip`. */
};

/**
 * @class EmptyAprilTagProvider
 * Provides an empty AprilTagPercept in the thread of the camera that does not detect tags. The
 * alias module of the Cognition thread requires the percept from both camera threads.
 */
MODULE(EmptyAprilTagProvider,
{,
  PROVIDES(AprilTagPercept),
});

class EmptyAprilTagProvider : public EmptyAprilTagProviderBase
{
  void update(AprilTagPercept& aprilTagPercept) override { aprilTagPercept.detections.clear(); }
};
