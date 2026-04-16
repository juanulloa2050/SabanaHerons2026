/**
 * @file TriondaBallPerceptor.h
 *
 * Converts BallSpots from TriondaBallSpotsProvider directly into a BallPercept
 * without using the neural-network classifier. The largest color blob (first spot,
 * since TriondaBallSpotsProvider sorts by size) is accepted as the ball.
 *
 * @author SabanaHerons 2026
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Configuration/BallSpecification.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Perception/BallPercepts/BallSpots.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/MeasurementCovariance.h"
#include "Math/Eigen.h"

MODULE(TriondaBallPerceptor,
{,
  REQUIRES(BallSpots),
  REQUIRES(BallSpecification),
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  REQUIRES(MeasurementCovariance),
  PROVIDES(BallPercept),
  DEFINES_PARAMETERS(
  {,
    (float)(30.f) fixedRadiusInImage, /**< Estimated ball radius in pixels. Tune if needed. */
    (int)(1)      minSpotsRequired,   /**< Minimum number of spots before accepting detection. */
  }),
});

class TriondaBallPerceptor : public TriondaBallPerceptorBase
{
  void update(BallPercept& ballPercept) override;
};
