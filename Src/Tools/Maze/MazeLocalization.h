/**
 * @file Tools/Maze/MazeLocalization.h
 *
 * The pose filter of the maze localizer, free of representations and modules so that it can be unit tested.
 * The pose lives in the maze frame (mm, x east, y north, origin at the outer bottom-left corner, rotation
 * counterclockwise from east). It is predicted from odometry deltas and the gyro heading and corrected by
 * AprilTag detections, whose id is the cell index (roadmap 4.5).
 *
 * The gyro yaw is expected counterclockwise positive like the pose rotation. The gyro heading is
 * `gyroYaw - gyroOffset`; `reset` sets the offset so that it equals the start heading, and every square-on
 * tag fix re-aligns it so that the gyro agrees with the corrected pose.
 *
 * Tag model: the tag sits at the centre of a wall face, `Oracle::halfCellMm` from the cell centre, and its
 * z-axis (the normal of `tagInRobot`) points out of the wall into the cell.
 */

#pragma once

#include "MazeGraph.h"
#include "MazeTagLayout.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include "Math/Pose3f.h"

namespace Maze
{
  /** Where the heading of the pose comes from. */
  enum class HeadingSource : unsigned char
  {
    odometry, /**< Integrated odometry rotation. */
    gyro,     /**< Gyro yaw minus the gyro offset. */
    tag,      /**< Corrected by a tag since the last prediction. */
  };

  /** How much the pose can be trusted; the module maps this to `RobotPose::LocalizationQuality`. */
  enum class LocalizationQuality : unsigned char
  {
    superb,
    okay,
    poor,
  };

  struct LocalizationParameters
  {
    float maxFaceAngleError = Angle::fromDegrees(45.f); /**< Largest angle between the measured and the expected tag normal; larger detections are rejected. */
    float translationGain = 0.5f;                       /**< Share of the position error that a tag fix corrects at zero distance. */
    float farDistance = 2000.f;                         /**< Tag distance (mm) at which the translation gain is scaled by `farGainScale`. */
    float farGainScale = 0.4f;                          /**< Scale of the translation gain at `farDistance` and beyond; linear in between. */
    float rotationGain = 0.5f;                          /**< Share of the heading error that a square-on tag fix corrects. */
    float squareOnAngle = Angle::fromDegrees(25.f);     /**< Largest angle between the camera ray and the tag normal for the heading correction. */
    float superbMaxTime = 10000.f;                      /**< Quality is superb up to this time since the last fix (ms) ... */
    float superbMaxDistance = 1500.f;                   /**< ... and this walked distance (mm). */
    float okayMaxTime = 30000.f;                        /**< Quality is okay up to this time (ms) ... */
    float okayMaxDistance = 4000.f;                     /**< ... and this distance (mm); beyond it is poor. */
  };

  /** The outcome of one tag detection. */
  struct TagFix
  {
    bool accepted = false;        /**< Whether the detection corrected the pose. */
    Heading face = Heading::north; /**< The face of the cell the tag was matched to; only valid if `accepted`. */
    float faceAngleError = 0.f;   /**< Angle between the measured and the expected tag normal in rad; set if the id was valid. */
    bool headingCorrected = false; /**< Whether the tag was square-on and corrected the heading. */
  };

  /** The outcome of `chooseFace`. */
  struct FaceChoice
  {
    bool found = false;            /**< Whether the tag has a usable normal and the layout offers a face for the cell. */
    Heading face = Heading::north; /**< The candidate face whose expected normal is closest to the measured one. */
    float angleError = 0.f;        /**< Angle between the measured and the expected normal in rad; only valid if `found`. */
  };

  /**
   * Which face of cell `id` a tag detection belongs to: the candidate face of the layout whose expected
   * normal (out of the wall, into the cell) is closest to the measured normal, seen from a robot that
   * points `robotRotation` in the maze frame. The filter and the mapper share this choice. It does not
   * apply an error limit; callers compare `angleError` with `LocalizationParameters::maxFaceAngleError`.
   */
  [[nodiscard]] FaceChoice chooseFace(int id, float robotRotation, const Pose3f& tagInRobot, const MazeTagLayout& layout);

  /** Ignores a percept whose timestamp equals the one processed last (Cognition repeats percepts). */
  struct TimestampFilter
  {
    /** @return Whether the percept is new; remembers its timestamp. */
    bool accept(unsigned timestamp)
    {
      if(hasLast && timestamp == last)
        return false;
      hasLast = true;
      last = timestamp;
      return true;
    }

    void reset() { hasLast = false; }

    unsigned last = 0;
    bool hasLast = false;
  };

  class MazeLocalization
  {
  public:
    explicit MazeLocalization(const LocalizationParameters& parameters = LocalizationParameters()) : parameters(parameters) {}

    /**
     * Puts the robot at the centre of a cell, "positioned at start". The pose is known, so the quality
     * starts as superb with the fix time `now`.
     * @param gyroYaw The current gyro yaw in rad; becomes the reference of the gyro heading.
     */
    void reset(int startCell, Heading startHeading, float gyroYaw, unsigned now = 0);

    /**
     * Moves the pose.
     * @param odometryDelta Motion since the last call in the robot frame (translation in mm).
     * @param gyroYaw Gyro yaw in rad.
     * @param useGyro Take the rotation from the gyro; otherwise integrate the odometry rotation.
     */
    void predict(const Pose2f& odometryDelta, float gyroYaw, bool useGyro);

    /**
     * Corrects the pose with a detected tag.
     * @param id Tag id, which is the cell index; outside 0..63 the detection is rejected.
     * @param tagInRobot Pose of the tag in the robot frame (mm), see `Oracle::observeTag` for the axes.
     * @param now Time in ms.
     */
    TagFix fixWithTag(int id, const Pose3f& tagInRobot, unsigned now, const MazeTagLayout& layout);

    /** Quality from the time and the distance walked since the last fix. */
    [[nodiscard]] LocalizationQuality quality(unsigned now) const;

    /**
     * The cell that contains the pose.
     * @return Whether the pose is inside the maze; if not, row and col are clamped to the nearest cell.
     */
    bool cell(int& row, int& col) const;

    /** The maze heading closest to the pose rotation. */
    [[nodiscard]] Heading heading() const;

    LocalizationParameters parameters;
    Pose2f pose;                  /**< Robot pose in the maze frame. */
    float gyroOffset = 0.f;       /**< Gyro yaw minus the pose rotation. */
    float lastGyroYaw = 0.f;      /**< Gyro yaw seen by the last `reset` or `predict`. */
    unsigned lastFixTime = 0;     /**< Time of the last tag fix or of the reset. */
    float distanceSinceFix = 0.f; /**< Distance walked since the last fix in mm. */
    HeadingSource headingSource = HeadingSource::odometry;
    float headingDrift = 0.f;     /**< Sum of the absolute heading corrections by tags in rad. */
  };
}
