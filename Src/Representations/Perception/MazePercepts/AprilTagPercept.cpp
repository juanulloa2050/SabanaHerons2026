/**
 * @file AprilTagPercept.cpp
 *
 * Drawing and verification of the AprilTag percept.
 */

#include "AprilTagPercept.h"
#include "Debugging/DebugDrawings.h"
#include "Platform/BHAssert.h"
#include <cmath>
#include <string>

void AprilTagPercept::draw() const
{
  DEBUG_DRAWING("representation:AprilTagPercept:field", "drawingOnField")
  {
    for(const Detection& detection : detections)
    {
      const Vector3f& position = detection.tagInRobot.translation;
      const Vector3f normal = detection.tagInRobot.rotation.col(2);
      // The tag is a short bar across its normal.
      const Vector2f along(-normal.y(), normal.x());
      const Vector2f centre = position.head<2>();
      LINE("representation:AprilTagPercept:field", centre.x() - along.x() * 75.f, centre.y() - along.y() * 75.f,
           centre.x() + along.x() * 75.f, centre.y() + along.y() * 75.f, 20, Drawings::solidPen, ColorRGBA::blue);
      ARROW("representation:AprilTagPercept:field", centre.x(), centre.y(), centre.x() + normal.x() * 100.f, centre.y() + normal.y() * 100.f,
            10, Drawings::solidPen, ColorRGBA::blue);
      DRAW_TEXT("representation:AprilTagPercept:field", centre.x(), centre.y(), 60, ColorRGBA::blue, detection.id);
    }
  }
}

void AprilTagPercept::verify() const
{
  for(const Detection& detection : detections)
  {
    ASSERT(detection.id >= 0);
    ASSERT(std::isfinite(detection.tagInRobot.translation.x()));
    ASSERT(std::isfinite(detection.tagInRobot.translation.y()));
    ASSERT(std::isfinite(detection.tagInRobot.translation.z()));
  }
}
