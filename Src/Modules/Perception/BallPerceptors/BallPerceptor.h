/**
 * @file BallPerceptor.h
 *
 * This file declares a module that detects balls in images with a neural network.
 *
 * @author Bernd Poppinga
 * @author Felix Thielke
 * @author Gerrit Felsch
 */

#pragma once

#include "Representations/Configuration/BallSpecification.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Infrastructure/CameraImage.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/MotionControl/MotionInfo.h"
#include "Representations/Perception/MeasurementCovariance.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Perception/BallPercepts/BallSpots.h"
#include "Representations/Perception/BallPercepts/RawBallPatch.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/ImagePreprocessing/ECImage.h"
#include "ImageProcessing/PatchUtilities.h"
#include "Math/Eigen.h"
#include "Framework/Module.h"
#include <CompiledNN/CompiledNN.h>
#include <CompiledNN/Model.h>

MODULE(BallPerceptor,
{,
  REQUIRES(BallSpots),
  REQUIRES(BallSpecification),
  REQUIRES(CameraImage),
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  REQUIRES(ECImage),
  REQUIRES(FieldDimensions),
  REQUIRES(GameState),
  REQUIRES(MeasurementCovariance),
  REQUIRES(MotionInfo),
  REQUIRES(RobotPose),
  PROVIDES(BallPercept),
  PROVIDES(RawBallPatch),
  LOADS_PARAMETERS(
  {
    ENUM(NormalizationMode,
    {,
      none,
      normalizeContrast,
      normalizeBrightness,
    }),

    (std::string) encoderName, /**< The file name (relative to "NeuralNetworks/BallPerceptor") from which to load the model.  */
    (std::string) classifierName,
    (std::string) correctorName,
    (float) guessedThreshold, /**< Limit from which a ball is guessed. */
    (float) acceptThreshold, /**< Limit from which a ball is accepted. */
    (float) ensureThreshold, /**< Limit from which a ball is detected for sure. */
    (NormalizationMode) normalizationMode, /**< The kind of normalization used for patches. */
    (float) normalizationOutlierRatio, /**< The ratio of pixels ignored when determining the value range that is scaled to 0..255. */
    (float) ballAreaFactor,
    (bool) useFloat,
    (PatchUtilities::ExtractionMode) extractionMode,
    (bool)(true) useCircleFallback,
    (float)(0.45f) circleScoreThreshold,
    (int)(25) circleEdgeThreshold,
    (int)(2) circleStreakForSeen,
    // Upper-camera overrides (smaller circle → lower thresholds)
    (float)(0.30f) upperCircleScoreThreshold,
    (int)(15) upperCircleEdgeThreshold,
    (int)(2) upperCircleStreakForSeen,
    // Color diversity check: rejects monocolor blobs (grass, lines, jerseys)
    (bool)(true)  useColorDiversity,
    (float)(8.f)  colorDiversityThreshold, /**< Min std-dev of Cr or Cb inside circle. */
    (float)(6.f)  upperColorDiversityThreshold,
  }),
});

class BallPerceptor : public BallPerceptorBase
{
public:
  BallPerceptor();

private:
  NeuralNetwork::CompiledNN encoder;
  NeuralNetwork::CompiledNN classifier;
  NeuralNetwork::CompiledNN corrector;

  std::unique_ptr<NeuralNetwork::Model> encModel;
  std::unique_ptr<NeuralNetwork::Model> clModel;
  std::unique_ptr<NeuralNetwork::Model> corModel;

  std::size_t patchSize = 0;
  bool useColorEncoder = false;

  // Temporal hysteresis for circle fallback
  Vector2f circleLastPos = Vector2f::Zero();
  int      circleStreak  = 0;

  // Raw patch streaming: holds the last extracted patch for CameraStreamer
  std::vector<float> lastExtractedPatch;  // filled by extractColorPatch() each call
  Vector2i           lastExtractedSpot;
  int                lastExtractedArea = 0;
  // Best-candidate patch this frame (set in update() when new best is found)
  std::vector<float> bestFramePatch;
  Vector2i           bestFrameSpot;
  int                bestFrameArea = 0;
  bool               bestFrameValid = false;

  void update(BallPercept& theBallPercept) override;
  void update(RawBallPatch& theRawBallPatch) override;
  float apply(const Vector2i& ballSpot, Vector2f& ballPosition, float& predRadius);
  void compile();
  float computeCircleScore(const Vector2i& center, float radius) const;
  float computeColorDiversity(const Vector2i& center, float radius) const;

  /**
   * Extracts a YCrCb color patch centered at ballSpot and writes it directly
   * into encoder.input(0).data() as float HWC [patchSize, patchSize, 3].
   * Channel order: Y, Cr(=V), Cb(=U) — matches Python cv2.COLOR_BGR2YCrCb.
   * Each channel is normalized independently with normalizeBrightness.
   */
  void extractColorPatch(const Vector2i& ballSpot, int ballArea);
};
