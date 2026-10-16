/**
 * @file AprilTagDetector.cpp
 *
 * Implementation of the AprilTag detector, see AprilTagDetector.h for the frame conventions.
 */

#include "AprilTagDetector.h"
#include "Debugging/DebugDrawings.h"
#include "Debugging/Stopwatch.h"
#include "Platform/BHAssert.h"
#include "Streaming/Output.h"
#include "Tools/Maze/AprilTagGeometry.h"
#include <algorithm>
#include <apriltag/apriltag.h>
#include <apriltag/apriltag_pose.h>
#include <apriltag/common/matd.h>
#include <apriltag/tag36h11.h>
#include <cmath>

MAKE_MODULE(AprilTagDetector);

AprilTagDetector::AprilTagDetector()
{
  Maze::MazeTagLayout layout;
  if(Maze::MazeTagLayout::load("mazeTagLayout.cfg", layout))
    tagSize = layout.size * 0.001f;
  else
    OUTPUT_WARNING("AprilTagDetector: mazeTagLayout.cfg not found, assuming a tag size of 150 mm.");

  family = tag36h11_create();
  detector = apriltag_detector_create();
  apriltag_detector_add_family(detector, family);
  detector->nthreads = 1;
}

AprilTagDetector::~AprilTagDetector()
{
  apriltag_detector_destroy(detector);
  tag36h11_destroy(family);
}

void AprilTagDetector::update(AprilTagPercept& aprilTagPercept)
{
  DECLARE_DEBUG_DRAWING("module:AprilTagDetector:tags", "drawingOnImage");

  aprilTagPercept.detections.clear();
  aprilTagPercept.timestamp = theFrameInfo.time;

  const Image<PixelTypes::GrayscaledPixel>& gray = theECImage.grayscaled;
  if(gray.width == 0 || gray.height == 0 || theECImage.timestamp <= 10 || theCameraInfo.focalLength <= 0.f)
    return;
  if(frameSkip > 1 && frameCounter++ % static_cast<unsigned>(frameSkip) != 0)
    return;

  detector->quad_decimate = quadDecimate; // parameters can change at runtime
  Maze::AprilTagGeometry::Filter filter;
  filter.minDecisionMargin = minDecisionMargin;
  filter.maxHamming = maxHamming;
  filter.sizeTolerance = sizeTolerance;
  filter.maxDistance = maxDistance;
  filter.maxId = maxId;

  STOPWATCH("AprilTagDetector")
  {
    // The image is wrapped, not copied: one byte per pixel, rows are contiguous.
    image_u8_t image = {static_cast<int32_t>(gray.width), static_cast<int32_t>(gray.height), static_cast<int32_t>(gray.width),
                        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(gray[0]))};

    zarray_t* found = apriltag_detector_detect(detector, &image);
    for(int i = 0; i < zarray_size(found); ++i)
    {
      apriltag_detection_t* tag;
      zarray_get(found, i, &tag);

      if(!Maze::AprilTagGeometry::acceptsDecoding(tag->id, tag->hamming, tag->decision_margin, filter))
        continue;

      apriltag_detection_info_t info;
      info.det = tag;
      info.tagsize = tagSize;
      info.fx = theCameraInfo.focalLength;
      info.fy = theCameraInfo.focalLengthHeight;
      info.cx = theCameraInfo.opticalCenter.x();
      info.cy = theCameraInfo.opticalCenter.y();
      apriltag_pose_t pose;
      estimate_tag_pose(&info, &pose);

      Matrix3f rotation;
      for(int row = 0; row < 3; ++row)
        for(int col = 0; col < 3; ++col)
          rotation(row, col) = static_cast<float>(MATD_EL(pose.R, row, col));
      const Vector3f translation(static_cast<float>(MATD_EL(pose.t, 0, 0)),
                                 static_cast<float>(MATD_EL(pose.t, 1, 0)),
                                 static_cast<float>(MATD_EL(pose.t, 2, 0)));
      matd_destroy(pose.R);
      matd_destroy(pose.t);

      if(!rotation.allFinite() || !translation.allFinite() || translation.z() <= 0.f)
        continue;

      // The pose distance must match the distance that the apparent size of the tag implies.
      float longestEdge = 0.f;
      AprilTagPercept::Detection detection;
      for(int c = 0; c < 4; ++c)
      {
        detection.corners.emplace_back(static_cast<float>(tag->p[c][0]), static_cast<float>(tag->p[c][1]));
        longestEdge = std::max(longestEdge, (detection.corners[c] - Vector2f(static_cast<float>(tag->p[(c + 1) % 4][0]),
                                                                              static_cast<float>(tag->p[(c + 1) % 4][1]))).norm());
      }
      if(!Maze::AprilTagGeometry::acceptsDistance(translation.norm() * 1000.f, longestEdge, theCameraInfo.focalLength,
                                                  tagSize * 1000.f, filter))
        continue;

      detection.id = tag->id;
      detection.decisionMargin = tag->decision_margin;
      detection.tagInRobot = Maze::AprilTagGeometry::toRobotFrame(rotation, translation, theCameraMatrix);
      aprilTagPercept.detections.push_back(detection);
    }
    apriltag_detections_destroy(found);
  }

  COMPLEX_DRAWING("module:AprilTagDetector:tags")
  {
    for(const AprilTagPercept::Detection& detection : aprilTagPercept.detections)
    {
      Vector2f centre = Vector2f::Zero();
      for(int c = 0; c < 4; ++c)
      {
        const Vector2f& from = detection.corners[c];
        const Vector2f& to = detection.corners[(c + 1) % 4];
        LINE("module:AprilTagDetector:tags", from.x(), from.y(), to.x(), to.y(), 3, Drawings::solidPen, ColorRGBA::green);
        centre += from * 0.25f;
      }
      DRAW_TEXT("module:AprilTagDetector:tags", centre.x() - 15.f, centre.y(), 30, ColorRGBA::red, "id " << detection.id);
    }
  }
}
