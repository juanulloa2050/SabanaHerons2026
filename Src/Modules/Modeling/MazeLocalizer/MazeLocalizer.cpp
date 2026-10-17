/**
 * @file MazeLocalizer.cpp
 *
 * Implementation of the maze localizer, see the header.
 */

#include "MazeLocalizer.h"
#include "Debugging/DebugDrawings.h"
#include "Debugging/Debugging.h"
#include "Math/Random.h"
#include "Math/Rotation.h"
#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeOracle.h"
#include <algorithm>
#include <cmath>

MAKE_MODULE(MazeLocalizer);

MazeLocalizer::MazeLocalizer()
{
  if(!Maze::MazeTagLayout::load("mazeTagLayout.cfg", layout))
    OUTPUT_WARNING("MazeLocalizer: mazeTagLayout.cfg not found, using a tag on every wall face.");
}

void MazeLocalizer::applyParameters()
{
  Maze::LocalizationParameters& p = localization.parameters;
  p.maxFaceAngleError = maxFaceAngleError;
  p.translationGain = translationGain;
  p.farDistance = farDistance;
  p.farGainScale = farGainScale;
  p.rotationGain = rotationGain;
  p.squareOnAngle = squareOnAngle;
  p.superbMaxTime = superbMaxTime;
  p.superbMaxDistance = superbMaxDistance;
  p.okayMaxTime = okayMaxTime;
  p.okayMaxDistance = okayMaxDistance;
}

float MazeLocalizer::gyroYaw() const
{
  return Angle::normalize(Rotation::Euler::getZAngle(theInertialData.orientation3D) + injectedGyroDrift);
}

void MazeLocalizer::followStart()
{
  const Maze::Hypothesis working = {std::clamp(theMazeStart.cell, 0, Maze::cellCount - 1), static_cast<Maze::Heading>(static_cast<int>(theMazeStart.heading))};
  if(working == frame)
    return;
  localization.transform(Maze::StartDeduction::startToMaze(working) * Maze::StartDeduction::startToMaze(frame).inverse());
  frame = working;
  timestampLastJump = theFrameInfo.time; // The pose jumped into another frame.
}

void MazeLocalizer::resetToStart(float yaw)
{
  localization.reset(frame.cell, frame.heading, yaw, theFrameInfo.time);
  tagFixes = 0;
  tagRejections = 0;
  tagFilter.reset();
  ++resets;
  timestampLastJump = theFrameInfo.time;
}

