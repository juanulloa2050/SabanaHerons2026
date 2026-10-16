/**
 * @file MazeLocalizer.h
 *
 * Self-localization in the maze; replaces the soccer SelfLocator in the Maze scenarios. It provides
 * the `RobotPose` in the maze frame (mm, x east, y north, origin at the outer bottom-left corner,
 * rotation counterclockwise from east, so maze north is +90 degrees) and the `MazeCellPose` (cell,
 * heading, source of the heading, tag statistics).
 *
 * The pose is predicted every frame from the odometry delta and, if `useGyro`, from the gyro yaw of the
 * torso (counterclockwise positive, like the odometry rotation). AprilTag detections correct it; the
 * filter itself is in Tools/Maze/MazeLocalization.h and has its own unit tests.
 *
 * Start: until MZ-041 (start deduction) and MZ-024 (`start.cell`) exist, the robot is "positioned at
 * start": on the first frame and on every transition into playing the pose is reset to `startCell`
 * and `startHeading` and the gyro is re-referenced (`resetToStart`). Standby and pick-up therefore put
 * the localizer back at the start while MazeMapper keeps the map.
 *
 * Simulation test knobs: SimRobot has neither gyro drift nor odometry noise. `injectedGyroDriftPerMinute`
 * and the `injectedOdometry*` parameters add them so that the corrections can be tested in the simulator.
 * They must be 0 on the real robot.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/MazeCellPose.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/MotionControl/OdometryData.h"
#include "Representations/Perception/MazePercepts/AprilTagPercept.h"
#include "Representations/Sensing/InertialData.h"
#include "Tools/Maze/MazeLocalization.h"
#include "Tools/Maze/MazeSession.h"
#include "Tools/Maze/MazeTagLayout.h"

MODULE(MazeLocalizer,
{,
  REQUIRES(AprilTagPercept),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(InertialData),
  REQUIRES(OdometryData),
  PROVIDES(MazeCellPose),
  PROVIDES(RobotPose),
  LOADS_PARAMETERS(
  {,
    (int) startCell,                          /**< Cell the robot starts in (row * 8 + column). Replaced by `start.cell` of MZ-024. */
    (MazeCellPose::Heading) startHeading,     /**< Maze heading the robot faces at the start. */
    (bool) useGyro,                           /**< Take the heading from the gyro; otherwise integrate the odometry rotation. */
    (Angle) maxFaceAngleError,                /**< Largest angle between the measured and the expected tag normal; larger detections are rejected. */
    (float) translationGain,                  /**< Share of the position error that a tag fix corrects at zero distance. */
    (float) farDistance,                      /**< Tag distance (mm) at which the translation gain is scaled by `farGainScale`. */
    (float) farGainScale,                     /**< Scale of the translation gain at `farDistance` and beyond. */
    (float) rotationGain,                     /**< Share of the heading error that a square-on tag fix corrects. */
    (Angle) squareOnAngle,                    /**< Largest angle between the camera ray and the tag normal for the heading correction. */
    (float) superbMaxTime,                    /**< Quality is superb up to this time since the last fix (ms) ... */
    (float) superbMaxDistance,                /**< ... and this walked distance (mm). */
    (float) okayMaxTime,                      /**< Quality is okay up to this time (ms) ... */
    (float) okayMaxDistance,                  /**< ... and this distance (mm); beyond it is poor. */
    (float) injectedGyroDriftPerMinute,       /**< Simulation only: degrees per minute that are added to the gyro yaw. 0 on the robot. */
    (float) injectedOdometryTranslationNoise, /**< Simulation only: standard deviation of the relative error of each odometry translation delta (fraction). 0 on the robot. */
    (Angle) injectedOdometryRotationNoise,    /**< Simulation only: standard deviation of the rotation error per metre walked, added to the odometry rotation (only matters without the gyro). 0 on the robot. */
  }),
});

/**
 * @class MazeLocalizer
 * Pose in the maze frame and in cells.
 */
class MazeLocalizer : public MazeLocalizerBase
{
public:
  MazeLocalizer();

private:
  void update(RobotPose& robotPose) override;
  void update(MazeCellPose& mazeCellPose) override;

  /** Runs the filter once per frame; both `update` functions read its result. */
  void step();

  /** The robot is positioned at the start: hook for MZ-041 and MZ-024 to put a deduced start here. */
  void resetToStart(float gyroYaw);

  /** The current gyro yaw in rad, counterclockwise positive, including the injected drift. */
  float gyroYaw() const;

  /** Copies the loaded parameters to the filter. */
  void applyParameters();

  Maze::MazeLocalization localization;
  Maze::MazeTagLayout layout;
  Maze::TimestampFilter tagFilter;
  Pose2f lastOdometry;
  bool hasLastOdometry = false;
  Maze::PlayingTracker session;   /**< Decides when the robot is positioned at the start. */
  bool stepped = false;           /**< `step` ran for `lastStepTime`. */
  unsigned lastStepTime = 0;
  float injectedGyroDrift = 0.f;  /**< Accumulated injected drift in rad. */
  unsigned tagFixes = 0;          /**< Accepted tag fixes since the last reset. */
  unsigned tagRejections = 0;     /**< Detections that did not correct the pose since the last reset. */
  unsigned resets = 0;
  unsigned timestampLastJump = 0;
};
