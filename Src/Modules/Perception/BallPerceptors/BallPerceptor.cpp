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
#include <iostream>
#include <chrono>
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

  if(!encoder.valid() || !classifier.valid() || !corrector.valid())
  {
    OUTPUT_TEXT("[BallPerceptor] Neural networks not compiled - skipping detection");
    return;
  }

  const std::vector<Vector2i>& ballSpots = theBallSpots.ballSpots;
  if(ballSpots.empty())
  {
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
    if(useFloat)
    {
      PatchUtilities::extractPatch(ballSpot, Vector2i(ballArea, ballArea), Vector2i(patchSize, patchSize), theECImage.grayscaled, encoder.input(0).data(), extractionMode);
      switch(normalizationMode)
      {
        case BallPerceptorModule::Params::normalizeContrast:
          PatchUtilities::normalizeContrast(encoder.input(0).data(), Vector2i(patchSize, patchSize), normalizationOutlierRatio);
          break;
        case BallPerceptorModule::Params::normalizeBrightness:
          PatchUtilities::normalizeBrightness(encoder.input(0).data(), Vector2i(patchSize, patchSize), normalizationOutlierRatio);
      }
    }
    else
    {
      PatchUtilities::extractPatch(ballSpot, Vector2i(ballArea, ballArea), Vector2i(patchSize, patchSize), theECImage.grayscaled, reinterpret_cast<unsigned char*>(encoder.input(0).data()), extractionMode);
      switch(normalizationMode)
      {
        case BallPerceptorModule::Params::normalizeContrast:
          PatchUtilities::normalizeContrast(reinterpret_cast<unsigned char*>(encoder.input(0).data()), Vector2i(patchSize, patchSize), normalizationOutlierRatio);
          break;
        case BallPerceptorModule::Params::normalizeBrightness:
          PatchUtilities::normalizeBrightness(reinterpret_cast<unsigned char*>(encoder.input(0).data()), Vector2i(patchSize, patchSize), normalizationOutlierRatio);
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

  // predict ball position if poss for ball is high enough
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
  ASSERT(encoder.input(0).dims(2) == 1);

  ASSERT(classifier.output(0).rank() == 1);
  ASSERT(classifier.output(0).dims(0) == 1 && corrector.output(0).dims(0) == 3);
  patchSize = encoder.input(0).dims(0);
}
