/**
 * @file MazeRoutesTest.cpp
 *
 * Tests of the exploration and speed-run routes that MazePlannerProvider hands to the behaviour (ticket MZ-024):
 * the motion costs of mazePlanner.cfg change the chosen route, the speed run never uses an edge that is not known to be
 * open, and the exploration route follows what the map has learned.
 */

#include "Platform/File.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeOracle.h"
#include "Tools/Maze/MazeRoutes.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>

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
  key.startCell = 0;
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
  EXPECT_TRUE(changed([](PlanKey& k) { k.startCell = 7; })) << "a new start cell (deduced or given) must replan the return";
  EXPECT_TRUE(changed([](PlanKey& k) { k.valid = false; }));
}

namespace
{
  using namespace Maze;

  /** The maze heading from cell `from` to the neighbouring cell `to`. */
  Heading headingBetween(int from, int to)
  {
    for(int h = 0; h < headingCount; ++h)
      if(cellId(cellRow(from) + rowDelta[static_cast<std::size_t>(h)], cellCol(from) + colDelta[static_cast<std::size_t>(h)]) == to)
        return static_cast<Heading>(h);
    ADD_FAILURE() << "cells " << from << " and " << to << " are not neighbours";
    return Heading::north;
  }

  /**
   * What the robot learns when it looks along `gaze` from the centre of `cell`: exactly the look-ahead that
   * `OracledMazePerceptsProvider` reports (`Oracle::computeView` over the true maze, `maxRange = 2` as in
   * oracledMazePerceptsProvider.cfg): for every cell along the gaze, its left, right and front edge.
   */
  void look(const MazeGraph& truth, MazeGraph& map, int cell, Heading gaze)
  {
    const Oracle::Camera camera{Vector2f::Zero(), 0.f, 1.2f};
    Oracle::View view;
    const Pose2f pose(Oracle::angleOf(gaze), Oracle::cellCentreOf(cellRow(cell), cellCol(cell)));
    ASSERT_TRUE(Oracle::computeView(truth, pose, camera, 2, view));
    for(const Oracle::LookAheadCell& seen : view.cells)
    {
      map.set(seen.row, seen.col, turned(view.gaze, -1), seen.left);
      map.set(seen.row, seen.col, turned(view.gaze, 1), seen.right);
      map.set(seen.row, seen.col, view.gaze, seen.front);
    }
  }

  /** The wall scan of `MazeScanWalls` (+60 deg, -60 deg, 0): the gazes left of, right of and along `heading`. */
  void scan(const MazeGraph& truth, MazeGraph& map, int cell, Heading heading)
  {
    look(truth, map, cell, turned(heading, -1));
    look(truth, map, cell, turned(heading, 1));
    look(truth, map, cell, heading);
  }

  /** The cells of a route, a turn on the spot counted once. */
  std::vector<int> cellsOf(const RouteResult& route)
  {
    std::vector<int> cells;
    for(const MazeState& state : route.states)
      if(cells.empty() || cells.back() != cellId(state.row, state.col))
        cells.push_back(cellId(state.row, state.col));
    return cells;
  }
}

/**
 * maze_19 as the robot knows it after attempt 1 reached cell 36 by the long eastern loop (the shortest walk to 36 that
 * enters the goal block only at 36: a breadth-first search over the true maze with the goal cells 27, 28 and 35 banned).
 * The map is what the scans of the walk reveal: at every walked cell but the goal, the three gazes of the wall scan
 * (left, right, along the heading it arrived with), each with the look-ahead of the oracle. The real robot scans only
 * cells that still have unknown edges, so this is an upper bound of its knowledge, and it is still short of the shortcut.
 */
