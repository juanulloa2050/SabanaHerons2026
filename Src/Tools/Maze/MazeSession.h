/**
 * @file Tools/Maze/MazeSession.h
 *
 * The stateful decisions of MazeLocalizer and MazeMapper that do not depend on representations, so that
 * they can be unit tested: when the robot is "active" (playing and not picked up), when it is
 * "positioned at start" (the first frame and every transition into active), when a new attempt begins,
 * and which parts of the map update run in a frame.
 */

#pragma once

#include "MazeLocalization.h"
#include "Representations/Sensing/FallDownState.h"

namespace Maze
{
  /**
   * Whether the robot takes part in an attempt: the game is playing, the robot is not penalized and it is not picked up
   * (`FallDownState::pickedUp`: upright without ground contact for a minimum time). A picked-up or penalized robot is
   * not active even if the game state still says playing (a chest-button penalty without GameController leaves the game
   * state at playing), so both end the attempt like a pause does. A fall or a flicker of the ground contact is not a
   * pick-up and does not end the attempt.
   */
  [[nodiscard]] constexpr bool isActive(bool playing, bool penalized, bool pickedUp)
  {
    return playing && !penalized && !pickedUp;
  }

  /**
   * The same with the body state of the robot: only `FallDownState::pickedUp` is a pick-up. A fall (falling, fallen,
   * staggering, squatting) is not, so a fall during an attempt neither resets the pose nor counts an attempt.
   */
  [[nodiscard]] constexpr bool isActive(bool playing, bool penalized, FallDownState::State body)
  {
    return isActive(playing, penalized, body == FallDownState::pickedUp);
  }

  /** What `PlayingTracker::update` found in one frame. */
  struct PlayingTransition
  {
    bool firstFrame = false;     /**< The first frame this tracker saw. */
    bool enteredPlaying = false; /**< The robot is active now and was not in the previous frame (or this is the first frame). */
    bool newAttempt = false;     /**< `enteredPlaying`, and an earlier attempt already ran. The very first one is attempt 1. */

    /** The localizer puts the pose at the start. */
    [[nodiscard]] bool resetPose() const { return firstFrame || enteredPlaying; }
  };

  /** Detects transitions into the active state (see `isActive`); call `update` once per frame. */
  class PlayingTracker
  {
  public:
    PlayingTransition update(bool active)
    {
      PlayingTransition transition;
      transition.firstFrame = !seenFrame;
      transition.enteredPlaying = active && !wasPlaying;
      transition.newAttempt = transition.enteredPlaying && hasPlayed;
      hasPlayed = hasPlayed || active;
      wasPlaying = active;
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
   * The map learns only while the robot is active (a carried robot would not match the pose); a visit is
   * recorded before the quality gate, observations are subject to it. Whether the pose is inside the maze is
   * decided by `StartAnchoredMap`, which also has to see the frames of a pose outside it.
   */
  [[nodiscard]] constexpr MapActions mapActions(bool active, bool qualityOk)
  {
    MapActions actions;
    actions.markVisited = active;
    actions.observe = active && qualityOk;
    return actions;
  }
}
