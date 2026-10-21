/**
 * @file Tools/Maze/MazeBlockedWalk.h
 *
 * Whether a walk is blocked by a wall that the map did not know, free of representations and modules so that it can be unit
 * tested. Odometry and the pose follow the commanded walk, so they cannot tell (MZ-046, measured in SimRobot: a blocked walk
 * keeps its commanded speed in the pose). What does tell is the gyroscope: a robot that pushes against a panel rocks
 * (standard deviation of the yaw rate over 250 ms: 0.06..0.19 rad/s walking free, 1 rad/s and more blocked), and, on the robot,
 * a contact of the foot bumpers while a forward walk is commanded.
 *
 * Rule, evaluated per frame:
 *  - alarm: a forward walk is commanded (at least `minWalkSpeed`) and the deviation of the yaw rate over `gyroWindowMs`
 *    exceeds `gyroStdThreshold`. It blocks once it held for `blockedTimeoutMs` without a gap, which also ignores the jolt of
 *    the start and the stop of a walk (about 120 ms).
 *  - bumper: contact while a forward walk is commanded blocks once it held for `bumperHoldMs`.
 * The commanded speed of a walk dips under `minWalkSpeed` for up to ~300 ms now and then (measured in SimRobot, also while the
 * robot pushes against a panel), so the gyroscope alarm survives a dip of up to `walkGapMs`; the bumper does not.
 * A standing robot (nothing commanded, or a speed under `minWalkSpeed`) is never blocked, whatever the gyroscope says.
 * The result is latched until `reset`, which has to be called when an action starts. The clock is injected by the caller.
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include <deque>

namespace Maze
{
  STREAMABLE(BlockedWalkParameters,
  {,
    (unsigned)(250) gyroWindowMs,       /**< Window of the deviation of the yaw rate (ms). */
    (float)(0.5f) gyroStdThreshold,     /**< Deviation of the yaw rate (rad/s) above which the walk looks blocked. */
    (float)(100.f) minWalkSpeed,        /**< A walk slower than this (commanded, mm/s) is not a walk that can be blocked. */
    (unsigned)(1000) blockedTimeoutMs,  /**< How long the gyroscope alarm has to hold without a gap (ms). */
    (unsigned)(100) bumperHoldMs,       /**< How long the bumper contact has to hold without a gap (ms). */
    (unsigned)(400) walkGapMs,          /**< How long the commanded speed may stay under `minWalkSpeed` before the gyroscope alarm is dropped (ms). */
  });

  /** What the detector needs to know about one frame. */
  struct BlockedWalkInput
  {
    unsigned timeMs = 0;
    bool walkCommanded = false; /**< The motion is walking (as opposed to standing, turning on the spot or kicking). */
    float forwardSpeed = 0.f;   /**< Commanded forward speed (mm/s). */
    float gyroZ = 0.f;          /**< Yaw rate (rad/s). */
    bool bumperContact = false; /**< A foot bumper is pressed. */
  };

  /** What made the walk blocked. */
  enum class BlockedCause
  {
    none,
    gyro,
    bumper,
  };

  class BlockedWalkDetector
  {
  public:
    explicit BlockedWalkDetector(const BlockedWalkParameters& parameters = BlockedWalkParameters()) : parameters(parameters) {}

    /** A new action begins: forgets the samples, the holds and a latched result. */
    void reset();

    /** @return Whether the walk is blocked (latched until `reset`). A clock that went back counts as a reset. */
    bool update(const BlockedWalkInput& input);

    [[nodiscard]] bool blocked() const { return cause != BlockedCause::none; }
    [[nodiscard]] BlockedCause blockedBy() const { return cause; }

    /** Deviation of the yaw rate over the window of the last `update`, 0 if there are too few samples. */
    [[nodiscard]] float gyroDeviation() const { return deviation; }

    BlockedWalkParameters parameters;

  private:
    struct Sample
    {
      unsigned time;
      float gyroZ;
    };

    std::deque<Sample> samples;
    unsigned lastTime = 0;
    bool hasTime = false;
    bool hasWalked = false;
    unsigned lastWalking = 0;
    bool gyroAlarm = false;
    unsigned gyroSince = 0;
    bool bumperAlarm = false;
    unsigned bumperSince = 0;
    float deviation = 0.f;
    BlockedCause cause = BlockedCause::none;
  };
}
