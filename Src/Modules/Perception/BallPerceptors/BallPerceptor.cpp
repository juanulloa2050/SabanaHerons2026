/**
 * @file BallPerceptor.cpp
 *
 * This file implements a module that detects balls in images with a neural network.
 *
 * @author Bernd Poppinga
 * @author Felix Thielke
 * @author Gerrit Felsch
 */

#include "BallPerceptor.h"
#include "Platform/File.h"
#include "Platform/SystemCall.h"
#include "Debugging/Annotation.h"
#include "Debugging/DebugDrawings.h"
#include "Debugging/Stopwatch.h"
#include "Streaming/Global.h"
#include "Streaming/Output.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>
#include <vector>
#include "Tools/Math/Projection.h"
#include "Tools/Math/Transformation.h"

MAKE_MODULE(BallPerceptor);

BallPerceptor::BallPerceptor() :
  encoder(&Global::getAsmjitRuntime()),
  classifier(&Global::getAsmjitRuntime()),
  corrector(&Global::getAsmjitRuntime())
{
  compile();
}

void BallPerceptor::update(BallPercept& theBallPercept)
{
  DECLARE_DEBUG_DRAWING("module:BallPerceptor:spots", "drawingOnImage");

  DEBUG_RESPONSE_ONCE("module:BallPerceptor:compile")
    compile();

  theBallPercept.status = BallPercept::notSeen;
  bestFrameValid = false;

  if(!encoder.valid() || !classifier.valid() || !corrector.valid())
  {
    OUTPUT_TEXT("[BallPerceptor] Neural networks not compiled - skipping detection");
    return;
  }

  const std::vector<Vector2i>& ballSpots = theBallSpots.ballSpots;
  if(ballSpots.empty())
  {
    circleStreak = std::max(0, circleStreak - 1);  // decay, not hard reset
    OUTPUT_TEXT("[BallPerceptor] No ball spots to classify");
    return;
  }

  OUTPUT_TEXT("[BallPerceptor] Classifying " << static_cast<unsigned>(ballSpots.size()) << " spots | patchSize=" << static_cast<unsigned>(patchSize) << " | thresholds: guessed=" << guessedThreshold << " accept=" << acceptThreshold << " ensure=" << ensureThreshold);

  float prob, bestProb = guessedThreshold;
  Vector2f ballPosition, bestBallPosition;
  float radius, bestRadius;
  for(std::size_t i = 0; i < ballSpots.size(); ++i)
  {
    prob = apply(ballSpots[i], ballPosition, radius);

    COMPLEX_DRAWING("module:BallPerceptor:spots")
    {
      std::stringstream ss;
      ss << i << ": " << static_cast<int>(prob * 100);
      DRAW_TEXT("module:BallPerceptor:spots", ballSpots[i].x(), ballSpots[i].y(), 15, ColorRGBA::red, ss.str());
    }

    if(prob > bestProb)
    {
      bestProb = prob;
      bestBallPosition = ballPosition;
      bestRadius = radius;
      if(useColorEncoder && !lastExtractedPatch.empty())
      {
        bestFramePatch = lastExtractedPatch;
        bestFrameSpot  = lastExtractedSpot;
        bestFrameArea  = lastExtractedArea;
        bestFrameValid = true;
      }
      OUTPUT_TEXT("[BallPerceptor]   New best: spot[" << static_cast<unsigned>(i) << "] prob=" << prob << " pos=(" << ballPosition.x() << ", " << ballPosition.y() << ") radius=" << radius);
      if(SystemCall::getMode() == SystemCall::physicalRobot && prob >= ensureThreshold)
      {
        OUTPUT_TEXT("[BallPerceptor]   Ensure threshold reached - stopping early");
        break;
      }
    }
  }

  if(bestProb > guessedThreshold)
  {
    // Temporal hysteresis: track consecutive frames with a circle-confirmed spot
    // at the same image position to promote guessed → seen.
    const bool isCircleResult = (bestProb == guessedThreshold);
    if(isCircleResult)
    {
      const float drift = (bestBallPosition - circleLastPos).norm();
      if(circleStreak > 0 && drift < bestRadius * 2.f)
        ++circleStreak;
      else
        circleStreak = 1;
      circleLastPos = bestBallPosition;

      const int streakNeeded = (theCameraInfo.camera == CameraInfo::upper)
                               ? upperCircleStreakForSeen : circleStreakForSeen;
      if(circleStreak >= streakNeeded)
        bestProb = acceptThreshold;  // promote to seen
    }
    else
    {
      circleStreak = std::max(0, circleStreak - 1);
    }

    theBallPercept.positionInImage = bestBallPosition;
    theBallPercept.radiusInImage = bestRadius;

    // TL: Function is called with hardcoded numbers here, as this parameter is only for backward compatibility to previous approach and should vanish hopefully soon.
    if(theMeasurementCovariance.transformWithCovLegacy(theBallPercept.positionInImage, theBallSpecification.radius, Vector2f(0.04f, 0.06f),
                                                       theBallPercept.positionOnField, theBallPercept.covarianceOnField))
    {
      theBallPercept.status = bestProb >= acceptThreshold ? BallPercept::seen : BallPercept::guessed;
      OUTPUT_TEXT("[BallPerceptor] DETECTED: status=" << (theBallPercept.status == BallPercept::seen ? "SEEN" : "GUESSED") << " | prob=" << bestProb << " | imgPos=(" << bestBallPosition.x() << ", " << bestBallPosition.y() << ") | fieldPos=(" << theBallPercept.positionOnField.x() << ", " << theBallPercept.positionOnField.y() << ") | radius=" << bestRadius);
      ANNOTATION("BallPerceptor", (theBallPercept.status == BallPercept::seen ? "SEEN" : "GUESSED") << " prob=" << bestProb << " imgPos=(" << static_cast<int>(bestBallPosition.x()) << "," << static_cast<int>(bestBallPosition.y()) << ") fieldPos=(" << static_cast<int>(theBallPercept.positionOnField.x()) << "," << static_cast<int>(theBallPercept.positionOnField.y()) << ")mm r=" << static_cast<int>(bestRadius));
      {
        using Clock = std::chrono::steady_clock;
        static Clock::time_point lastBallLogTime = Clock::time_point::min();
        const auto now = Clock::now();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBallLogTime).count() > 2000)
        {
          lastBallLogTime = now;
          std::cout << "[BallPerceptor|" << (theCameraInfo.camera == CameraInfo::upper ? "U" : "L") << "] "
                    << (theBallPercept.status == BallPercept::seen ? "SEEN" : "GUESSED")
                    << " prob=" << bestProb
                    << " fieldPos=(" << static_cast<int>(theBallPercept.positionOnField.x()) << ","
                    << static_cast<int>(theBallPercept.positionOnField.y()) << ")mm"
                    << " r=" << static_cast<int>(bestRadius) << std::endl;
        }
      }
      return;
    }
    else
    {
      OUTPUT_TEXT("[BallPerceptor] Probability=" << bestProb << " but transform to field failed");
    }
  }
  else
  {
    OUTPUT_TEXT("[BallPerceptor] No spot passed guessed threshold (bestProb=" << bestProb << ")");
    ANNOTATION("BallPerceptor", "notSeen bestProb=" << bestProb << " spots=" << static_cast<unsigned>(ballSpots.size()));
  }

  // Special ball handling for penalty goal keeper
  if((theGameState.state == GameState::opponentPenaltyShot || theGameState.state == GameState::opponentPenaltyKick) &&
     theMotionInfo.isKeyframeMotion(KeyframeMotionRequest::sitDownKeeper))
  {
    Vector2f inImageLowPoint;
    Vector2f inImageUpPoint;
    if(Transformation::robotToImage(theRobotPose.inverse() * Vector2f(theFieldDimensions.xPosOwnGoalArea + 350.f, 0.f), theCameraMatrix, theCameraInfo, inImageLowPoint)
       && Transformation::robotToImage(theRobotPose.inverse() * Vector2f(theFieldDimensions.xPosOwnPenaltyMark, 0.f), theCameraMatrix, theCameraInfo, inImageUpPoint))
    {
      const int lowerY = std::min(static_cast<int>(inImageLowPoint.y()), theCameraInfo.height);
      const int upperY = std::max(static_cast<int>(inImageUpPoint.y()), 0);

      std::vector<Vector2i> sortedBallSpots = theBallSpots.ballSpots;
      std::sort(sortedBallSpots.begin(), sortedBallSpots.end(), [&](const Vector2i& a, const Vector2i& b) {return a.y() > b.y(); });

      for(const Vector2i& spot : sortedBallSpots)
        if(spot.y() < lowerY && spot.y() > upperY)
        {
          theBallPercept.positionInImage = spot.cast<float>();
          // TL: Function is called with hardcoded numbers here, as this parameter is only for backward compatibility to previous approach and should vanish hopefully soon.
          if(theMeasurementCovariance.transformWithCovLegacy(theBallPercept.positionInImage, theBallSpecification.radius, Vector2f(0.04f, 0.06f),
                                                             theBallPercept.positionOnField, theBallPercept.covarianceOnField))
          {
            theBallPercept.status = BallPercept::seen;
          }

          theBallPercept.radiusInImage = 30.f;
        }
    }
    OUTPUT_TEXT("[BallPerceptor] Penalty keeper special: status=" << (theBallPercept.status == BallPercept::seen ? "SEEN" : "notSeen"));
  }

}

