#include "MazeExplorationSim.h"
#include "Platform/File.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

// GCC defines __SANITIZE_*__; Clang reports sanitizers through __has_feature.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define MAZE_SANITIZED_BUILD 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define MAZE_SANITIZED_BUILD 1
#endif
#endif

namespace
{
  using namespace Maze;

  constexpr int mazeFiles = 20;

  std::string mazePath(int index)
  {
    return std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_" + std::to_string(index) + ".maze";
  }

  std::string readFile(const std::string& path)
  {
    std::ifstream file(path, std::ios::binary);
    if(!file)
    {
      ADD_FAILURE() << "cannot open " << path;
      return std::string();
    }
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
  }

  MazeDescription loadMaze(int index)
  {
    MazeDescription maze;
    std::string error;
    EXPECT_TRUE(MazeFile::load(mazePath(index), maze, error)) << error;
    return maze;
  }

  MazeState startOf(const MazeDescription& maze)
  {
    MazeState state;
    state.row = cellRow(maze.startCell);
    state.col = cellCol(maze.startCell);
    state.heading = maze.startHeading;
    return state;
  }

  /** The maze turned clockwise by 90 degrees, with start cell and start heading turned the same way. */
  MazeDescription rotated(const MazeDescription& maze)
  {
    MazeDescription result;
    result.graph = maze.graph.rotated90();
    result.startCell = cellId(cellCol(maze.startCell), size - 1 - cellRow(maze.startCell));
    result.startHeading = turned(maze.startHeading, 1);
    return result;
  }

  /** The optimal route on a fully known maze. */
  MazePlan optimalRoute(const MazeDescription& maze, const MazeCosts& costs = MazeCosts())
  {
    MazePlanner planner(costs);
    planner.computeCostToGo(maze.graph, UnknownPolicy::asWall);
    return planner.routeFromTable(startOf(maze));
  }

  /** "40.100" -> 40100; also accepts "40.1" and "40". */
  int secondsToMs(const std::string& text)
  {
    const std::size_t dot = text.find('.');
    const int seconds = std::stoi(text.substr(0, dot));
    if(dot == std::string::npos)
      return seconds * 1000;
    std::string fraction = text.substr(dot + 1);
    EXPECT_LE(fraction.size(), 3u) << "more than millisecond precision in \"" << text << "\"";
    fraction.resize(3, '0');
    return seconds * 1000 + std::stoi(fraction);
  }

  std::vector<std::string> split(const std::string& line)
  {
    std::vector<std::string> fields;
    std::istringstream stream(line);
    for(std::string field; std::getline(stream, field, ',');)
      fields.push_back(field);
    return fields;
  }

  struct ExpectedRow
  {
    std::string file, sha;
    int cells, turns90, turns180, timeMs;
    int exploreCells, exploreTurns, exploreTimeMs;
  };

  std::vector<ExpectedRow> readExpected()
  {
    std::vector<ExpectedRow> rows;
    std::istringstream stream(readFile(std::string(File::getBHDir()) + "/Config/Mazes/generated/expected.csv"));
    std::string line;
    std::getline(stream, line); // header
    while(std::getline(stream, line))
    {
      const std::vector<std::string> f = split(line);
      if(f.size() != 9)
      {
        ADD_FAILURE() << "expected.csv row has " << f.size() << " fields, expected 9: \"" << line << "\"";
        continue;
      }
      rows.push_back({f[0], f[1], std::stoi(f[2]), std::stoi(f[3]), std::stoi(f[4]), secondsToMs(f[5]),
                      std::stoi(f[6]), std::stoi(f[7]), secondsToMs(f[8])});
    }
    return rows;
  }

  int mazeIndex(const std::string& file)
  {
    return std::stoi(file.substr(file.find('_') + 1));
  }

