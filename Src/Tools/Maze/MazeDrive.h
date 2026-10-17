/**
 * @file Tools/Maze/MazeDrive.h
 *
 * The decisions of the maze behaviour that are not a phase: what to say next (a bounded speech queue), and which action of
 * the plan to run next (drive locking, one scan per cell, time limits). Free of modules and skills so that it can be
 * unit tested; `HandleMaze` feeds it with the facts of the frame and runs the skill it chooses.
 */

#pragma once

#include "MazeGraph.h"
#include "MazePlanner.h"
#include <deque>
#include <optional>
#include <string>

namespace Maze
{
  /** How long one phrase is said before the next one starts (ms). */
  constexpr unsigned announcementTimeMs = 1500;

  /** The most phrases that wait to be said. Taps queue phrases without limit, so when it is full the oldest waiting one is dropped. */
  constexpr std::size_t maxQueuedPhrases = 8;

  /** The longest a walk to a neighbouring cell may take before the plan is read again (ms). */
  constexpr unsigned walkTimeoutMs = 20000;

  /** The longest a turn may take before the plan is read again (ms). */
  constexpr unsigned turnTimeoutMs = 12000;

  /** The longest a wall scan may take before the plan is read again (ms). */
  constexpr unsigned scanTimeoutMs = 2500;

  /**
   * The phrases that wait to be said. `update` returns the phrase to hand to `Say` in this frame. `Say` drops the same text
   * twice in a row until it was not called for a frame, so a frame without a phrase follows every finished phrase.
   */
  class SpeechQueue
  {
  public:
    /** Queues a phrase. If `maxQueuedPhrases` are waiting, the oldest one that is not being said is dropped. */
    void push(const std::string& phrase);

    /** @return The phrase to say now, if any; call it once per frame while the behaviour runs. */
    [[nodiscard]] std::optional<std::string> update(unsigned now);

    [[nodiscard]] std::size_t size() const { return phrases.size(); }
    [[nodiscard]] bool empty() const { return phrases.empty(); }

  private:
    std::deque<std::string> phrases; /**< The first one is being said once `running` is set. */
    bool running = false;            /**< The first phrase has been handed to `Say`. */
    unsigned startTime = 0;          /**< When it was handed over. */
    bool gap = false;                /**< Skip one frame so that `Say` forgets the last phrase. */
  };

  /** What the drive of an attempt is doing. */
  enum class DriveKind
  {
    idle, /**< Standing: there is no route (or no pose) to follow. */
    walk, /**< Walking to the neighbouring cell. */
    turn, /**< Turning to the next heading. */
    scan, /**< Scanning the walls of a cell with unknown edges. */
  };

  /** What to do next at a cell centre. */
  enum class DriveNext
  {
    idle,
    walk,
    turn,
    scan,
    keep, /**< The plan does not describe the current pose yet: go on with the current action. */
  };

  /** What the drive needs to know when it picks the next action. */
  struct DriveInput
  {
    bool speedRun = false;         /**< Follow the speed-run route (no scans), otherwise the exploration route. */
    bool planValid = false;        /**< `MazePlan::valid`: the pose is inside the maze. */
    bool planCurrent = false;      /**< The plan was computed for the pose the robot has now. */
    bool routeFound = false;       /**< The followed route exists. */
    bool hasAction = false;        /**< The followed route has a next action (it is not empty). */
    Action nextAction = Action::forward;
    int targetRow = 0;             /**< The state after the next action. */
    int targetCol = 0;
    Heading targetHeading = Heading::north;
    int cell = 0;                  /**< The cell of the robot. */
    bool unknownEdges = false;     /**< Any edge of that cell is unknown in the map. */
  };

  /** What `DriveController::choose` decided. */
  struct DriveDecision
  {
    DriveNext next = DriveNext::idle;
    bool reportNoRoute = false; /**< There is no route and this was not reported since the last route. */
  };

  /**
   * Reads the plan at a cell centre and locks the next action until it is over. The exploration route changes whenever
   * the map learns an edge, so only its first action is binding; a stale plan keeps the locked action.
   */
  class DriveController
  {
  public:
    /** A new phase begins: nothing is locked, the cells may be scanned again. */
    void reset();

    /** @return Whether the action that runs is over: its skill is done, or it is too slow so that the plan is read again. */
    [[nodiscard]] bool actionOver(bool skillDone, unsigned now) const;

    /** Picks the next action and locks it. */
    DriveDecision choose(const DriveInput& input, unsigned now);

    [[nodiscard]] DriveKind kind() const { return driveKind; }
    [[nodiscard]] int targetRow() const { return lockedRow; }
    [[nodiscard]] int targetCol() const { return lockedCol; }
    [[nodiscard]] Heading targetHeading() const { return lockedHeading; }

  private:
    DriveKind driveKind = DriveKind::idle;
    unsigned startTime = 0;
    int lockedRow = 0; /**< The target of the running walk, or the cell of the running turn. */
    int lockedCol = 0;
    Heading lockedHeading = Heading::north; /**< The heading to keep (walk) or to reach (turn). */
    int scannedCell = -1;                   /**< The cell where the walls were scanned last in this phase. */
    bool noRouteReported = false;
  };
}