float BallPerceptor::apply(const Vector2i& ballSpot, Vector2f& ballPosition, float& predRadius)
{
  Vector2f relativePoint;
  Geometry::Circle ball;
  if(!(Transformation::imageToRobotHorizontalPlane(ballSpot.cast<float>(), theBallSpecification.radius, theCameraMatrix, theCameraInfo, relativePoint)
       && Projection::calculateBallInImage(relativePoint, theCameraMatrix, theCameraInfo, theBallSpecification.radius, ball)))
  {
    OUTPUT_TEXT("[BallPerceptor::apply] Spot (" << ballSpot.x() << ", " << ballSpot.y() << ") -> projection failed");
    return -1.f;
  }

  int ballArea = static_cast<int>(ball.radius * ballAreaFactor);
  ballArea += 4 - (ballArea % 4);

  OUTPUT_TEXT("[BallPerceptor::apply] Spot (" << ballSpot.x() << ", " << ballSpot.y() << ") | relField=(" << relativePoint.x() << ", " << relativePoint.y() << ") | dist=" << relativePoint.norm() << "mm | ballArea=" << ballArea << "px | imgRadius=" << ball.radius << "px");

  RECTANGLE("module:BallPerceptor:spots", static_cast<int>(ballSpot.x() - ballArea / 2), static_cast<int>(ballSpot.y() - ballArea / 2), static_cast<int>(ballSpot.x() + ballArea / 2), static_cast<int>(ballSpot.y() + ballArea / 2), 2, Drawings::PenStyle::solidPen, ColorRGBA::black);

  STOPWATCH("module:BallPerceptor:getImageSection")
  {
    if(useColorEncoder)
    {
      // Color encoder: extract YCrCb patch from raw CameraImage (YUYV)
      extractColorPatch(ballSpot, ballArea);
    }
    else if(useFloat)
    {
      PatchUtilities::extractPatch(ballSpot, Vector2i(ballArea, ballArea), Vector2i(static_cast<int>(patchSize), static_cast<int>(patchSize)), theECImage.grayscaled, encoder.input(0).data(), extractionMode);
      switch(normalizationMode)
      {
        case BallPerceptorModule::Params::normalizeContrast:
          PatchUtilities::normalizeContrast(encoder.input(0).data(), Vector2i(static_cast<int>(patchSize), static_cast<int>(patchSize)), normalizationOutlierRatio);
          break;
        case BallPerceptorModule::Params::normalizeBrightness:
          PatchUtilities::normalizeBrightness(encoder.input(0).data(), Vector2i(static_cast<int>(patchSize), static_cast<int>(patchSize)), normalizationOutlierRatio);
      }
    }
    else
    {
      PatchUtilities::extractPatch(ballSpot, Vector2i(ballArea, ballArea), Vector2i(static_cast<int>(patchSize), static_cast<int>(patchSize)), theECImage.grayscaled, reinterpret_cast<unsigned char*>(encoder.input(0).data()), extractionMode);
      switch(normalizationMode)
      {
        case BallPerceptorModule::Params::normalizeContrast:
          PatchUtilities::normalizeContrast(reinterpret_cast<unsigned char*>(encoder.input(0).data()), Vector2i(static_cast<int>(patchSize), static_cast<int>(patchSize)), normalizationOutlierRatio);
          break;
        case BallPerceptorModule::Params::normalizeBrightness:
          PatchUtilities::normalizeBrightness(reinterpret_cast<unsigned char*>(encoder.input(0).data()), Vector2i(static_cast<int>(patchSize), static_cast<int>(patchSize)), normalizationOutlierRatio);
      }
    }
  }
  const float stepSize = static_cast<float>(ballArea) / static_cast<float>(patchSize);

  // encode patch
  encoder.apply();

  // classify
  classifier.input(0) = encoder.output(0);
  classifier.apply();
  const float pred = classifier.output(0)[0];

  OUTPUT_TEXT("[BallPerceptor::apply]   Classifier prob=" << pred);

  // Circle fallback: sample at 3 radii to handle projection errors when head moves
  if(useCircleFallback && pred < guessedThreshold)
  {
    const bool  isUpper   = (theCameraInfo.camera == CameraInfo::upper);
    const float csThresh  = isUpper ? upperCircleScoreThreshold : circleScoreThreshold;
    const float r = ball.radius;
    const float cs = std::max({computeCircleScore(ballSpot, r * 0.75f),
                                computeCircleScore(ballSpot, r),
                                computeCircleScore(ballSpot, r * 1.35f)});
    OUTPUT_TEXT("[BallPerceptor::apply]   CircleScore=" << cs << " threshold=" << csThresh);
    if(cs >= csThresh)
    {
      // Reject monocolor blobs (grass, lines, jerseys) via color diversity
      if(useColorDiversity)
      {
        const float divThresh = (theCameraInfo.camera == CameraInfo::upper)
                                ? upperColorDiversityThreshold : colorDiversityThreshold;
        const float div = computeColorDiversity(ballSpot, r);
        OUTPUT_TEXT("[BallPerceptor::apply]   ColorDiversity=" << div << " threshold=" << divThresh);
        if(div < divThresh)
        {
          OUTPUT_TEXT("[BallPerceptor::apply]   CircleFallback REJECTED monocolor div=" << div);
          return -1.f;
        }
      }
      ballPosition = ballSpot.cast<float>();
      predRadius   = r;
      OUTPUT_TEXT("[BallPerceptor::apply]   CircleFallback ACCEPTED score=" << cs);
      return guessedThreshold;
    }
  }

  // predict ball position if NN prob is high enough
  if(pred > guessedThreshold)
  {
    corrector.input(0) = encoder.output(0);
    corrector.apply();
    ballPosition.x() = (corrector.output(0)[0] - patchSize / 2) * stepSize + ballSpot.x();
    ballPosition.y() = (corrector.output(0)[1] - patchSize / 2) * stepSize + ballSpot.y();
    predRadius = corrector.output(0)[2] * stepSize;
    OUTPUT_TEXT("[BallPerceptor::apply]   Corrector: correctedPos=(" << ballPosition.x() << ", " << ballPosition.y() << ") correctedRadius=" << predRadius);
  }

  return pred;
}