  /**
   * Row 3 from the start in column 0 to the goal cell (3, 3), every other inner edge a wall. The edge between
   * (3, 1) and (3, 2) is `shortcut`; a detour over row 2 around it is known to be open if `withDetour`.
   */
  MazeGraph corridor(EdgeState shortcut, bool withDetour)
  {
    MazeGraph graph;
    for(int row = 0; row < size; ++row)
      for(int col = 0; col < size; ++col)
      {
        if(col < size - 1)
          graph.set(row, col, Heading::east, EdgeState::wall);
        if(row < size - 1)
          graph.set(row, col, Heading::south, EdgeState::wall);
      }
    graph.set(3, 0, Heading::east, EdgeState::open);
    graph.set(3, 1, Heading::east, shortcut);
    graph.set(3, 2, Heading::east, EdgeState::open);
    if(withDetour)
    {
      graph.set(3, 1, Heading::north, EdgeState::open);
      graph.set(2, 1, Heading::east, EdgeState::open);
      graph.set(2, 2, Heading::east, EdgeState::open);
      graph.set(2, 3, Heading::south, EdgeState::open);
    }
    return graph;
  }
}

GTEST_TEST(MazePlanner, ExpectedCsvMatchesMazeFiles)
{
  const std::vector<ExpectedRow> rows = readExpected();
  ASSERT_EQ(rows.size(), static_cast<std::size_t>(mazeFiles));
  std::set<std::string> files;
  for(const ExpectedRow& row : rows)
    files.insert(row.file);
  EXPECT_EQ(files.size(), static_cast<std::size_t>(mazeFiles));
  for(int i = 1; i <= mazeFiles; ++i)
    EXPECT_EQ(files.count("maze_" + std::to_string(i) + ".maze"), 1u) << "maze_" << i;
  for(const ExpectedRow& row : rows)
  {
    const std::string sha = MazeFile::toHex(MazeFile::fnv1a32(readFile(mazePath(mazeIndex(row.file)))));
    ASSERT_EQ(sha, row.sha) << row.file << ": expected.csv is stale, run solve_maze.py --update";
  }
}

GTEST_TEST(MazePlanner, OptimalRouteMatchesReference)
{
  const std::vector<ExpectedRow> rows = readExpected();
  ASSERT_EQ(rows.size(), static_cast<std::size_t>(mazeFiles));
  for(const ExpectedRow& row : rows)
  {
    SCOPED_TRACE(row.file);
    const MazeDescription maze = loadMaze(mazeIndex(row.file));
    const MazePlan route = optimalRoute(maze);
    ASSERT_TRUE(route.found);
    EXPECT_EQ(route.cells, row.cells);
    EXPECT_EQ(route.turns90, row.turns90);
    EXPECT_EQ(route.turns180, row.turns180);
    EXPECT_EQ(route.timeMs, row.timeMs);

    // The A* speed run on the fully known maze finds a route that is as good.
    const MazePlan speedRun = MazePlanner().speedRun(maze.graph, startOf(maze));
    ASSERT_TRUE(speedRun.found);
    EXPECT_EQ(speedRun.timeMs, row.timeMs);
    EXPECT_EQ(speedRun.turns90, row.turns90);
    EXPECT_EQ(speedRun.turns180, row.turns180);
    EXPECT_EQ(speedRun.cells, row.cells);
  }
}

GTEST_TEST(MazePlanner, ExplorationMatchesReference)
{
  const std::vector<ExpectedRow> rows = readExpected();
  ASSERT_EQ(rows.size(), static_cast<std::size_t>(mazeFiles));
  for(const ExpectedRow& row : rows)
  {
    SCOPED_TRACE(row.file);
    const ExplorationResult result = simulateExploration(loadMaze(mazeIndex(row.file)));
    ASSERT_TRUE(result.success) << result.error;
    EXPECT_EQ(result.cells, row.exploreCells);
    EXPECT_EQ(result.turns, row.exploreTurns);
    EXPECT_EQ(result.timeMs, row.exploreTimeMs);
  }
}

GTEST_TEST(MazePlanner, HeuristicIsAdmissible)
{
  const MazePlanner planner;
  MazePlanner table;
  for(int i = 1; i <= mazeFiles; ++i)
  {
    SCOPED_TRACE("maze_" + std::to_string(i));
    table.computeCostToGo(loadMaze(i).graph, UnknownPolicy::asWall);
    for(int row = 0; row < size; ++row)
      for(int col = 0; col < size; ++col)
        for(int h = 0; h < headingCount; ++h)
          for(int moving = 0; moving < 2; ++moving)
          {
            const MazeState state = {row, col, static_cast<Heading>(h), moving == 1};
            const MazeCost toGo = table.costToGo(state);
            if(!toGo.isInfinite())
              ASSERT_LE(planner.heuristic(state), toGo.timeMs) << "cell " << cellId(row, col) << " heading " << h << " moving " << moving;
          }
  }
}

