/**
 * @file Tools/Maze/MazeBehavior.h
 *
 * The phases of the maze behaviour (roadmap 4.7, restricted to MZ-024) and what the robot says when it changes them,
 * free of modules and skills so that it can be unit tested. `HandleMaze` feeds `MazeBehavior::update` with the
 * facts of the frame and runs the CABSL state that belongs to the phase it returns.
 *
 *   notPlaying      -> waitingForStart   the game is playing and the robot is not penalized  "ready"
 *   waitingForStart -> exploration       headFront tap, no known route to the goal       "exploring"
 *   waitingForStart -> speedRun          headFront tap, a route over known-open edges     "speed run"
 *   waitingForStart -> notPlaying        the game stops playing, or the robot is penalized
 *   exploration     -> finished          the pose is in a goal cell                       "goal", "finished"
 *   speedRun        -> finished          the pose is in a goal cell                       "goal", "finished"
 *   exploration/speedRun -> finished     with returnToStart.enabled = false: "goal", "finished"
 *   exploration/speedRun -> finished     with returnToStart.enabled = true: "goal" only (the return follows)
 *   finished        -> returnToStart     trigger = timer: delayMs after the goal; trigger = button: headFront tap  "returning"
 *   returnToStart   -> standby           the pose is in the start cell, or maxDurationMs have passed (also if no route was found)  "standby"
 *   exploration/speedRun/finished/returnToStart -> standby   the robot is not active (picked up, penalized or not playing)   "standby"
 *   exploration/speedRun/finished/returnToStart -> standby   the attempt counter grew while the robot is active (the frames in which
 *                                                the robot was not active were not seen: the option did not run)    "ready"
 *   standby         -> standby           the robot is lifted again before the tap                       "standby"
 *   standby         -> standby           the robot becomes active again (put down, unpenalized, gc playing)  "ready"
 *   standby         -> speedRun / exploration   headFront tap while active, as for waitingForStart  "speed run" / "exploring"
 *
 * HandleMaze is not executed in every frame: CABSL stops at the first root option that acts, and HandlePlayerState acts
 * while the robot is penalized. The phase machine then never sees the inactive frames, so the attempt counter of the map
 * (it grows when the robot becomes active again) tells it that an attempt ended in the meantime.
 *
 * A penalty counts as a pick-up, also the chest-button one of a robot without GameController (the game state stays
 * playing then). So a robot that is put down says "standby", then "ready" once, then "exploring" / "speed run" on the tap.
 *
 * "start <id>" is said once when the start cell becomes known by deduction (never when it is given). The phrases are
 * data, so a test can check every transition; speaking them (without blocking the motion) is the behaviour's job.
 * The return to the start (MZ-024.1) is optional (`returnToStart` in handleMaze.cfg); the clock is `BehaviorInput::timeMs`, so
 * the delay and the time limit are testable. The return does not touch the attempt counter. From standby at the start cell
 * the tap starts the speed run as after any attempt.
 * `Unpositioned`/`StartScan` (MZ-041) and `Sitting` (MZ-042) are not phases here.
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include <string>
#include <vector>

namespace Maze
{
  /** What the robot is doing at the level of an attempt. */
  enum class Phase
  {
    notPlaying,      /**< The game is not playing and no attempt is running: stand. */
    waitingForStart, /**< Playing, waiting for the headFront tap. */
    exploration,     /**< Attempt 1: walk the exploration route until a goal cell is reached. */
    standby,         /**< Picked up, penalized (or the game stopped) after an attempt began: the map is kept, wait for the tap. */
    speedRun,        /**< Walk the route over known-open edges. */
    finished,        /**< A goal cell was reached: stand (and, if configured, wait for the trigger of the return). */
    returnToStart,   /**< Walk back to the start cell, exploring unknown edges (MZ-024.1). */
  };

  /** @return The name of the phase for logs. */
  [[nodiscard]] const char* phaseName(Phase phase);

  /** The phrases of the *State announcements* table that belong to the phases. */
  namespace Phrases
  {
    constexpr const char* ready = "ready";
    constexpr const char* exploring = "exploring";
    constexpr const char* speedRun = "speed run";
    constexpr const char* goal = "goal";
    constexpr const char* standby = "standby";
    constexpr const char* finished = "finished";
    constexpr const char* returning = "returning";
  }

  /** The `returnToStart` entry of `handleMaze.cfg` (MZ-024.1). */
  STREAMABLE(ReturnToStartConfig,
  {
    ENUM(Trigger,
    {,
      timer,  /**< `delayMs` after the goal is confirmed. */
      button, /**< A single headFront tap while standing at the goal. */
    }),

    (bool)(false) enabled,           /**< False: stand at the goal ("finished") as without the return. */
    (Trigger)(timer) trigger,
    (unsigned)(5000) delayMs,        /**< Wait after the goal for `trigger = timer`. */
    (unsigned)(120000) maxDurationMs, /**< Time limit of the return, counted from "returning". */
  });

  /** What `MazeBehavior::update` needs to know about one frame. */
  struct BehaviorInput
  {
    bool announce = true;          /**< `announce.enabled`: false keeps the phases but gives no phrases. */
    bool playing = false;          /**< The game state is playing (a penalized robot is handled by `penalized`). */
    bool penalized = false;        /**< The robot is penalized (`GameState::isPenalized`): it is neither playing nor active. */
    bool active = false;           /**< Playing, not penalized and not picked up (`Maze::isActive`). */
    bool headFront = false;        /**< A single head-front tap happened in this frame. */
    bool atGoal = false;           /**< The plan is valid and the pose is in a goal cell. */
    bool speedRunFound = false;    /**< The plan is valid and a route over known-open edges reaches the goal. */
    int attempt = 1;               /**< `MazeMap::attempt`: grows when the robot becomes active again; survives frames the option did not run. */
    bool startKnown = false;       /**< `MazeStart::known`. */
    bool startGiven = false;       /**< `MazeStart::given`. */
    int startCell = 0;             /**< `MazeStart::cell`. */
    unsigned timeMs = 0;           /**< `FrameInfo::time`; the clock of the delay and the time limit of the return. */
    bool atStart = false;          /**< The plan is valid and the pose is in the start cell (`MazePlan::atStart`). */
    ReturnToStartConfig returnToStart; /**< The return settings of `handleMaze.cfg`. */
  };

  /** What `MazeBehavior::update` decided. */
  struct BehaviorStep
  {
    Phase phase = Phase::notPlaying;
    bool changed = false;                 /**< The phase is not the one of the previous call. */
    std::vector<std::string> phrases;     /**< What to say, in this order; empty if announcements are off. */
  };

  /** The phase machine; call `update` once per frame. */
  class MazeBehavior
  {
  public:
    BehaviorStep update(const BehaviorInput& input);

    [[nodiscard]] Phase phase() const { return currentPhase; }

  private:
    Phase currentPhase = Phase::notPlaying;
    bool seenFrame = false;
    bool startWasKnown = false;
    bool wasActive = false; /**< The robot was active in the previous frame. */
    int lastAttempt = 1;    /**< The attempt counter of the previous call. */
    unsigned phaseStartMs = 0; /**< `timeMs` when the goal was reached (finished) or the return began (returnToStart). */
  };

  /** @return "tag <id>" if a tag was seen recently, otherwise "no tag". */
  [[nodiscard]] std::string tagPhrase(int tagId, bool seenRecently);

  /** The `announce` setting of `handleMaze.cfg`. */
  STREAMABLE(AnnounceConfig,
  {,
    (bool)(true) enabled, /**< Whether the robot says what it is doing out loud. */
  });

  /** The layout of `handleMaze.cfg` as far as the announcements and the return are concerned (the other entries are ignored). */
  STREAMABLE(AnnounceFile,
  {
    /**
     * Reads the announce and return settings from a config file; other entries of the file are not read.
     * @return Whether the file was found and read.
     */
    static bool load(const std::string& name, AnnounceFile& file),

    (AnnounceConfig) announce,
    (ReturnToStartConfig) returnToStart,
  });
}