void MazeLocalizer::step()
{
  if(stepped && lastStepTime == theFrameInfo.time)
    return;
  const unsigned previousStepTime = lastStepTime;
  const bool hadStepped = stepped;
  stepped = true;
  lastStepTime = theFrameInfo.time;
  applyParameters();

  // The injected drift grows with the time between two frames.
  if(hadStepped && injectedGyroDriftPerMinute != 0.f)
    injectedGyroDrift += Angle::fromDegrees(injectedGyroDriftPerMinute) * static_cast<float>(theFrameInfo.time - previousStepTime) / 60000.f;
  const float yaw = gyroYaw();

  // The mapper may have changed the working start hypothesis in the last frame.
  followStart();

  // Positioned at start: on the first frame and whenever the robot becomes active (playing, on the ground).
  if(session.update(Maze::isActive(theGameState.isPlaying(), theFallDownState.state == FallDownState::pickedUp)).resetPose())
    resetToStart(yaw);

  // The odometry delta in the robot frame of the last frame, with the optional simulated noise.
  Pose2f delta;
  if(hasLastOdometry)
    delta = lastOdometry.inverse() * theOdometryData;
  lastOdometry = theOdometryData;
  hasLastOdometry = true;
  if(injectedOdometryTranslationNoise > 0.f)
    delta.translation *= std::max(0.f, Random::normal(1.f, injectedOdometryTranslationNoise));
  if(injectedOdometryRotationNoise > 0.f)
    delta.rotation += Random::normal(0.f, static_cast<float>(injectedOdometryRotationNoise)) * delta.translation.norm() / 1000.f;
  localization.predict(delta, yaw, useGyro);

  // Tags: Cognition repeats percepts, so only a new timestamp is a new observation. Under a start that is not known yet
  // a tag could pull the pose to the wrong place, so the percept waits (and is used if it is still the newest) until it is.
  if(theMazeStart.known && tagFilter.accept(theAprilTagPercept.timestamp))
    for(const AprilTagPercept::Detection& detection : theAprilTagPercept.detections)
    {
      if(localization.fixWithTag(detection.id, detection.tagInRobot, theFrameInfo.time, layout).accepted)
        ++tagFixes;
      else
        ++tagRejections;
    }

  DEBUG_DRAWING("module:MazeLocalizer:pose", "drawingOnField")
  {
    // In the maze frame, like the RobotPose.
    DRAW_ROBOT_POSE("module:MazeLocalizer:pose", localization.pose, ColorRGBA::blue);
    int row, col;
    const bool inside = localization.cell(row, col);
    const Maze::CellCentre centre = Maze::cellCentre(row, col);
    const float half = Maze::cellSizeMm / 2.f - 10.f;
    RECTANGLE("module:MazeLocalizer:pose", centre.x - half, centre.y - half, centre.x + half, centre.y + half, 10, Drawings::dashedPen,
              inside ? ColorRGBA::blue : ColorRGBA::red);
    DRAW_TEXT("module:MazeLocalizer:pose", localization.pose.translation.x() + 120.f, localization.pose.translation.y() + 120.f, 60, ColorRGBA::blue,
              "fixes " << tagFixes << " drift " << Angle(localization.headingDrift()).toDegrees() << "deg");
  }
}

void MazeLocalizer::update(RobotPose& robotPose)
{
  step();
  robotPose = localization.pose;
  switch(localization.quality(theFrameInfo.time))
  {
    case Maze::LocalizationQuality::superb: robotPose.quality = RobotPose::LocalizationQuality::superb; break;
    case Maze::LocalizationQuality::okay: robotPose.quality = RobotPose::LocalizationQuality::okay; break;
    default: robotPose.quality = RobotPose::LocalizationQuality::poor; break;
  }
  // The error grows with the distance walked since the last tag fix; the position and the heading are independent.
  const float stdTranslation = 30.f + 0.06f * localization.distanceSinceFix;
  const float stdRotation = Angle::fromDegrees(2.f + localization.distanceSinceFix / 1000.f);
  robotPose.covariance = Matrix3f::Zero();
  robotPose.covariance(0, 0) = stdTranslation * stdTranslation;
  robotPose.covariance(1, 1) = stdTranslation * stdTranslation;
  robotPose.covariance(2, 2) = stdRotation * stdRotation;
  robotPose.timestampLastJump = timestampLastJump;
}

void MazeLocalizer::update(MazeCellPose& mazeCellPose)
{
  step();
  int row = 0, col = 0;
  mazeCellPose.insideMaze = localization.cell(row, col);
  mazeCellPose.row = row;
  mazeCellPose.col = col;
  mazeCellPose.heading = static_cast<MazeCellPose::Heading>(static_cast<int>(localization.heading()));
  switch(localization.quality(theFrameInfo.time))
  {
    case Maze::LocalizationQuality::superb: mazeCellPose.quality = RobotPose::LocalizationQuality::superb; break;
    case Maze::LocalizationQuality::okay: mazeCellPose.quality = RobotPose::LocalizationQuality::okay; break;
    default: mazeCellPose.quality = RobotPose::LocalizationQuality::poor; break;
  }
  mazeCellPose.headingSource = static_cast<MazeCellPose::HeadingSource>(static_cast<int>(localization.headingSource));
  mazeCellPose.headingDrift = localization.headingDrift();
  mazeCellPose.lastTagFixTime = localization.lastFixTime;
  mazeCellPose.tagFixes = tagFixes;
  mazeCellPose.tagRejections = tagRejections;
  mazeCellPose.resets = resets;
}