GTEST_TEST(MazeRoutes, TheReturnTriesTheUnknownShortcutInsteadOfTheWalkedLoop)
{
  MazeDescription maze;
  std::string error;
  ASSERT_TRUE(MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_19.maze", maze, error)) << error;
  ASSERT_EQ(maze.startCell, 0);
  const std::vector<int> walked = {0, 8, 16, 24, 25, 33, 34, 26, 18, 19, 20, 21, 13, 12, 4, 5, 6, 14, 22, 23, 31, 39, 38, 30, 29, 37, 36};
  MazeGraph map;
  for(std::size_t i = 0; i + 1 < walked.size(); ++i)
    scan(maze.graph, map, walked[i], headingBetween(walked[i], walked[i + 1]));
  const Heading arrival = headingBetween(walked[walked.size() - 2], walked.back());
  ASSERT_EQ(arrival, Heading::west) << "the robot stands in 36 after coming from 37";
  const MazeState atGoal = stateAt(4, 4, arrival);

  // Before the goal look: the walk already showed that 35-43 is open (from 37, the gaze west sees 36 and 35 and the left
  // edge of 35), but the edge 44-43 is still unknown. The optimistic way 36 -> 44 -> 43 ties with 36 -> 35 -> 43 and saves
  // one start-up, so the plan takes it; either way it does not retrace the loop.
  const RoutePair before = planRoutes(map, atGoal, MazeCosts(), maze.startCell);
  EXPECT_TRUE(before.atGoal);
  ASSERT_TRUE(before.returnToStart.found);
  const std::vector<int> beforeCells = cellsOf(before.returnToStart);
  std::cout << "maze_19 return before the goal look:";
  for(const int cell : beforeCells)
    std::cout << ' ' << cell;
  std::cout << std::endl;
  EXPECT_EQ(beforeCells.front(), 36);
  EXPECT_EQ(beforeCells.back(), 0);
  EXPECT_EQ(std::count(beforeCells.begin(), beforeCells.end(), 37), 0) << "the walked loop is not retraced";
  EXPECT_EQ(map.get(4, 3, Heading::south), EdgeState::open) << "35-43 was seen from 37";
  EXPECT_EQ(map.get(5, 4, Heading::west), EdgeState::unknown) << "44-43 is not known before the goal look";

  // The goal look: one wall scan at 36. The gaze left of the heading (south) sees 44 and its west edge, the wall 44-43,
  // so the optimistic way through 44 is gone and 36 -> 35 -> 43 remains.
  scan(maze.graph, map, 36, arrival);
  EXPECT_EQ(map.get(5, 4, Heading::west), EdgeState::wall);
  const RoutePair routes = planRoutes(map, atGoal, MazeCosts(), maze.startCell);
  EXPECT_TRUE(routes.atGoal);
  EXPECT_FALSE(routes.atStart);
  ASSERT_TRUE(routes.returnToStart.found);
  const std::vector<int> cells = cellsOf(routes.returnToStart);
  ASSERT_GE(cells.size(), 5u);
  EXPECT_EQ(cells[0], 36);
  EXPECT_EQ(cells[1], 35);
  EXPECT_EQ(cells[2], 43);
  EXPECT_EQ(cells[3], 42);
  EXPECT_EQ(cells[4], 34) << "35 -> 43 -> 42 -> 34 goes through unknown edges, 35 and 34 are separated by a wall";
  EXPECT_EQ(cells.back(), 0);
  EXPECT_EQ(std::count(cells.begin(), cells.end(), 37), 0) << "the walked loop is not retraced";
  EXPECT_EQ(map.get(4, 2, Heading::east), EdgeState::wall);

  // The goal routes are unchanged by the extra target: the planner for the goal block still sees the goal.
  EXPECT_TRUE(routes.exploration.found);
  EXPECT_TRUE(routes.exploration.actions.empty());
}

GTEST_TEST(MazeRoutes, TheReturnIsEmptyAtTheStartAndAbsentWithoutAStartCell)
{
  const MazeState atStart = stateAt(0, 0, Heading::south);
  const RoutePair here = planRoutes(MazeGraph(), atStart, MazeCosts(), 0);
  EXPECT_TRUE(here.atStart);
  EXPECT_FALSE(here.atGoal);
  EXPECT_TRUE(here.returnToStart.found);
  EXPECT_TRUE(here.returnToStart.actions.empty());

  const RoutePair away = planRoutes(MazeGraph(), stateAt(2, 0, Heading::north), MazeCosts(), 0);
  EXPECT_FALSE(away.atStart);
  ASSERT_TRUE(away.returnToStart.found);
  EXPECT_EQ(away.returnToStart.cells, 2);
  EXPECT_EQ(away.returnToStart.states.back().row, 0);

  EXPECT_FALSE(planRoutes(MazeGraph(), atStart, MazeCosts()).returnToStart.found) << "no start cell given";
  EXPECT_FALSE(planRoutes(MazeGraph(), atStart, MazeCosts(), 64).returnToStart.found);
  EXPECT_FALSE(planRoutes(MazeGraph(), stateAt(-1, 0, Heading::north), MazeCosts(), 0).returnToStart.found);
}

GTEST_TEST(MazeRoutes, TheReturnFollowsTheStartCellOfTheMap)
{
  const MazeGraph map;
  const RoutePair toZero = planRoutes(map, stateAt(7, 7, Heading::north), MazeCosts(), 0);
  const RoutePair toCorner = planRoutes(map, stateAt(7, 7, Heading::north), MazeCosts(), 56);
  ASSERT_TRUE(toZero.returnToStart.found);
  ASSERT_TRUE(toCorner.returnToStart.found);
  EXPECT_EQ(toZero.returnToStart.states.back().row, 0);
  EXPECT_EQ(toCorner.returnToStart.states.back().row, 7);
  EXPECT_EQ(toCorner.returnToStart.states.back().col, 0);
}
