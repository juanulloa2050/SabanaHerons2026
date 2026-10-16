/**
 * @file Tools/Maze/MazeLocalization.cpp
 *
 * Implementation of the maze pose filter, see the header for the model.
 */

#include "MazeLocalization.h"
#include "MazeHeading.h"
#include "MazeOracle.h"
#include <algorithm>
#include <cmath>

namespace Maze
{
  void MazeLocalization::reset(int startCell, Heading startHeading, float gyroYaw, unsigned now)
  {
    pose = Pose2f(angleOf(startHeading), Oracle::cellCentreOf(cellRow(startCell), cellCol(startCell)));
    gyroOffset = Angle::normalize(gyroYaw - pose.rotation);
    lastGyroYaw = gyroYaw;
    lastFixTime = now;
    distanceSinceFix = 0.f;
    headingSource = HeadingSource::gyro;
    headingDrift = 0.f;
  }

  void MazeLocalization::predict(const Pose2f& odometryDelta, float gyroYaw, bool useGyro)
  {
    pose.translation += Oracle::rotated(odometryDelta.translation, pose.rotation);
    distanceSinceFix += odometryDelta.translation.norm();
    lastGyroYaw = gyroYaw;
    if(useGyro)
    {
      pose.rotation = Angle::normalize(gyroYaw - gyroOffset);
      headingSource = HeadingSource::gyro;
    }
    else
    {
      pose.rotation = Angle::normalize(pose.rotation + odometryDelta.rotation);
      headingSource = HeadingSource::odometry;
    }
  }

  TagFix MazeLocalization::fixWithTag(int id, const Pose3f& tagInRobot, unsigned now, const MazeTagLayout& layout)
  {
    TagFix fix;
    if(id < 0 || id >= cellCount)
      return fix;

    const Vector2f normalInRobot = tagInRobot.rotation.col(2).head<2>();
    const Vector2f positionInRobot = tagInRobot.translation.head<2>();
    if(normalInRobot.norm() < 1e-3f)
      return fix;

    // Which face is it? The one whose expected normal, into the cell, is closest to the measured normal.
    const float measuredNormalAngle = pose.rotation + std::atan2(normalInRobot.y(), normalInRobot.x());
    float bestError = pi;
    Heading bestFace = Heading::north;
    bool found = false;
    for(const Heading face : layout.candidateFaces(id))
    {
      const float error = std::abs(Angle::normalize(measuredNormalAngle - angleOf(turned(face, 2))));
      if(!found || error < bestError)
      {
        found = true;
        bestError = error;
        bestFace = face;
      }
    }
    if(!found)
      return fix;
    fix.faceAngleError = bestError;
    if(bestError > parameters.maxFaceAngleError)
      return fix;
    fix.face = bestFace;
    fix.accepted = true;

    // Heading that makes the measured normal agree with the expected one, and the position that follows from it.
    const float measuredRotation = Angle::normalize(angleOf(turned(bestFace, 2)) - std::atan2(normalInRobot.y(), normalInRobot.x()));
    const Vector2f tagInMaze = Oracle::cellCentreOf(cellRow(id), cellCol(id)) + Oracle::directionOf(bestFace) * Oracle::halfCellMm;
    const Vector2f measuredPosition = tagInMaze - Oracle::rotated(positionInRobot, measuredRotation);

    const float distance = positionInRobot.norm();
    const float t = std::clamp(distance / parameters.farDistance, 0.f, 1.f);
    const float gain = parameters.translationGain * (1.f + (parameters.farGainScale - 1.f) * t);
    pose.translation += gain * (measuredPosition - pose.translation);

    // A tag seen square-on gives a good heading: the camera ray runs against the normal.
    const float squareOn = distance > 1.f ? std::acos(std::clamp(-positionInRobot.normalized().dot(normalInRobot.normalized()), -1.f, 1.f)) : 0.f;
    if(squareOn <= parameters.squareOnAngle)
    {
      const float correction = parameters.rotationGain * Angle::normalize(measuredRotation - pose.rotation);
      pose.rotation = Angle::normalize(pose.rotation + correction);
      gyroOffset = Angle::normalize(lastGyroYaw - pose.rotation);
      headingDrift += std::abs(correction);
      headingSource = HeadingSource::tag;
      fix.headingCorrected = true;
    }

    lastFixTime = now;
    distanceSinceFix = 0.f;
    return fix;
  }

  LocalizationQuality MazeLocalization::quality(unsigned now) const
  {
    const float elapsed = static_cast<float>(now - lastFixTime);
    if(elapsed <= parameters.superbMaxTime && distanceSinceFix <= parameters.superbMaxDistance)
      return LocalizationQuality::superb;
    if(elapsed <= parameters.okayMaxTime && distanceSinceFix <= parameters.okayMaxDistance)
      return LocalizationQuality::okay;
    return LocalizationQuality::poor;
  }

  bool MazeLocalization::cell(int& row, int& col) const
  {
    if(Oracle::cellContaining(pose.translation, row, col))
      return true;
    col = std::clamp(static_cast<int>(std::floor(pose.translation.x() / cellSizeMm)), 0, size - 1);
    row = size - 1 - std::clamp(static_cast<int>(std::floor(pose.translation.y() / cellSizeMm)), 0, size - 1);
    return false;
  }

  Heading MazeLocalization::heading() const
  {
    return headingOf(pose.rotation);
  }
}
