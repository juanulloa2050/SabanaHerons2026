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
  FaceChoice chooseFace(int id, float robotRotation, const Pose3f& tagInRobot, const MazeTagLayout& layout)
  {
    FaceChoice choice;
    if(id < 0 || id >= cellCount)
      return choice;
    const Vector2f normalInRobot = tagInRobot.rotation.col(2).head<2>();
    if(!normalInRobot.allFinite() || normalInRobot.norm() < 1e-3f)
      return choice;

    // Which face is it? The one whose expected normal, into the cell, is closest to the measured normal.
    const float measuredNormalAngle = robotRotation + std::atan2(normalInRobot.y(), normalInRobot.x());
    for(const Heading face : layout.candidateFaces(id))
    {
      const float error = std::abs(Angle::normalize(measuredNormalAngle - angleOf(turned(face, 2))));
      if(!choice.found || error < choice.angleError)
      {
        choice.found = true;
        choice.angleError = error;
        choice.face = face;
      }
    }
    return choice;
  }

  void MazeLocalization::reset(int startCell, Heading startHeading, float gyroYaw, unsigned now)
  {
    pose = Pose2f(angleOf(startHeading), Oracle::cellCentreOf(cellRow(startCell), cellCol(startCell)));
    gyroOffset = Angle::normalize(gyroYaw - pose.rotation);
    lastGyroYaw = gyroYaw;
    lastFixTime = now;
    distanceSinceFix = 0.f;
    headingSource = HeadingSource::gyro;
    gyroOffsetAtReset = gyroOffset;
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
    // A non-finite pose would poison the filter for good.
    if(!tagInRobot.translation.allFinite() || !tagInRobot.rotation.allFinite())
      return fix;

    const FaceChoice choice = chooseFace(id, pose.rotation, tagInRobot, layout);
    if(!choice.found)
      return fix;
    const Vector2f normalInRobot = tagInRobot.rotation.col(2).head<2>();
    const Vector2f positionInRobot = tagInRobot.translation.head<2>();
    const float bestError = choice.angleError;
    const Heading bestFace = choice.face;
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
      headingSource = HeadingSource::tag;
      fix.headingCorrected = true;
    }

    lastFixTime = now;
    distanceSinceFix = 0.f;
    return fix;
  }

  LocalizationQuality MazeLocalization::quality(unsigned now) const
  {
    // A clock that went backwards counts as no time elapsed instead of wrapping around.
    const float elapsed = now >= lastFixTime ? static_cast<float>(now - lastFixTime) : 0.f;
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