float BallPerceptor::computeCircleScore(const Vector2i& center, float radius) const
{
  static constexpr float twoPi = 6.28318530717959f;
  static constexpr int   N     = 16;
  const int edgeThresh = (theCameraInfo.camera == CameraInfo::upper)
                         ? upperCircleEdgeThreshold : circleEdgeThreshold;

  const int w = static_cast<int>(theECImage.grayscaled.width);
  const int h = static_cast<int>(theECImage.grayscaled.height);
  int hits  = 0;
  int valid = 0;

  for(int i = 0; i < N; ++i)
  {
    const float angle = twoPi * i / N;
    const int x = center.x() + static_cast<int>(radius * std::cos(angle));
    const int y = center.y() + static_cast<int>(radius * std::sin(angle));

    if(x < 1 || x >= w - 1 || y < 1 || y >= h - 1)
      continue;

    ++valid;
    const int gx = static_cast<int>(theECImage.grayscaled[Vector2i(x + 1, y)])
                 - static_cast<int>(theECImage.grayscaled[Vector2i(x - 1, y)]);
    const int gy = static_cast<int>(theECImage.grayscaled[Vector2i(x, y + 1)])
                 - static_cast<int>(theECImage.grayscaled[Vector2i(x, y - 1)]);
    const float grad = std::sqrt(static_cast<float>(gx * gx + gy * gy));
    if(grad >= static_cast<float>(edgeThresh))
      ++hits;
  }

  return valid > 0 ? static_cast<float>(hits) / valid : 0.f;
}

