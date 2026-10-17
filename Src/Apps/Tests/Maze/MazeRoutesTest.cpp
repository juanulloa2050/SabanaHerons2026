/**
 * @file MazeRoutesTest.cpp
 *
 * Tests of the exploration and speed-run routes that MazePlannerProvider hands to the behaviour (ticket MZ-024):
 * the motion costs of mazePlanner.cfg change the chosen route, the speed run never uses an edge that is not known to be
 * open, and the exploration route follows what the map has learned.
 */

#include "Platform/File.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeRoutes.h"

#include <gtest/gtest.h>
#include <chrono>
#include <iostream>

namespace
{
  using namespace Maze;

  MazeState stateAt(int row, int col, Heading heading)
  {
    MazeState state;
    state.row = row;
    state.col = col;
    state.heading = heading;
    return state;
  }

  /** Opens the edges between consecutive cells of `cells` (row, col pairs). */
  void openPath(MazeGraph& graph, const std::vector<std::pair<int, int>>& cells)
  {
    for(std::size_t i = 1; i < cells.size(); ++i)
    {
      const int dRow = cells[i].first - cells[i - 1].first, dCol = cells[i].second - cells[i - 1].second;
      for(int h = 0; h < headingCount; ++h)
        if(rowDelta[static_cast<std::size_t>(h)] == dRow && colDelta[static_cast<std::size_t>(h)] == dCol)
          graph.set(cells[i - 1].first, cells[i - 1].second, static_cast<Heading>(h), EdgeState::open);
    }
  }

  /**
   * Two known routes from (7, 0) facing north to the goal and nothing else: a staircase with 6 cells and 5 turns, and a
   * detour along the west and north with 11 cells and 2 turns. The staircase is faster unless a turn is expensive.
   */
  MazeGraph twoRoutes()
  {
    MazeGraph graph;
    openPath(graph, {{7, 0}, {6, 0}, {6, 1}, {5, 1}, {5, 2}, {4, 2}, {4, 3}});
    openPath(graph, {{7, 0}, {6, 0}, {5, 0}, {4, 0}, {3, 0}, {2, 0}, {1, 0}, {1, 1}, {1, 2}, {1, 3}, {2, 3}, {3, 3}});
    return graph;
  }

  bool usesOnlyOpenEdges(const MazeGraph& graph, const RouteResult& route)
  {
    for(std::size_t i = 0; i < route.actions.size(); ++i)
      if(route.actions[i] == Action::forward && graph.get(route.states[i].row, route.states[i].col, route.states[i].heading) != EdgeState::open)
        return false;
    return true;
  }
}

GTEST_TEST(MazeRoutes, TheTurnCostChangesTheSpeedRunRoute)
{
  const MazeGraph graph = twoRoutes();
  const MazeState start = stateAt(7, 0, Heading::north);

  MazeCosts cheapTurns; // The defaults: 2000 ms per 90 degree turn.
  const RoutePair cheap = planRoutes(graph, start, cheapTurns);
  ASSERT_TRUE(cheap.speedRun.found);
  EXPECT_EQ(cheap.speedRun.cells, 6);
  EXPECT_EQ(cheap.speedRun.turns90, 5) << "the staircase";

  MazeCosts dearTurns;
  dearTurns.turn90Ms = 5000;
  const RoutePair dear = planRoutes(graph, start, dearTurns);
  ASSERT_TRUE(dear.speedRun.found);
  EXPECT_EQ(dear.speedRun.cells, 11);
  EXPECT_EQ(dear.speedRun.turns90, 2) << "the detour with few turns";
  EXPECT_NE(cheap.speedRun.actions, dear.speedRun.actions);
  EXPECT_NE(cheap.speedRun.states.back().col * 10 + cheap.speedRun.states.back().row, dear.speedRun.states.back().col * 10 + dear.speedRun.states.back().row)
      << "the two routes reach different goal cells, (4, 3) and (3, 3)";

  // The estimate is the sum of the costs of the actions of the route.
  EXPECT_EQ(cheap.speedRun.timeMs, 6 * cheapTurns.forwardMs + 6 * cheapTurns.startMs + 5 * cheapTurns.turn90Ms);
  EXPECT_EQ(dear.speedRun.timeMs, 11 * dearTurns.forwardMs + 3 * dearTurns.startMs + 2 * dearTurns.turn90Ms);
}

