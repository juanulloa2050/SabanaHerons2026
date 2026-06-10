/**
 * @file TeammatesBallModel.h
 *
 * Declaration of a representation that represents a ball model based
 * on observations made by my teammates.
 *
 * @author <A href="mailto:tlaue@uni-bremen.de">Tim Laue</A>
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include "Math/Eigen.h"

/**
 * @struct TeammatesBallModel
 */
STREAMABLE(TeammatesBallModel,
{
  void verify() const;
  void draw() const,

  (Vector2f) position,                         /**< The position of the ball in global field coordinates (in mm) */
  (Vector2f) velocity,                         /**< The velocity of the ball in global field coordinates (in mm/s) */
  (bool)(false) isValid,                       /**< Position and velocity are valid (i.e. somebody has seen the ball), if true */
  (bool)(false) newerThanOwnBall,              /**< true if a teammate has more recently seen and communicated a ball than we locally have*/
  (bool)(false) isPredictionOnly,              /**< true if the model is currently only propagated from the last shared sighting */
  (unsigned)(0) timeWhenLastSeen,              /**< timestamp of the last teammate sighting that contributed to this model */
  (unsigned char)(0) contributors,             /**< number of teammate observations fused into the current model */
});
