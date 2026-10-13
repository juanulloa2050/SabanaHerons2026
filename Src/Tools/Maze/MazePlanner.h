/**
 * @file Tools/Maze/MazePlanner.h
 *
 * Time-weighted planner for the 8x8 maze. It mirrors Util/MazeTools/solve_maze.py exactly:
 * the state is (row, col, heading, moving), costs are integer milliseconds and the cost-to-go is
 * a (time, turns) pair compared lexicographically. Two users:
 *  - exploration (attempt 1): cost-to-go table with unknown edges as open, replanned on every step.
 *  - speed run (attempt 2): A* over the edges that are known to be open.
 * Pure standard C++ so that it can be used by any module and by the unit tests.
 */

#pragma once

#include "MazeGraph.h"
#include <array>
#include <vector>

namespace Maze
{
  /**
   * Durations of the primitive motions in milliseconds. These are priors; they are replaced by
   * measured values of Lab 2 in MZ-028.
   */
  struct MazeCosts
  {
    int forwardMs = 2500; /**< One cell forward while already walking. */
    int startMs = 800; /**< Extra time for the first step after standing (after a turn or at the start). */
    int turn90Ms = 2000;
    int turn180Ms = 3500;
  };

  enum class Action : unsigned char
  {
    forward,
    turnLeft,
    turnRight,
    turnAround,
  };

  constexpr int actionCount = 4;

  /** The planning state; goal cells are goal states for every heading and moving flag. */
  struct MazeState
  {
    int row = 0;
    int col = 0;
    Heading heading = Heading::north;
    bool moving = false; /**< Whether the last action was a step forward. */
  };

  constexpr int stateCount = cellCount * headingCount * 2;

  /** (time in ms, number of turns), compared lexicographically. */
  struct MazeCost
  {
    int timeMs = 0;
    int turns = 0;

    [[nodiscard]] static constexpr MazeCost infinity()
    {
      return {1 << 28, 1 << 28};
    }

    [[nodiscard]] constexpr bool isInfinite() const
    {
      return timeMs >= infinity().timeMs;
    }

    [[nodiscard]] constexpr bool operator<(const MazeCost& other) const
    {
      return timeMs < other.timeMs || (timeMs == other.timeMs && turns < other.turns);
    }

    [[nodiscard]] constexpr bool operator==(const MazeCost& other) const
    {
      return timeMs == other.timeMs && turns == other.turns;
    }

    [[nodiscard]] constexpr MazeCost operator+(const MazeCost& other) const
    {
      return {timeMs + other.timeMs, turns + other.turns};
    }
  };

  /** A sequence of actions from a start state to a goal cell. */
  struct MazePlan
  {
    bool found = false; /**< False if the goal cannot be reached. */
    std::vector<Action> actions;
    int cells = 0; /**< Number of forward steps. */
    int turns90 = 0;
    int turns180 = 0;
    int timeMs = 0;
  };

  class MazePlanner
  {
  public:
    explicit MazePlanner(const MazeCosts& costs = MazeCosts()) : costs(costs) {}

    /** Index of `state` in 0..stateCount-1. */
    [[nodiscard]] static int stateIndex(const MazeState& state);

    /**
     * Backward Dijkstra from all goal states. Keeps a copy of `graph` and `policy` for
     * `nextAction` and `routeFromTable`.
     */
    void computeCostToGo(const MazeGraph& graph, UnknownPolicy policy);

    /** Cost to reach a goal cell from `state` according to the last table; infinite if unreachable. */
    [[nodiscard]] MazeCost costToGo(const MazeState& state) const;

    /**
     * First strict minimum of action + cost-to-go(next) in the order forward, left, right, around.
     * @return False in a goal state or if the goal is unreachable.
     */
    [[nodiscard]] bool nextAction(const MazeState& state, Action& action) const;

    /** Repeats `nextAction` from `start` until a goal cell is entered; the optimal route if the maze is known. */
    [[nodiscard]] MazePlan routeFromTable(const MazeState& start) const;

    /** A* over edges that are known to be open (unknown counts as wall). */
    [[nodiscard]] MazePlan speedRun(const MazeGraph& graph, const MazeState& start) const;

    /**
     * Admissible lower bound of the time to a goal cell: the smallest over the four goal cells of
     * manhattan * forwardMs, plus one 90 degree turn if both coordinates differ.
     */
    [[nodiscard]] int heuristic(const MazeState& state) const;

    /** Result of `action` in `state`; false if it is a forward step through a blocked edge. */
    [[nodiscard]] bool apply(const MazeGraph& graph, UnknownPolicy policy, const MazeState& state, Action action, MazeState& next, MazeCost& cost) const;

    [[nodiscard]] const MazeCosts& getCosts() const { return costs; }

  private:
    MazeCosts costs;
    MazeGraph graph;
    UnknownPolicy policy = UnknownPolicy::asOpen;
    std::array<MazeCost, stateCount> table;

    /** Adds `action` to `plan` and updates the totals. */
    void append(MazePlan& plan, Action action, int timeMs) const;
  };
}
