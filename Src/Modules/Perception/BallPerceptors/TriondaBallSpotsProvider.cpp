/**
 * @file TriondaBallSpotsProvider.cpp
 *
 * Implementation of TriondaBallSpotsProvider.
 * Detects orange blobs in YUYV image space and emits them as BallSpots
 * for consumption by BallPerceptor.
 *
 * YUYV layout (BHuman CameraImage):
 *   theCameraImage(x, y)  →  pixel pair {y0, u, y1, v}
 *   Visual pixel (2x,   y) : luminance = y0,  Cb = u,  Cr = v
 *   Visual pixel (2x+1, y) : luminance = y1,  Cb = u,  Cr = v
 *   theCameraImage.width  = visual_width / 2  (number of YUYV pairs per row)
 *   theCameraImage.height = visual_height
 *
 * @author SabanaHerons 2026
 */

#include "TriondaBallSpotsProvider.h"
#include "Debugging/Debugging.h"
#include "Streaming/Output.h"
#include <algorithm>
#include <cmath>

MAKE_MODULE(TriondaBallSpotsProvider);

// ── Internal blob type ────────────────────────────────────────────────────────

namespace
{
  struct Blob
  {
    long sumX  = 0;
    long sumY  = 0;
    int  count = 0;

    void absorb(int vx, int vy) { sumX += vx; sumY += vy; ++count; }
    int cx() const { return static_cast<int>(sumX / count); }
    int cy() const { return static_cast<int>(sumY / count); }
  };
} // namespace

// ── Module update ─────────────────────────────────────────────────────────────

void TriondaBallSpotsProvider::update(BallSpots& ballSpots)
{
  ballSpots.ballSpots.clear();
  ballSpots.firstSpotIsPredicted = false;

  // Image dimensions in visual pixels
  const int vH    = static_cast<int>(theCameraImage.height);
  const int yuvW  = static_cast<int>(theCameraImage.width);  // YUYV columns
  const int vW    = yuvW * 2;                                 // visual width

  // Per-camera parameter overrides: upper camera sees ball smaller/further away
  const bool isUpper      = (theCameraInfo.camera == CameraInfo::upper);
  const int  activeScan   = isUpper ? upperScanStep    : scanStep;
  const int  activeMinBlob= isUpper ? upperMinBlobPixels: minBlobPixels;
  const int  activeMinSat = isUpper ? upperMinSaturation: minSaturation;

  const int yuvStep = std::max(1, activeScan / 2);
  const int yStep   = activeScan;

  std::vector<Blob> blobs;
  blobs.reserve(16);

  const int mr2 = mergeRadius * mergeRadius;  // squared merge radius

  for(int vy = 0; vy < vH; vy += yStep)
  {
    for(int yuvX = 0; yuvX < yuvW; yuvX += yuvStep)
    {
      const auto& px = theCameraImage(static_cast<unsigned>(yuvX),
                                       static_cast<unsigned>(vy));

      const int U = static_cast<int>(px.u);
      const int V = static_cast<int>(px.v);

      // Chroma gate: both visual pixels in this YUYV column share U and V.
      // Skip the entire column early if chroma is not orange.
      if(U < minU || U > maxU || V < minV || V > maxV)
        continue;

      // Check left visual pixel (y0)
      if(static_cast<int>(px.y0) >= minY && static_cast<int>(px.y0) <= maxY)
      {
        const int vx = yuvX * 2;

        // Find closest existing blob within mergeRadius
        Blob* best = nullptr;
        int   bestD2 = mr2 + 1;
        for(auto& b : blobs)
        {
          const int dx = vx - b.cx();
          const int dy = vy - b.cy();
          const int d2 = dx * dx + dy * dy;
          if(d2 < bestD2) { bestD2 = d2; best = &b; }
        }
        if(best)
          best->absorb(vx, vy);
        else
          blobs.push_back(Blob{}), blobs.back().absorb(vx, vy);
      }

      // Check right visual pixel (y1)
      const int vxRight = yuvX * 2 + 1;
      if(vxRight < vW &&
         static_cast<int>(px.y1) >= minY && static_cast<int>(px.y1) <= maxY)
      {
        Blob* best = nullptr;
        int   bestD2 = mr2 + 1;
        for(auto& b : blobs)
        {
          const int dx = vxRight - b.cx();
          const int dy = vy      - b.cy();
          const int d2 = dx * dx + dy * dy;
          if(d2 < bestD2) { bestD2 = d2; best = &b; }
        }
        if(best)
          best->absorb(vxRight, vy);
        else
          blobs.push_back(Blob{}), blobs.back().absorb(vxRight, vy);
      }
    }
  }

  // Second pass: saturation gate using ECImage — catches red/blue/white patches
  // that the chroma gate misses. Excludes grass-green by hue range.
  if(useSatGate)
  {
    const int satH = static_cast<int>(theECImage.grayscaled.height);
    const int satW = static_cast<int>(theECImage.grayscaled.width);

    for(int vy = 0; vy < satH; vy += yStep)
    {
      for(int vx = 0; vx < satW; vx += scanStep)
      {
        const int sat = static_cast<int>(theECImage.saturated[Vector2i(vx, vy)]);
        if(sat < activeMinSat)
          continue;

        const int hue = static_cast<int>(theECImage.hued[Vector2i(vx, vy)]);
        if(hue >= greenHueLow && hue <= greenHueHigh)
          continue;  // grass green — skip

        const int Y = static_cast<int>(theECImage.grayscaled[Vector2i(vx, vy)]);
        if(Y < minY || Y > maxY)
          continue;

        Blob* best = nullptr;
        int   bestD2 = mr2 + 1;
        for(auto& b : blobs)
        {
          const int dx = vx - b.cx();
          const int dy = vy - b.cy();
          const int d2 = dx * dx + dy * dy;
          if(d2 < bestD2) { bestD2 = d2; best = &b; }
        }
        if(best)
          best->absorb(vx, vy);
        else
          blobs.push_back(Blob{}), blobs.back().absorb(vx, vy);
      }
    }
  }

  // Sort by blob size descending so the largest (most likely the ball) comes first
  std::sort(blobs.begin(), blobs.end(), [](const Blob& a, const Blob& b){ return a.count > b.count; });

  // Emit spots for blobs that are large enough
  for(const auto& b : blobs)
  {
    if(b.count >= activeMinBlob)
      ballSpots.addBallSpot(b.cx(), b.cy());
  }

  // Per-frame text floods the Debug thread's queue on the robot (it can never
  // drain without a connected client) — only emit when explicitly requested.
  DEBUG_RESPONSE("module:TriondaBallSpotsProvider:stats")
    OUTPUT_TEXT("[TriondaBallSpotsProvider] cam="
                << (theCameraInfo.camera == CameraInfo::upper ? "Upper" : "Lower")
                << " blobs=" << static_cast<unsigned int>(blobs.size())
                << " emitted=" << static_cast<unsigned int>(ballSpots.ballSpots.size()));
}
