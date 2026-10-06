/**
 * @file RawBallPatch.h
 *
 * Carries the raw 32×32×3 float patch (YCrCb, HWC, values 0-255) that
 * BallPerceptor extracted from the best ball candidate this frame.
 * CameraStreamer reads this and appends it to the TCP stream so that
 * Python training scripts receive bit-identical patches to what the NN sees.
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include "Math/Eigen.h"
#include <vector>

STREAMABLE(RawBallPatch,
{,
  (std::vector<float>) data,   /**< Flat HWC float32: patchSize*patchSize*3, values 0-255. */
  (int)(0)  patchSize,         /**< Side length of the patch (typically 32). */
  (Vector2i) spotPosition,     /**< BallSpot (x,y) this patch was extracted from. */
  (int)(0)  ballArea,          /**< ballArea used to extract (for Python to verify scale). */
  (bool)(false) valid,         /**< true only when BallPerceptor produced a patch this frame. */
});
