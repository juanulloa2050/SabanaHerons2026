/**
 * @file AprilTagDetector.h
 *
 * Detects the maze's tag36h11 AprilTags in the grayscale image of the upper camera and publishes
 * them as AprilTagPercept: id, the four corners in the image, the decision margin and the pose of
 * the tag in the robot frame.
 *
 * Frames and filters. The pose conversion from the apriltag conventions to the AprilTagPercept
 * conventions and the plausibility filters live in Tools/Maze/AprilTagGeometry.h (unit tested).
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
  REQUIRES(FrameInfo),
  PROVIDES(AprilTagPercept),
});

class EmptyAprilTagProvider : public EmptyAprilTagProviderBase
{
  void update(AprilTagPercept& aprilTagPercept) override
  {
    aprilTagPercept.detections.clear();
    aprilTagPercept.timestamp = theFrameInfo.time;
  }
};
