/**
 * @file Tools/Maze/MazeSession.h
 *
 * The stateful decisions of MazeLocalizer and MazeMapper that do not depend on representations, so that
 * they can be unit tested: when the robot is "positioned at start" (the first frame and every transition
 * into playing), when a new attempt begins, and which parts of the map update run in a frame.
 */

#pragma once

#include "MazeLocalization.h"

namespace Maze
{
  /** What `PlayingTracker::update` found in one frame. */
  struct PlayingTransition
  {
    bool firstFrame = false;     /**< The first frame this tracker saw. */
    bool enteredPlaying = false; /**< The game state is playing now and was not in the previous frame (or this is the first frame). */
    bool newAttempt = false;     /**< `enteredPlaying`, and an earlier attempt already ran. The very first one is attempt 1. */

    /** The localizer puts the pose at the start. */
    [[nodiscard]] bool resetPose() const { return firstFrame || enteredPlaying; }
  };

  /** Detects transitions into playing; call `update` once per frame. */
  class PlayingTracker
  {
  public:
    PlayingTransition update(bool playing)
    {
      PlayingTransition transition;
      transition.firstFrame = !seenFrame;
      transition.enteredPlaying = playing && !wasPlaying;
      transition.newAttempt = transition.enteredPlaying && hasPlayed;
      hasPlayed = hasPlayed || playing;
      wasPlaying = playing;
      seenFrame = true;
      return transition;
    }

  private:
    bool seenFrame = false;
    bool wasPlaying = false;
    bool hasPlayed = false; /**< An attempt has started. */
  };

  /** What the mapper does with the current frame. */
  struct MapActions
  {
    bool markVisited = false; /**< The robot's cell counts as visited. */
    bool observe = false;     /**< The wall percept and the tags are added to the map. */
  };

  /** Whether a pose of quality `actual` is good enough when the worst accepted quality is `worst` (superb is the best). */
  [[nodiscard]] constexpr bool qualityAccepted(LocalizationQuality actual, LocalizationQuality worst)
  {
    return static_cast<int>(actual) <= static_cast<int>(worst);
  }

  /**
   * The map learns only while playing (a carried robot would not match the pose) and while the pose is
   * inside the maze; a visit is recorded before the quality gate, observations are subject to it.
   */
  [[nodiscard]] constexpr MapActions mapActions(bool playing, bool insideMaze, bool qualityOk)
  {
    MapActions actions;
    actions.markVisited = playing && insideMaze;
    actions.observe = actions.markVisited && qualityOk;
    return actions;
  }
}
