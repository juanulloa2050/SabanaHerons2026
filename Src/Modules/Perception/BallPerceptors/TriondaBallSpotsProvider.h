/**
 * @file TriondaBallSpotsProvider.h
 *
 * Provides BallSpots by detecting orange-colored regions directly in the
 * YUYV camera image.  Designed to find the "trionda" FIFA mini ball whose
 * color (orange) is not reliably detected by BHuman's scan-line / BOP-based
 * BallSpotsProvider.
 *
 * Algorithm:
 *   1. Scan the image at configurable step intervals.
 *   2. Classify each pixel as "orange" using a YCrCb threshold in YUYV space
 *      (Cr = V component, high; Cb = U component, low; Y = luminance, mid-range).
 *   3. Merge nearby orange pixels into blobs using a greedy centroid approach.
 *   4. Emit the centroid of every blob that exceeds a minimum pixel count.
 *
 * @author SabanaHerons 2026
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/CameraImage.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Perception/BallPercepts/BallSpots.h"
#include "Representations/Perception/ImagePreprocessing/ECImage.h"
#include <vector>

MODULE(TriondaBallSpotsProvider,
{,
  REQUIRES(CameraImage),
  REQUIRES(CameraInfo),
  REQUIRES(ECImage),
  PROVIDES(BallSpots),
  DEFINES_PARAMETERS(
  {,
    /**
     * Scan step in visual pixels.  Larger = faster but misses small balls.
     * Recommended: 3–6.
     */
    (int)(4) scanStep,

    /** Luminance (Y) range for orange pixels. */
    (int)(60)  minY,
    (int)(235) maxY,

    /**
     * Cb (U) range.  Orange has low blue: keep this below 128.
     * Narrowing this range reduces false positives on skin / yellow lines.
     */
    (int)(60)  minU,
    (int)(122) maxU,

    /**
     * Cr (V) range.  Orange has high red: keep this well above 128.
     * The trionda FIFA ball is strongly saturated, so 148 is a safe minimum.
     */
    (int)(148) minV,
    (int)(255) maxV,

    /**
     * Minimum number of orange scan-pixels for a blob to be emitted as a
     * BallSpot.  Increase to reduce noise from orange field markings or
     * jerseys; decrease if the ball is very far away (small in image).
     */
    (int)(4) minBlobPixels,

    /**
     * Manhattan-distance radius (in visual pixels) used when deciding whether
     * a new orange pixel belongs to an existing blob or starts a new one.
     * Should be roughly equal to the expected ball diameter at mid-range.
     */
    (int)(40) mergeRadius,

    /**
     * Saturation-based gate (second pass using ECImage.saturated).
     * Catches blue/red/white patches of the trionda that the chroma gate misses.
     * minSat: min saturation (0-255). Pixels below this are considered grey/white.
     * maxGreenHue: hue range [0-255 BHuman scale] excluded as grass-green.
     *   BHuman hue: 0=red, 64=yellow, 128=green, 192=blue.
     */
    (int)(60)  minSaturation,
    (int)(80)  greenHueLow,
    (int)(170) greenHueHigh,
    (bool)(true) useSatGate,
  }),
});

class TriondaBallSpotsProvider : public TriondaBallSpotsProviderBase
{
  void update(BallSpots& ballSpots) override;
};