void BallPerceptor::compile()
{
  const std::string baseDir = std::string(File::getBHDir()) + "/Config/NeuralNets/BallPerceptor/";
  encModel = std::make_unique<NeuralNetwork::Model>(baseDir + encoderName);
  clModel = std::make_unique<NeuralNetwork::Model>(baseDir + classifierName);
  corModel = std::make_unique<NeuralNetwork::Model>(baseDir + correctorName);

  if(!useFloat)
    encModel->setInputUInt8(0);

  encoder.compile(*encModel);
  classifier.compile(*clModel);
  corrector.compile(*corModel);

  ASSERT(encoder.numOfInputs() == 1);
  ASSERT(classifier.numOfInputs() == 1);
  ASSERT(corrector.numOfInputs() == 1);

  ASSERT(classifier.numOfOutputs() == 1);
  ASSERT(corrector.numOfOutputs() == 1);
  ASSERT(encoder.numOfOutputs() == 1);

  ASSERT(encoder.input(0).rank() == 3);
  ASSERT(encoder.input(0).dims(0) == encoder.input(0).dims(1));

  // Support grayscale (1ch) and color YCrCb (3ch) encoders
  const auto inputChannels = encoder.input(0).dims(2);
  ASSERT(inputChannels == 1 || inputChannels == 3);
  useColorEncoder = (inputChannels == 3);

  // Color encoder requires float buffer; enforce it at compile time
  if(useColorEncoder && !useFloat)
    OUTPUT_TEXT("[BallPerceptor] WARNING: useFloat must be true for color encoder — results may be wrong.");

  ASSERT(classifier.output(0).rank() == 1);
  ASSERT(classifier.output(0).dims(0) == 1 && corrector.output(0).dims(0) == 3);
  patchSize = encoder.input(0).dims(0);
}

