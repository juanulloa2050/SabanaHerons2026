/**
 * @file Tools/Maze/MazeRunTracker.h
 *
 * The measurement of a simulated maze run (MZ-021): when the head tap that starts an attempt is due, and the metrics of every
 * attempt, computed from the ground truth robot pose in the maze frame (mm, x east, y north, origin in the south-west
 * corner, a rotation of 0 looks east) and the walls of the maze file. Free of representations, modules and the
 * clock, so that it can be unit tested; MazeRunMetricsProvider feeds it and prints the result.
 *
 * Sequence: waiting for the game to play, `startTapAfterMs` later the first tap (attempt 1), and, once the attempt ended at
 * a goal and two attempts were asked for, the wait for the robot to be back in the start cell and still for `settleMs`
 * (the robot walks back on its own, see `returnToStart` in handleMaze.cfg), then the second tap (attempt 2). An attempt ends
 * when the centre of the robot enters a goal cell or after `attemptTimeoutMs`. If the return does not arrive within
 * `returnTimeoutMs`, or attempt 1 ended without the goal, attempt 2 is reported as not started. The tracker reports exactly
 * one result per requested attempt.
 *
 * Metrics of an attempt, all from the ground truth, so independent of what the robot believes:
 *  - reached goal: the robot centre is inside a goal cell before the timeout.
 *  - time: from the tap to the goal entry, or to the timeout.
 *  - wall contacts: entries into contact. The robot is in contact while the distance from its centre to the nearest wall
 *    (the centre line of an existing edge, borders included) is below `contactRadius`; it leaves contact only above
 *    `contactRadius + hysteresis`, so a flicker at the limit counts once. A proxy for touching a wall (it is not an ODE
 *    contact): the radius stands for the half width of the robot plus the half thickness of the wall.
 *  - max stall: the longest interval of the attempt in which the robot moved less than `stallDistance` and turned less than
 *    `stallAngle` from an anchor pose; the anchor is replaced by the current pose whenever one of the two is exceeded. Standing
 *    wall scans count as stall.
 */

#pragma once

#include "MazeFile.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include <vector>

namespace Maze
{
  /** Settings of the tracker; the defaults are those of mazeRunMetricsProvider.cfg. */
  struct RunConfig
  {
    int attempts = 2;                  /**< 1 or 2 attempts. */
    unsigned startTapAfterMs = 3000;   /**< First tap this long after the game was playing; 0 never taps and tracks nothing. */
    unsigned attemptTimeoutMs = 300000; /**< An attempt that has not reached the goal by then is over. */
    unsigned returnTimeoutMs = 150000; /**< Longest wait between the goal of attempt 1 and the robot standing at the start. */
    unsigned settleMs = 3000;          /**< The robot has to stand still this long in the start cell before the second tap. */
    float contactRadius = 150.f;       /**< Distance in mm from the robot centre to a wall centre line below which it is in contact. */
    float hysteresis = 20.f;           /**< The robot is out of contact only above contactRadius + hysteresis (mm). */
    float stallDistance = 50.f;        /**< Less translation than this (mm) from the anchor pose is no progress. */
    float stallAngle = 0.26f;          /**< Less rotation than this (rad, about 15 degrees) from the anchor pose is no progress. */
  };

  /** Why an attempt ended. */
  enum class RunStatus : unsigned char
  {
    goal,       /**< A goal cell was entered. */
    timeout,    /**< `attemptTimeoutMs` passed. */
    notStarted, /**< The attempt did not run: the first one did not reach the goal or the return did not come back in time. */
  };

  /** Where the sequence is. */
  enum class RunPhase : unsigned char
  {
    waitingForStart, /**< Waiting for the game to play and for the first tap. */
    running,         /**< An attempt is running. */
    returning,       /**< Attempt 1 is over, waiting for the robot to stand at the start. */
    done,            /**< Every requested attempt has a result. */
  };

  /** The result of one attempt. */
  struct AttemptResult
  {
    int attempt = 1;
    RunStatus status = RunStatus::goal;
    bool reachedGoal = false;
    unsigned timeMs = 0;
    int wallContacts = 0;
    unsigned maxStallMs = 0;
  };

  /** One frame of input. */
  struct RunFrame
  {
    unsigned timeMs = 0;   /**< Simulated time of the frame. */
    bool playing = false;  /**< The game state is playing. */
    Pose2f pose;           /**< Ground truth robot pose in the maze frame. */
  };

  /** What the tracker asks for after a frame. */
  struct RunUpdate
  {
    bool tap = false;                     /**< Tap the head front now. */
    std::vector<AttemptResult> finished;  /**< Attempts that ended in this frame, in order (several if an attempt was not started). */
  };

  class RunTracker
  {
  public:
    RunTracker(const MazeDescription& maze, const RunConfig& config);

    /** Call once per frame with increasing times. */
    RunUpdate update(const RunFrame& frame);

    [[nodiscard]] RunPhase phase() const { return phaseValue; }

    /** The attempt that runs, or the last one that did; 0 before the first tap. */
    [[nodiscard]] int attempt() const { return current.attempt; }

    /** The metrics of the attempt that runs (so far), or of the last one. */
    [[nodiscard]] const AttemptResult& currentResult() const { return current; }

  private:
    struct Segment
    {
      Vector2f from, to;
    };

    void beginAttempt(int attempt, const RunFrame& frame);
    void updateAttempt(const RunFrame& frame, RunUpdate& update);
    void updateReturn(const RunFrame& frame, RunUpdate& update);
    void finishAttempt(RunStatus status, const RunFrame& frame, RunUpdate& update);
    void reportNotStarted(int firstAttempt, RunUpdate& update);

    /** Distance in mm from `point` to the nearest wall. */
    [[nodiscard]] float distanceToWall(const Vector2f& point) const;

    /** Whether `pose` is within the stall limits of `anchor`. */
    [[nodiscard]] bool isStill(const Pose2f& pose, const Pose2f& anchor) const;

    std::vector<Segment> walls;
    int startCell;
    RunConfig config;
    RunPhase phaseValue = RunPhase::waitingForStart;
    AttemptResult current;
    unsigned playingSince = 0;
    bool wasPlaying = false;
    unsigned attemptStart = 0;
    bool inContact = false;
    Pose2f stallAnchor;
    unsigned stallAnchorTime = 0;
    unsigned returnStart = 0;
    bool settling = false;
    Pose2f settleAnchor;
    unsigned settleStart = 0;
  };
}
