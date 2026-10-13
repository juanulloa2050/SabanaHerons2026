/**
 * @file Apps/Tests/Maze/MazeExplorationSim.h
 *
 * Test-only simulation of exploration attempt 1 with the sensing model of explore_run in
 * Util/MazeTools/solve_maze.py: only the border is known at first, the four edges of a cell are
 * learned on arrival (the start included), and the robot replans before every action with
 * unknown edges counting as open.
 */

#pragma once

#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazePlanner.h"

namespace Maze
{
  struct ExplorationResult
  {
    bool success = false;
    std::string error;
    int cells = 0;
    int turns = 0; /**< 90 and 180 degree turns together. */
    int timeMs = 0;
  };

  inline ExplorationResult simulateExploration(const MazeDescription& real, const MazeCosts& costs = MazeCosts())
  {
    constexpr int stepGuard = 10000;
    ExplorationResult result;
    MazePlanner planner(costs);
    MazeGraph known;
    MazeState state;
    state.row = cellRow(real.startCell);
    state.col = cellCol(real.startCell);
    state.heading = real.startHeading;

    const auto learn = [&](int row, int col)
    {
      for(int h = 0; h < headingCount; ++h)
        known.set(row, col, static_cast<Heading>(h), real.graph.get(row, col, static_cast<Heading>(h)));
    };
    learn(state.row, state.col);

    for(int steps = 0; !isGoal(state.row, state.col); ++steps)
    {
      if(steps >= stepGuard)
      {
        result.error = "step guard exceeded";
        return result;
      }
      planner.computeCostToGo(known, UnknownPolicy::asOpen);
      Action action = Action::forward;
      if(!planner.nextAction(state, action))
      {
        result.error = "goal unreachable";
        return result;
      }
      if(action == Action::forward && real.graph.get(state.row, state.col, state.heading) != EdgeState::open)
      {
        result.error = "forward through a closed edge";
        return result;
      }
      MazeState next;
      MazeCost step;
      if(!planner.apply(known, UnknownPolicy::asOpen, state, action, next, step))
      {
        result.error = "planned action is not applicable";
        return result;
      }
      if(action == Action::forward)
        ++result.cells;
      else
        ++result.turns;
      result.timeMs += step.timeMs;
      state = next;
      if(action == Action::forward)
        learn(state.row, state.col);
    }
    result.success = true;
    return result;
  }
}
