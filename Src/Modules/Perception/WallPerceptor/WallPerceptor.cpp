/**
 * @file WallPerceptor.cpp
 *
 * Implementation of the wall perceptor, see WallPerceptor.h.
 */

#include "WallPerceptor.h"
#include "Debugging/DebugDrawings.h"
#include "Debugging/Stopwatch.h"
#include "Math/Eigen.h"
#include "Tools/Math/Transformation.h"
#include "Tools/Maze/MazeWallScan.h"
#include "Tools/Maze/MazeWallView.h"
#include <algorithm>
#include <cmath>

MAKE_MODULE(WallPerceptor);

int WallPerceptor::horizonRow() const
{
  const int column = theCameraInfo.width / 2;
  return Maze::WallScan::horizonRow(theCameraInfo.height, [&](int row, float& distanceMm)
  {
    Vector2f floorPoint;
    if(!Transformation::imageToRobot(static_cast<float>(column), static_cast<float>(row), theCameraMatrix, theCameraInfo, floorPoint))
      return false;
    distanceMm = (floorPoint - theCameraMatrix.translation.head<2>()).norm();
    return true;
  }, maxScanDistanceMm);
}

void WallPerceptor::update(WallPercept& wallPercept)
{
  DECLARE_DEBUG_DRAWING("module:WallPerceptor:boundary", "drawingOnImage");
  DECLARE_DEBUG_DRAWING("module:WallPerceptor:points", "drawingOnField");

  wallPercept = WallPercept();
  wallPercept.timestamp = theFrameInfo.time;

  // Same signal as the MazeLocalizer pose reset: the first frame and every transition into active.
  if(tracker.update(Maze::isActive(theGameState.isPlaying(), theGameState.isPenalized(), theFallDownState.state)).resetPose())
    floorModel.reset();
  floorModel.params = {floorTolerance, floorTrackingRate, static_cast<unsigned>(std::max(0, floorMaxAgeMs))};

  const Image<PixelTypes::GrayscaledPixel>& gray = theECImage.grayscaled;
  if(gray.width == 0 || gray.height == 0 || theECImage.timestamp <= 10 || theCameraInfo.focalLength <= 0.f || !theCameraMatrix.isValid ||
     static_cast<int>(gray.width) != theCameraInfo.width || static_cast<int>(gray.height) != theCameraInfo.height)
    return;
  const int horizon = horizonRow();
  if(horizon < 0)
    return;

  STOPWATCH("WallPerceptor")
  {
    Maze::WallScan::Params scanParams;
    scanParams.columnStep = columnStep;
    scanParams.smoothRows = smoothRows;
    scanParams.columnHalfWidth = columnHalfWidth;
    scanParams.minGradient = minGradient;
    scanParams.minWallContrast = minWallContrast;
    scanParams.minWallCheckHeightMm = minWallCheckHeightMm;
    scanParams.minWallRowFraction = minWallRowFraction;
    scanParams.maxFloorRunRows = maxFloorRunRows;
    scanParams.minCheckRows = minCheckRows;
    scanParams.floorReferenceRows = floorReferenceRows;

    // The point `heightMm` above a floor pixel, back in the image.
    const Maze::WallScan::HeightProjection projection = [this](int column, int row, float heightMm, int& rowAtHeight)
    {
      Vector2f floorPoint, pointInImage;
      if(!Transformation::imageToRobot(static_cast<float>(column), static_cast<float>(row), theCameraMatrix, theCameraInfo, floorPoint) ||
         !Transformation::robotToImage(Vector3f(floorPoint.x(), floorPoint.y(), heightMm), theCameraMatrix, theCameraInfo, pointInImage))
        return false;
      rowAtHeight = static_cast<int>(std::lround(pointInImage.y()));
      return true;
    };

    const Maze::WallScan::GreyImage image = {reinterpret_cast<const unsigned char*>(gray[0]), static_cast<int>(gray.width), static_cast<int>(gray.height), static_cast<int>(gray.width)};
    float floorReference = 0.f;
    const std::vector<Maze::WallScan::ColumnBoundary> boundaries = Maze::WallScan::scanColumns(image, horizon, projection, scanParams, &floorReference);

    floorModel.observe(boundaries, floorReference, theFrameInfo.time);

    std::vector<Vector2f> boundaryPoints, clearPoints;
    for(const Maze::WallScan::ColumnBoundary& boundary : boundaries)
    {
      Vector2f point;
      if(boundary.found())
      {
        if(Transformation::imageToRobot(static_cast<float>(boundary.column), static_cast<float>(boundary.row), theCameraMatrix, theCameraInfo, point))
          boundaryPoints.push_back(point);
      }
      else if(floorModel.isFloor(boundary.bottom, theFrameInfo.time) &&
              Transformation::imageToRobot(static_cast<float>(boundary.column), static_cast<float>(horizon), theCameraMatrix, theCameraInfo, point))
        clearPoints.push_back(point);
    }

    COMPLEX_DRAWING("module:WallPerceptor:boundary")
    {
      for(const Maze::WallScan::ColumnBoundary& boundary : boundaries)
        if(boundary.found())
          CROSS("module:WallPerceptor:boundary", boundary.column, boundary.row, 4, 2, Drawings::solidPen, ColorRGBA::green);
        else
          CROSS("module:WallPerceptor:boundary", boundary.column, horizon, 4, 2, Drawings::solidPen,
                floorModel.isFloor(boundary.bottom, theFrameInfo.time) ? ColorRGBA::red : ColorRGBA::yellow);
    }
    COMPLEX_DRAWING("module:WallPerceptor:points")
    {
      for(const Vector2f& point : boundaryPoints)
        CIRCLE("module:WallPerceptor:points", point.x(), point.y(), 20, 5, Drawings::solidPen, ColorRGBA::green, Drawings::solidBrush, ColorRGBA::green);
      for(const Vector2f& point : clearPoints)
        CIRCLE("module:WallPerceptor:points", point.x(), point.y(), 20, 5, Drawings::solidPen, ColorRGBA::red, Drawings::solidBrush, ColorRGBA::red);
    }

    Maze::WallView::Params viewParams;
    viewParams.snapToleranceMm = snapToleranceMm;
    viewParams.minSupportPoints = minSupportPoints;
    viewParams.openClearanceMm = openClearanceMm;
    viewParams.vertexMarginMm = vertexMarginMm;

    const Vector3f gazeInRobot = theCameraMatrix.rotation * Vector3f(1.f, 0.f, 0.f);
    const int range = Maze::WallView::lookAheadRange(theMotionInfo.executedPhase == MotionPhase::stand, lookAheadRangeWalking, lookAheadRangeStanding);
    Maze::WallView::Result result;
    if(!Maze::WallView::computeWallView(boundaryPoints, clearPoints, theRobotPose, theCameraMatrix.translation.head<2>(),
                                        std::atan2(gazeInRobot.y(), gazeInRobot.x()), range, viewParams, result))
      return;

    wallPercept.frontDistance = result.frontDistance;
    wallPercept.leftDistance = result.leftDistance;
    wallPercept.rightDistance = result.rightDistance;
    wallPercept.lookAheadTurns = result.lookAheadTurns;
    for(const Maze::WallView::Segment& segment : result.segments)
    {
      WallPercept::Segment entry;
      entry.start = segment.start;
      entry.end = segment.end;
      wallPercept.segments.push_back(entry);
    }
    for(const Maze::WallView::Cell& cell : result.cellsAhead)
    {
      WallPercept::CellAhead entry;
      entry.ahead = cell.ahead;
      entry.left = static_cast<WallPercept::Edge>(cell.left);
      entry.right = static_cast<WallPercept::Edge>(cell.right);
      entry.front = static_cast<WallPercept::Edge>(cell.front);
      entry.distance = cell.distance;
      wallPercept.cellsAhead.push_back(entry);
    }
  }
}