void BallPerceptor::extractColorPatch(const Vector2i& ballSpot, int ballArea)
{
  // encoder.input(0).data() → float[patchSize * patchSize * 3] in HWC layout
  // Channel order: Y(0), Cr=V(1), Cb=U(2)  ← matches Python cv2.COLOR_BGR2YCrCb

  const int ps   = static_cast<int>(patchSize);
  const int half = ballArea / 2;
  const int imgW = static_cast<int>(theCameraImage.width);
  const int imgH = static_cast<int>(theCameraImage.height);
  const float step = static_cast<float>(ballArea) / static_cast<float>(ps);
  const int x0 = ballSpot.x() - half;
  const int y0 = ballSpot.y() - half;

  // Extract three planar channels from the YUYV camera image
  std::vector<float> chY(static_cast<size_t>(ps * ps));
  std::vector<float> chCr(static_cast<size_t>(ps * ps));
  std::vector<float> chCb(static_cast<size_t>(ps * ps));

  for(int oy = 0; oy < ps; ++oy)
  {
    for(int ox = 0; ox < ps; ++ox)
    {
      const int ix  = std::clamp(x0 + static_cast<int>(static_cast<float>(ox) * step), 0, imgW - 1);
      const int iy  = std::clamp(y0 + static_cast<int>(static_cast<float>(oy) * step), 0, imgH - 1);
      const int idx = oy * ps + ox;
      chY[idx]  = static_cast<float>(theCameraImage.getY(static_cast<size_t>(ix), static_cast<size_t>(iy)));
      chCr[idx] = static_cast<float>(theCameraImage.getV(static_cast<size_t>(ix), static_cast<size_t>(iy)));  // V = Cr
      chCb[idx] = static_cast<float>(theCameraImage.getU(static_cast<size_t>(ix), static_cast<size_t>(iy)));  // U = Cb
    }
  }

  // Normalize each channel independently (replica of Python normalizeBrightness)
  const Vector2i sz(ps, ps);
  PatchUtilities::normalizeBrightness(chY.data(),  sz, normalizationOutlierRatio);
  PatchUtilities::normalizeBrightness(chCr.data(), sz, normalizationOutlierRatio);
  PatchUtilities::normalizeBrightness(chCb.data(), sz, normalizationOutlierRatio);

  // Interleave into HWC float buffer
  float* out = encoder.input(0).data();
  for(int i = 0; i < ps * ps; ++i)
  {
    out[i * 3 + 0] = chY[i];
    out[i * 3 + 1] = chCr[i];
    out[i * 3 + 2] = chCb[i];
  }

  // Cache patch for RawBallPatch streaming
  lastExtractedPatch.assign(out, out + ps * ps * 3);
  lastExtractedSpot = ballSpot;
  lastExtractedArea = ballArea;
}

