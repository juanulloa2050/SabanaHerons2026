/**
 * @file PerceptionProviders.cpp
 *
 * This file implements all modules that provide representations from perception
 * for the current Cognition frame.
 *
 * @author Thomas Röfer
 */

#include "PerceptionProviders.h"
#include "Representations/Infrastructure/CameraStatus.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Perception/FieldPercepts/CirclePercept.h"
#include "Representations/Perception/FieldPercepts/FieldLines.h"
#include "Representations/Perception/FieldPercepts/FieldLineIntersections.h"
#include "Representations/Perception/FieldPercepts/IntersectionsPercept.h"
#include "Representations/Perception/FieldPercepts/PenaltyMarkPercept.h"
#include "Representations/Perception/FieldPercepts/LinesPercept.h"
#include "Representations/Perception/ImagePreprocessing/BodyContour.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/ImagePreprocessing/ECImage.h"
#include "Representations/Perception/ImagePreprocessing/FieldBoundary.h"
#include "Representations/Perception/MazePercepts/AprilTagPercept.h"
#include "Representations/Perception/ObstaclesPercepts/ObstaclesFieldPercept.h"

MAKE_MODULE(PerceptionImageCoordinateSystemProvider);

void PerceptionImageCoordinateSystemProvider::update(ImageCoordinateSystem& theImageCoordinateSystem)
{
  if(Cognition::isUpper)
  {
    theImageCoordinateSystem = theUpperImageCoordinateSystem;
    theImageCoordinateSystem.cameraInfo = theCameraInfo;
  }
  else
  {
    theImageCoordinateSystem = theLowerImageCoordinateSystem;
    theImageCoordinateSystem.cameraInfo = theCameraInfo;
  }
}

// AprilTags are only detected in the upper camera thread. Cognition may skip an upper frame
// (see Cognition.cpp: it drops frames older than the last accepted one, and a slow upper thread
// loses against a fast, idle lower thread), but the data of that frame is still received and
// stored in UpperAprilTagPercept. Therefore, always copy the newest upper result, in every frame
// and independent of Cognition::isUpper. As a consequence, the same detections may repeat over
// several Cognition frames; consumers distinguish them by the timestamp.
MAKE_MODULE(PerceptionAprilTagPerceptProvider);

void PerceptionAprilTagPerceptProvider::update(AprilTagPercept& theAprilTagPercept)
{
  theAprilTagPercept = theUpperAprilTagPercept;
}

// Define a module that updates a representation with the newer one of both aliases
#define ALIAS_MODULE(Representation) \
  MODULE(Perception##Representation##Provider, \
  {, \
    SELECTS(Representation), \
  }); \
  \
  class Perception##Representation##Provider : public Perception##Representation##ProviderBase \
  { \
    void update(Representation& the##Representation) override \
    { \
      if(Cognition::isUpper) \
        the##Representation = theUpper##Representation; \
      else \
        the##Representation = theLower##Representation; \
    } \
  }; \
  \
  MAKE_MODULE(Perception##Representation##Provider)

// Also declare alias representations
#define ALIAS(Representation) \
  DECLARE(Representation); \
  \
  ALIAS_MODULE(Representation)

ALIAS_MODULE(FrameInfo);
ALIAS(BallPercept);
ALIAS(BodyContour);
ALIAS(CameraInfo);
ALIAS(CameraMatrix);
ALIAS(CameraStatus);
ALIAS(CirclePercept);
ALIAS(FieldBoundary);
ALIAS(FieldLines);
ALIAS(FieldLineIntersections);
ALIAS(IntersectionsPercept);
ALIAS(LinesPercept);
ALIAS(ObstaclesFieldPercept);
ALIAS(OptionalECImage);
ALIAS(PenaltyMarkPercept);
ALIAS(RobotCameraMatrix);
