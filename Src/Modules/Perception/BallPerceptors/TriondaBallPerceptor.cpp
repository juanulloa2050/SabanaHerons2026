/**
 * @file TriondaBallPerceptor.cpp
 *
 * @author SabanaHerons 2026
 */

#include "TriondaBallPerceptor.h"
#include "Debugging/Annotation.h"
#include "Streaming/Output.h"
#include <iostream>
#include <chrono>

MAKE_MODULE(TriondaBallPerceptor);

void TriondaBallPerceptor::update(BallPercept& ballPercept)
{
  ballPercept.status = BallPercept::notSeen;

  const auto& spots = theBallSpots.ballSpots;
  if(static_cast<int>(spots.size()) < minSpotsRequired)
    return;

  // First spot is the largest blob (TriondaBallSpotsProvider sorts by count desc)
  ballPercept.positionInImage = spots[0].cast<float>();
  ballPercept.radiusInImage   = fixedRadiusInImage;

  if(theMeasurementCovariance.transformWithCovLegacy(
       ballPercept.positionInImage,
       theBallSpecification.radius,
       Vector2f(0.04f, 0.06f),
       ballPercept.positionOnField,
       ballPercept.covarianceOnField))
  {
    ballPercept.status    = BallPercept::seen;
    ballPercept.radiusOnField = theBallSpecification.radius;

    ANNOTATION("TriondaBallPerceptor", "SEEN spots=" << static_cast<unsigned>(spots.size())
               << " imgPos=(" << static_cast<int>(ballPercept.positionInImage.x())
               << "," << static_cast<int>(ballPercept.positionInImage.y())
               << ") fieldPos=(" << static_cast<int>(ballPercept.positionOnField.x())
               << "," << static_cast<int>(ballPercept.positionOnField.y()) << ")mm");

    {
      using Clock = std::chrono::steady_clock;
      static Clock::time_point lastLog = Clock::time_point::min();
      const auto now = Clock::now();
      if(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLog).count() > 2000)
      {
        lastLog = now;
        std::cout << "[TriondaBallPerceptor|"
                  << (theCameraInfo.camera == CameraInfo::upper ? "U" : "L")
                  << "] SEEN spots=" << static_cast<unsigned>(spots.size())
                  << " fieldPos=(" << static_cast<int>(ballPercept.positionOnField.x())
                  << "," << static_cast<int>(ballPercept.positionOnField.y()) << ")mm"
                  << " r=" << static_cast<int>(ballPercept.radiusInImage) << std::endl;
      }
    }
  }
  else
  {
    OUTPUT_TEXT("[TriondaBallPerceptor] transform to field failed");
  }
}