void BallPerceptor::update(RawBallPatch& theRawBallPatch)
{
  theRawBallPatch.valid = bestFrameValid;
  if(bestFrameValid)
  {
    theRawBallPatch.data         = bestFramePatch;
    theRawBallPatch.patchSize    = static_cast<int>(patchSize);
    theRawBallPatch.spotPosition = bestFrameSpot;
    theRawBallPatch.ballArea     = bestFrameArea;
  }
  else
  {
    theRawBallPatch.data.clear();
    theRawBallPatch.patchSize = 0;
  }
}

float BallPerceptor::computeColorDiversity(const Vector2i& center, float radius) const
{
  // Sample a 7×7 grid inside the projected ball circle.
  // Return max(stddev_Cr, stddev_Cb) — the trionda has mixed saturated colors
  // so diversity is high; monocolor FPs (grass, lines, jerseys) have low diversity.
  static constexpr int G = 7;
  const int imgW = static_cast<int>(theCameraImage.width);   // YUYV cols
  const int imgH = static_cast<int>(theCameraImage.height);

  float sumCr = 0, sumCb = 0;
  float sumCr2 = 0, sumCb2 = 0;
  int   n = 0;

  for(int gy = -(G/2); gy <= G/2; ++gy)
  {
    for(int gx = -(G/2); gx <= G/2; ++gx)
    {
      if(gx*gx + gy*gy > (G/2+1)*(G/2+1)) continue;  // stay inside circle
      const int vx = center.x() + static_cast<int>(gx * radius / (G/2));
      const int vy = center.y() + static_cast<int>(gy * radius / (G/2));
      const int yuvX = std::clamp(vx / 2, 0, imgW - 1);
      const int yuvY = std::clamp(vy,     0, imgH - 1);
      const float cr = static_cast<float>(theCameraImage.getV(yuvX, yuvY));
      const float cb = static_cast<float>(theCameraImage.getU(yuvX, yuvY));
      sumCr += cr;  sumCr2 += cr*cr;
      sumCb += cb;  sumCb2 += cb*cb;
      ++n;
    }
  }

  if(n < 2) return 0.f;
  const float varCr = sumCr2/n - (sumCr/n)*(sumCr/n);
  const float varCb = sumCb2/n - (sumCb/n)*(sumCb/n);
  return std::sqrt(std::max({varCr, varCb, 0.f}));
}