GTEST_TEST(MazePlanner, RotationsKeepTimeAndTurns)
{
  for(int i = 1; i <= mazeFiles; ++i)
  {
    SCOPED_TRACE("maze_" + std::to_string(i));
    MazeDescription maze = loadMaze(i);
    const MazePlan reference = optimalRoute(maze);
    ASSERT_TRUE(reference.found);
    for(int quarter = 1; quarter <= 3; ++quarter)
    {
      maze = rotated(maze);
      SCOPED_TRACE("rotated by " + std::to_string(quarter * 90));
      const MazePlan route = optimalRoute(maze);
      ASSERT_TRUE(route.found);
      EXPECT_EQ(route.timeMs, reference.timeMs);
      EXPECT_EQ(route.cells, reference.cells);
      EXPECT_EQ(route.turns90, reference.turns90);
      EXPECT_EQ(route.turns180, reference.turns180);
    }
  }
}

GTEST_TEST(MazePlanner, PlanningIsFastEnough)
{
  constexpr int batches = 5;
  constexpr int runsPerBatch = 100;
  constexpr double limitMs = 5.0;
  const MazeDescription maze = loadMaze(1);
  MazePlanner planner;
  const MazeGraph unexplored; // only the border is known: the most edges to relax

  // The fastest batch is used, so a short load spike on the host does not fail the test.
  int sink = 0;
  auto fastestBatchMs = [&](auto&& run)
  {
    double best = std::numeric_limits<double>::max();
    for(int batch = 0; batch < batches; ++batch)
    {
      const auto begin = std::chrono::steady_clock::now();
      for(int i = 0; i < runsPerBatch; ++i)
        sink += run(i);
      best = std::min(best, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / runsPerBatch);
    }
    return best;
  };

  const double replanMs = fastestBatchMs([&](int i)
  {
    planner.computeCostToGo(i % 2 == 0 ? unexplored : maze.graph, UnknownPolicy::asOpen);
    return planner.costToGo(startOf(maze)).timeMs;
  });
  const double speedRunMs = fastestBatchMs([&](int)
  {
    return planner.speedRun(maze.graph, startOf(maze)).timeMs;
  });

  std::cout << "[ MazePlanner ] replan " << replanMs << " ms, speed run " << speedRunMs << " ms (sink " << sink << ")\n";
  // Even the unoptimized Develop build of Tests is ~10x below the limit; sanitizers are not.
#ifdef MAZE_SANITIZED_BUILD
  static_cast<void>(limitMs);
  std::cout << "[ MazePlanner ] timing limit not checked: sanitized build\n";
#else
  EXPECT_LT(replanMs, limitMs);
  EXPECT_LT(speedRunMs, limitMs);
#endif
}

GTEST_TEST(MazePlanner, FreshPlannerReachesNothing)
{
  const MazePlanner planner;
  const MazeState start = {0, 0, Heading::south, false};
  Action action;
  EXPECT_TRUE(planner.costToGo(start).isInfinite());
  EXPECT_FALSE(planner.nextAction(start, action));
}

GTEST_TEST(MazePlanner, InvalidStatesAreUnreachable)
{
  const MazeDescription maze = loadMaze(1);
  MazePlanner planner;
  planner.computeCostToGo(maze.graph, UnknownPolicy::asOpen);
  const std::array<MazeState, 5> invalid = {{
    {-1, 0, Heading::south, false},
    {0, -1, Heading::east, false},
    {size, 0, Heading::north, false},
    {0, size, Heading::west, true},
    {0, 0, static_cast<Heading>(headingCount), false},
  }};
  for(const MazeState& state : invalid)
  {
    Action action;
    MazeState next;
    MazeCost cost;
    EXPECT_FALSE(MazePlanner::isValid(state));
    EXPECT_TRUE(planner.costToGo(state).isInfinite());
    EXPECT_FALSE(planner.nextAction(state, action));
    EXPECT_FALSE(planner.apply(maze.graph, UnknownPolicy::asOpen, state, Action::forward, next, cost));
    EXPECT_FALSE(planner.routeFromTable(state).found);
    EXPECT_FALSE(planner.speedRun(maze.graph, state).found);
  }
  EXPECT_TRUE(MazePlanner::isValid(startOf(maze)));
}