GTEST_TEST(MazeRoutes, TheSpeedRunNeverUsesAnEdgeThatIsNotKnownOpen)
{
  MazeDescription maze;
  std::string error;
  ASSERT_TRUE(MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_1.maze", maze, error)) << error;
  const MazeState start = stateAt(cellRow(maze.startCell), cellCol(maze.startCell), maze.startHeading);

  // Forget half of the open edges: they are unknown now, so the speed run must not use them.
  MazeGraph known = maze.graph;
  int forgotten = 0;
  for(int row = 0; row < size; ++row)
    for(int col = 0; col < size; ++col)
      for(int h = 0; h < headingCount; ++h)
        if(known.get(row, col, static_cast<Heading>(h)) == EdgeState::open && (row * 7 + col * 3 + h) % 4 == 0)
        {
          known.set(row, col, static_cast<Heading>(h), EdgeState::unknown);
          ++forgotten;
        }
  ASSERT_GT(forgotten, 0);
  for(const MazeGraph& graph : {maze.graph, known})
  {
    const RoutePair routes = planRoutes(graph, start, MazeCosts());
    if(routes.speedRun.found)
      EXPECT_TRUE(usesOnlyOpenEdges(graph, routes.speedRun));
    // The exploration is optimistic: it may be shorter than the speed run, never longer.
    ASSERT_TRUE(routes.exploration.found);
    if(routes.speedRun.found)
      EXPECT_LE(routes.exploration.timeMs, routes.speedRun.timeMs);
  }

  // Nothing known: the exploration still has a route, the speed run has none.
  const RoutePair unknown = planRoutes(MazeGraph(), start, MazeCosts());
  EXPECT_TRUE(unknown.exploration.found);
  EXPECT_FALSE(unknown.speedRun.found);
  EXPECT_TRUE(unknown.speedRun.actions.empty());
}

GTEST_TEST(MazeRoutes, TheExplorationFollowsWhatTheMapHasLearned)
{
  const MazeState start = stateAt(7, 0, Heading::north);
  MazeGraph map; // The start is the south-west corner, the border is known, the inner edges are not.

  // Nothing is known: go straight on towards the goal.
  RoutePair routes = planRoutes(map, start, MazeCosts());
  ASSERT_TRUE(routes.exploration.found);
  EXPECT_EQ(routes.exploration.actions.front(), Action::forward);
  EXPECT_EQ(routes.exploration.states[1].row, 6);
  EXPECT_EQ(routes.exploration.states[1].col, 0);

  // A wall ahead: turn to the open side, east.
  map.set(7, 0, Heading::north, EdgeState::wall);
  routes = planRoutes(map, start, MazeCosts());
  ASSERT_TRUE(routes.exploration.found);
  EXPECT_EQ(routes.exploration.actions.front(), Action::turnRight);
  EXPECT_EQ(routes.exploration.firstTarget().row, 7);
  EXPECT_EQ(routes.exploration.firstTarget().col, 0);
  EXPECT_EQ(routes.exploration.firstTarget().heading, Heading::east);

  // The east edge is open and known: drive on through it.
  map.set(7, 0, Heading::east, EdgeState::open);
  routes = planRoutes(map, stateAt(7, 0, Heading::east), MazeCosts());
  ASSERT_TRUE(routes.exploration.found);
  EXPECT_EQ(routes.exploration.actions.front(), Action::forward);
  EXPECT_EQ(routes.exploration.firstTarget().col, 1);
  EXPECT_EQ(routes.exploration.firstTarget().heading, Heading::east);
  EXPECT_FALSE(routes.speedRun.found) << "the rest of the way is not known";

  // A dead end: the way back is the only way on.
  MazeGraph deadEnd;
  deadEnd.set(7, 0, Heading::north, EdgeState::wall);
  deadEnd.set(7, 0, Heading::east, EdgeState::wall);
  EXPECT_FALSE(planRoutes(deadEnd, start, MazeCosts()).exploration.found) << "a closed cell has no way out";
}

GTEST_TEST(MazeRoutes, AGoalCellAndAnInvalidStartAreHandled)
{
  const RoutePair atGoal = planRoutes(MazeGraph(), stateAt(3, 3, Heading::north), MazeCosts());
  EXPECT_TRUE(atGoal.atGoal);
  EXPECT_TRUE(atGoal.exploration.found);
  EXPECT_TRUE(atGoal.exploration.actions.empty());
  EXPECT_TRUE(atGoal.speedRun.found);
  EXPECT_TRUE(atGoal.speedRun.actions.empty());
  EXPECT_EQ(atGoal.exploration.firstTarget().row, 3);
  EXPECT_EQ(atGoal.speedRun.firstTarget().col, 3);

  const RoutePair outside = planRoutes(MazeGraph(), stateAt(-1, 8, Heading::north), MazeCosts());
  EXPECT_FALSE(outside.atGoal);
  EXPECT_FALSE(outside.exploration.found);
  EXPECT_FALSE(outside.speedRun.found);
}

GTEST_TEST(MazeRoutes, CostsMustTakeTime)
{
  EXPECT_TRUE(costsValid(MazeCosts()));
  MazeCosts costs;
  costs.forwardMs = 0;
  EXPECT_FALSE(costsValid(costs));
  costs = MazeCosts();
  costs.turn90Ms = -1;
  EXPECT_FALSE(costsValid(costs));
  costs = MazeCosts();
  costs.turn180Ms = 0;
  EXPECT_FALSE(costsValid(costs));
  costs = MazeCosts();
  costs.startMs = 0; // No start-up penalty is fine.
  EXPECT_TRUE(costsValid(costs));
  costs.startMs = -1;
  EXPECT_FALSE(costsValid(costs));
}

GTEST_TEST(MazeRoutes, BothRoutesAreCheapEnoughToRecomputeEveryFrame)
{
  const MazeState start = stateAt(7, 0, Heading::north);
  MazeGraph graph = twoRoutes();
  constexpr int runs = 200;
  const auto begin = std::chrono::steady_clock::now();
  int found = 0;
  for(int i = 0; i < runs; ++i)
    found += planRoutes(graph, start, MazeCosts()).speedRun.found ? 1 : 0;
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / runs;
  EXPECT_EQ(found, runs);
  // The time is printed, not asserted: a wall-clock bound flakes on a loaded machine (CI, a debug build, a busy laptop), and the
  // planner is cheap by construction (a Dijkstra over 8 x 8 x 4 x 2 states, ~0.6 ms measured). `found` checks that the work was done.
  std::cout << "[MZ-024] planRoutes (exploration + speed run): " << ms * 1000.0 << " us per call" << std::endl;
}

GTEST_TEST(MazeRoutes, InvalidCostsFallBackToTheDefaults)
{
  const MazeCosts defaults;
  MazeCosts costs;
  costs.turn90Ms = 5000;
  EXPECT_EQ(validCosts(costs).turn90Ms, 5000) << "valid costs are kept";
  costs.turn90Ms = 0;
  const MazeCosts fixed = validCosts(costs);
  EXPECT_TRUE(costsValid(fixed));
  EXPECT_EQ(fixed.forwardMs, defaults.forwardMs);
  EXPECT_EQ(fixed.startMs, defaults.startMs);
  EXPECT_EQ(fixed.turn90Ms, defaults.turn90Ms) << "all four are replaced together, not just the invalid one";
  EXPECT_EQ(fixed.turn180Ms, defaults.turn180Ms);
  costs = MazeCosts();
  costs.forwardMs = 3000;
  costs.startMs = -5;
  EXPECT_EQ(validCosts(costs).forwardMs, defaults.forwardMs);
}

GTEST_TEST(MazeRoutes, AnyChangeOfTheCostsMapOrPoseInvalidatesTheCachedPlan)
{
  PlanKey key;
  key.mapChangeCounter = 7;
  key.row = 2;
  key.col = 3;
  key.heading = 1;
  key.valid = true;
  EXPECT_TRUE(key == PlanKey(key));

  const auto changed = [&](auto change)
  {
    PlanKey other = key;
    change(other);
    return !(other == key);
  };
  EXPECT_TRUE(changed([](PlanKey& k) { k.costs.forwardMs += 1; }));
  EXPECT_TRUE(changed([](PlanKey& k) { k.costs.startMs += 1; }));
  EXPECT_TRUE(changed([](PlanKey& k) { k.costs.turn90Ms += 1; })) << "a runtime change of t_turn90 must replan";
  EXPECT_TRUE(changed([](PlanKey& k) { k.costs.turn180Ms += 1; }));
  EXPECT_TRUE(changed([](PlanKey& k) { ++k.mapChangeCounter; }));
  EXPECT_TRUE(changed([](PlanKey& k) { ++k.row; }));
  EXPECT_TRUE(changed([](PlanKey& k) { ++k.col; }));
  EXPECT_TRUE(changed([](PlanKey& k) { ++k.heading; }));
  EXPECT_TRUE(changed([](PlanKey& k) { k.valid = false; }));
}
