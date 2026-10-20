/**
 * @file Tools/Maze/MazeRunTracker.cpp
 *
 * Implementation of the measurement of a simulated maze run, see MazeRunTracker.h.
 */

#include "MazeRunTracker.h"
#include "MazeOracle.h"
#include "Math/Angle.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Maze
{
  namespace
  {
    /** Distance from `point` to the segment between `from` and `to`. */
    float distanceToSegment(const Vector2f& point, const Vector2f& from, const Vector2f& to)
    {
      const Vector2f along = to - from;
      const float lengthSquared = along.squaredNorm();
      const float t = lengthSquared > 0.f ? std::clamp((point - from).dot(along) / lengthSquared, 0.f, 1.f) : 0.f;
      return (point - (from + along * t)).norm();
    }
  }

  RunTracker::RunTracker(const MazeDescription& maze, const RunConfig& config) :
    startCell(maze.startCell),
    config(config)
  {
    // Every wall slot once: the north edge of every cell and the south edge of the last row, the west edge of every cell and
    // the east edge of the last column.
    const auto add = [&](int row, int col, Heading heading)
    {
      if(maze.graph.get(row, col, heading) != EdgeState::wall)
        return;
      const Vector2f middle = Oracle::cellCentreOf(row, col) + Oracle::directionOf(heading) * Oracle::halfCellMm;
      const Vector2f along(-Oracle::directionOf(heading).y(), Oracle::directionOf(heading).x());
      walls.push_back({middle - along * Oracle::halfCellMm, middle + along * Oracle::halfCellMm});
    };
    for(int row = 0; row < size; ++row)
      for(int col = 0; col < size; ++col)
      {
        add(row, col, Heading::north);
        add(row, col, Heading::west);
        if(row == size - 1)
          add(row, col, Heading::south);
        if(col == size - 1)
          add(row, col, Heading::east);
      }
  }

  float RunTracker::distanceToWall(const Vector2f& point) const
  {
    float best = std::numeric_limits<float>::max();
    for(const Segment& wall : walls)
      best = std::min(best, distanceToSegment(point, wall.from, wall.to));
    return best;
  }

  bool RunTracker::isStill(const Pose2f& pose, const Pose2f& anchor) const
  {
    return (pose.translation - anchor.translation).norm() < config.stallDistance &&
           std::abs(Angle::normalize(pose.rotation - anchor.rotation)) < config.stallAngle;
  }

  void RunTracker::beginAttempt(int attempt, const RunFrame& frame)
  {
    current = AttemptResult();
    current.attempt = attempt;
    phaseValue = RunPhase::running;
    attemptStart = frame.timeMs;
    // Touching a wall at the moment of the tap is not an entry made during the attempt.
    inContact = distanceToWall(frame.pose.translation) < config.contactRadius;
    stallAnchor = frame.pose;
    stallAnchorTime = frame.timeMs;
  }

  void RunTracker::finishAttempt(RunStatus status, const RunFrame& frame, RunUpdate& update)
  {
    current.status = status;
    current.reachedGoal = status == RunStatus::goal;
    current.timeMs = frame.timeMs - attemptStart;
    update.finished.push_back(current);
    if(current.attempt >= config.attempts)
      phaseValue = RunPhase::done;
    else if(current.reachedGoal)
    {
      phaseValue = RunPhase::returning;
      returnStart = frame.timeMs;
      settling = false;
    }
    else
      reportNotStarted(current.attempt + 1, update);
  }

  void RunTracker::reportNotStarted(int firstAttempt, RunUpdate& update)
  {
    for(int attempt = firstAttempt; attempt <= config.attempts; ++attempt)
    {
      AttemptResult result;
      result.attempt = attempt;
      result.status = RunStatus::notStarted;
      update.finished.push_back(result);
      current = result;
    }
    phaseValue = RunPhase::done;
  }

  void RunTracker::updateAttempt(const RunFrame& frame, RunUpdate& update)
  {
    // Contact, with hysteresis.
    const float distance = distanceToWall(frame.pose.translation);
    if(!inContact && distance < config.contactRadius)
    {
      inContact = true;
      ++current.wallContacts;
    }
    else if(inContact && distance > config.contactRadius + config.hysteresis)
      inContact = false;

    // Stall: the time since the anchor, which moves along whenever the robot made progress.
    if(isStill(frame.pose, stallAnchor))
      current.maxStallMs = std::max(current.maxStallMs, frame.timeMs - stallAnchorTime);
    else
    {
      stallAnchor = frame.pose;
      stallAnchorTime = frame.timeMs;
    }

    current.timeMs = frame.timeMs - attemptStart;
    int row, col;
    if(Oracle::cellContaining(frame.pose.translation, row, col) && isGoal(row, col))
      finishAttempt(RunStatus::goal, frame, update);
    else if(current.timeMs >= config.attemptTimeoutMs)
      finishAttempt(RunStatus::timeout, frame, update);
  }

  void RunTracker::updateReturn(const RunFrame& frame, RunUpdate& update)
  {
    if(frame.timeMs - returnStart >= config.returnTimeoutMs)
    {
      reportNotStarted(current.attempt + 1, update);
      return;
    }
    int row, col;
    if(!Oracle::cellContaining(frame.pose.translation, row, col) || cellId(row, col) != startCell)
    {
      settling = false;
      return;
    }
    if(!settling || !isStill(frame.pose, settleAnchor))
    {
      settling = true;
      settleAnchor = frame.pose;
      settleStart = frame.timeMs;
    }
    if(frame.timeMs - settleStart >= config.settleMs)
    {
      update.tap = true;
      beginAttempt(current.attempt + 1, frame);
    }
  }

  RunUpdate RunTracker::update(const RunFrame& frame)
  {
    RunUpdate update;
    if(config.startTapAfterMs == 0 || config.attempts < 1)
      return update;
    switch(phaseValue)
    {
      case RunPhase::waitingForStart:
        if(frame.playing && !wasPlaying)
          playingSince = frame.timeMs;
        if(frame.playing && frame.timeMs - playingSince >= config.startTapAfterMs)
        {
          update.tap = true;
          beginAttempt(1, frame);
        }
        break;
      case RunPhase::running:
        updateAttempt(frame, update);
        break;
      case RunPhase::returning:
        updateReturn(frame, update);
        break;
      case RunPhase::done:
        break;
    }
    wasPlaying = frame.playing;
    return update;
  }
}