GTEST_TEST(MazePlanner, SecondsToMsAcceptsShortFractions)
{
  EXPECT_EQ(secondsToMs("40.100"), 40100);
  EXPECT_EQ(secondsToMs("40.1"), 40100);
  EXPECT_EQ(secondsToMs("40"), 40000);
  EXPECT_EQ(secondsToMs("0.005"), 5);
}

GTEST_TEST(MazePlanner, UnknownEdgeIsOpenForExplorationAndWallForSpeedRun)
{
  const MazeState start = {3, 0, Heading::east, false};
  MazePlanner planner;

  // The shortcut is unknown: exploration walks through it (3 cells), the speed run takes the known detour (5 cells).
  const MazeGraph withDetour = corridor(EdgeState::unknown, true);
  planner.computeCostToGo(withDetour, UnknownPolicy::asOpen);
  const MazePlan exploring = planner.routeFromTable(start);
  ASSERT_TRUE(exploring.found);
  EXPECT_EQ(exploring.cells, 3);
  EXPECT_EQ(exploring.turns90 + exploring.turns180, 0);

  const MazePlan detour = planner.speedRun(withDetour, start);
  ASSERT_TRUE(detour.found);
  EXPECT_EQ(detour.cells, 5);
  EXPECT_GT(detour.timeMs, exploring.timeMs);

  // Without the detour the speed run has no route, as with a wall on the shortcut; exploration still has one.
  const MazeGraph unknownOnly = corridor(EdgeState::unknown, false);
  EXPECT_FALSE(planner.speedRun(unknownOnly, start).found);
  EXPECT_FALSE(planner.speedRun(corridor(EdgeState::wall, false), start).found);
  planner.computeCostToGo(unknownOnly, UnknownPolicy::asOpen);
  EXPECT_TRUE(planner.routeFromTable(start).found);
  planner.computeCostToGo(unknownOnly, UnknownPolicy::asWall);
  EXPECT_FALSE(planner.routeFromTable(start).found);

  // Once the shortcut is known to be open the speed run uses it.
  const MazePlan shortcut = planner.speedRun(corridor(EdgeState::open, true), start);
  ASSERT_TRUE(shortcut.found);
  EXPECT_EQ(shortcut.cells, 3);
}

GTEST_TEST(MazePlanner, NeverLeavesTheGridOnAnUnexploredGraph)
{
  MazePlanner planner;
  const MazeGraph unexplored;
  planner.computeCostToGo(unexplored, UnknownPolicy::asOpen);
  int checked = 0;
  for(int row = 0; row < size; ++row)
    for(int col = 0; col < size; ++col)
      for(int h = 0; h < headingCount; ++h)
        for(int moving = 0; moving < 2; ++moving)
        {
          const MazeState state = {row, col, static_cast<Heading>(h), moving == 1};
          ++checked;
          Action action;
          if(!planner.nextAction(state, action))
          {
            EXPECT_TRUE(isGoal(row, col)) << "cell " << cellId(row, col);
            continue;
          }
          MazeState next;
          MazeCost step;
          ASSERT_TRUE(planner.apply(unexplored, UnknownPolicy::asOpen, state, action, next, step));
          EXPECT_TRUE(inside(next.row, next.col)) << "cell " << cellId(row, col) << " heading " << h;
          // Facing the border, going forward is never offered.
          if(!inside(row + rowDelta[static_cast<std::size_t>(h)], col + colDelta[static_cast<std::size_t>(h)]))
            EXPECT_NE(action, Action::forward);
        }
  EXPECT_EQ(checked, stateCount);
}
