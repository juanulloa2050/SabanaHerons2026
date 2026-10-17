/**
 * @file MazeLocalizerAcceptanceTest.cpp
 *
 * Offline acceptance runs of the maze localizer and mapper (ticket MZ-020): a simulated walking robot with noisy
 * odometry and a drifting gyro moves through maze_1 and the pure cores of MazeLocalizer and MazeMapper process
 * what the oracle shows it. The measured numbers are printed with the prefix "[MZ-020]" for the pull request.
 * See MazeLocalizerSim.h for what is modelled and what is not.
 */

#include "MazeLocalizerSim.h"

#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Streaming/InStreams.h"

#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

namespace
{
  using namespace Maze;
  using namespace Maze::Sim;

  float deg(float degrees)
  {
    return Angle::fromDegrees(degrees);
  }

  void note(const std::string& text)
  {
    std::cout << "[MZ-020] " << text << std::endl;
  }

  MazeDescription loadMaze(int index)
  {
    MazeDescription maze;
    std::string error;
    EXPECT_TRUE(MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_" + std::to_string(index) + ".maze", maze, error)) << error;
    return maze;
  }

  /** The layout of a cfg text; registers the streamable enums once, like the application does at start-up. */
  MazeTagLayout layoutOf(const std::string& mode, const std::string& faces, const std::string& walls = "")
  {
    static const bool registered = (FunctionList::execute(), true);
    (void) registered;
    const std::string text = layoutText(mode, faces, walls);
    MazeTagLayout layout;
    InMapMemory stream(text.c_str(), text.size());
    stream >> layout;
    return layout;
  }

  Config configFor(const MazeDescription& maze, const MazeTagLayout& layout, unsigned seed = 1)
  {
    Config config;
    config.maze = maze;
    config.layout = layout;
    config.seed = seed;
    return config;
  }

  /** Result of walking a route. */
  struct RouteResult
  {
    int cells = 0;           /**< Forward steps. */
    int checkpoints = 0;     /**< Cell centres reached. */
    int cellsCorrect = 0;    /**< Checkpoints with the right estimated cell. */
    float finalPosition = 0.f;
    float finalHeading = 0.f;
    float maxCheckpointPosition = 0.f;
    float maxCheckpointHeading = 0.f;
    float timeSeconds = 0.f;
    Stats stats;
  };

  /** Walks `route`, checking the estimate at every cell centre. */
  RouteResult walk(WalkingRobot& run, const std::vector<Action>& route)
  {
    RouteResult result;
    run.stand(1);
    for(const Action action : route)
    {
      run.perform(action);
      if(action != Action::forward)
        continue;
      ++result.cells;
      ++result.checkpoints;
      result.cellsCorrect += run.cellCorrect() ? 1 : 0;
      result.maxCheckpointPosition = std::max(result.maxCheckpointPosition, run.positionError());
      result.maxCheckpointHeading = std::max(result.maxCheckpointHeading, run.headingError());
    }
    result.finalPosition = run.positionError();
    result.finalHeading = run.headingError();
    result.timeSeconds = static_cast<float>(run.time - 1000) / 1000.f;
    result.stats = run.stats;
    return result;
  }

  std::string describe(const RouteResult& r)
  {
    std::ostringstream text;
    text.precision(3);
    text << r.cells << " cells in " << r.timeSeconds << " s: final error " << r.finalPosition << " mm / " << Angle(r.finalHeading).toDegrees()
         << " deg, checkpoint max " << r.maxCheckpointPosition << " mm / " << Angle(r.maxCheckpointHeading).toDegrees() << " deg, mean "
         << r.stats.meanPositionError() << " mm, cell " << r.cellsCorrect << "/" << r.checkpoints << ", tag fixes " << r.stats.fixes
         << " (" << static_cast<float>(r.stats.fixes) / r.timeSeconds << "/s, " << static_cast<float>(r.stats.fixes) / static_cast<float>(std::max(1, r.cells)) << "/cell), frames with a fix "
         << 100.f * static_cast<float>(r.stats.framesWithFix) / static_cast<float>(std::max(1u, r.stats.perceptFrames)) << " %, rejected " << r.stats.rejections;
    return text.str();
  }

  const char* headingName(int heading)
  {
    constexpr const char* names[] = {"north", "east", "south", "west"};
    return names[heading];
  }

  std::string coverage(const Stats& stats)
  {
    std::ostringstream text;
    text.precision(3);
    for(int h = 0; h < headingCount; ++h)
    {
      const unsigned frames = stats.framesByHeading[static_cast<std::size_t>(h)];
      text << " " << headingName(h) << ": " << frames << " frames, tag in view "
           << (frames ? 100.f * static_cast<float>(stats.tagFramesByHeading[static_cast<std::size_t>(h)]) / static_cast<float>(frames) : 0.f) << " %, fix "
           << (frames ? 100.f * static_cast<float>(stats.fixFramesByHeading[static_cast<std::size_t>(h)]) / static_cast<float>(frames) : 0.f) << " %;";
    }
    return text.str();
  }
}

GTEST_TEST(MazeLocalizerAcceptance, TenCellNoisyRunStaysWithinEightCentimetresAndTenDegrees)
{
  const MazeDescription maze = loadMaze(1);
  const std::vector<Action> route = explorationRoute(maze);
  const int cells = static_cast<int>(std::count(route.begin(), route.end(), Action::forward));
  ASSERT_GE(cells, 10);
  note("exploration route of maze_1 (edges learned on arrival, enters the detour at cell 11): " + std::to_string(cells) + " cells, " + std::to_string(route.size() - static_cast<std::size_t>(cells)) + " turns");

  // 5 % odometry error in translation (systematic and per frame), 2 deg/m odometry rotation, 3 deg/min gyro drift.
  for(unsigned seed = 1; seed <= 2; ++seed)
  {
    WalkingRobot run(configFor(maze, layoutOf("allFaces", "north, east, south, west"), seed));
    const RouteResult result = walk(run, route);
    note("nominal noise (3 deg/min), seed " + std::to_string(seed) + ": " + describe(result));
    EXPECT_LE(result.finalPosition, 80.f) << "seed " << seed;
    EXPECT_LE(result.finalHeading, deg(10.f)) << "seed " << seed;
    EXPECT_EQ(result.cellsCorrect, result.checkpoints) << "seed " << seed;
    EXPECT_EQ(result.stats.wrongCellFrames, 0u) << "seed " << seed;
    EXPECT_GT(result.stats.fixes, 0u);
  }

  // Harsher: 15 deg/min gyro drift, 10 % odometry error, one sixth of the tags missed.
  {
    Config config = configFor(maze, layoutOf("allFaces", "north, east, south, west"));
    config.noise.gyroDriftDegPerMinute = 15.f;
    config.noise.odometryTranslationBias = 0.1f;
    config.noise.dropoutRate = 0.17f;
    WalkingRobot run(config);
    const RouteResult result = walk(run, route);
    note("harsh noise (15 deg/min, odometry +10 %, dropout 0.17): " + describe(result));
    EXPECT_LE(result.finalPosition, 80.f);
    EXPECT_LE(result.finalHeading, deg(10.f));
    EXPECT_EQ(result.cellsCorrect, result.checkpoints);
  }

  // Without tags the same noise accumulates: the tags are what the acceptance relies on.
  Config blind = configFor(maze, layoutOf("allFaces", "north, east, south, west"));
  blind.noise.dropoutRate = 1.f;
  WalkingRobot blindRun(blind);
  const RouteResult blindResult = walk(blindRun, route);
  note("same route without tags (dropout 1.0): " + describe(blindResult));
  EXPECT_EQ(blindResult.stats.fixes, 0u);
}

GTEST_TEST(MazeLocalizerAcceptance, WithoutTagsTheQualityDegradesToPoor)
{
  const MazeDescription maze = loadMaze(1);
  Config config = configFor(maze, layoutOf("allFaces", "north, east, south, west"));
  config.noise.dropoutRate = 1.f;
  WalkingRobot run(config);
  const LocalizationParameters& p = config.localization;

  LocalizationQuality worst = LocalizationQuality::superb;
  unsigned okayAt = 0, poorAt = 0;
  float okayDistance = 0.f, poorDistance = 0.f;
  bool improved = false;
  const unsigned start = run.time;
  // Watch the quality in every frame.
  run.onFrame = [&](const WalkingRobot& robot)
  {
    const LocalizationQuality quality = robot.localization.quality(robot.time);
    if(static_cast<int>(quality) < static_cast<int>(worst))
      improved = true;
    if(quality != LocalizationQuality::superb && !okayAt)
    {
      okayAt = robot.time - start;
      okayDistance = robot.localization.distanceSinceFix;
    }
    if(quality == LocalizationQuality::poor && !poorAt)
    {
      poorAt = robot.time - start;
      poorDistance = robot.localization.distanceSinceFix;
    }
    worst = static_cast<LocalizationQuality>(std::max(static_cast<int>(worst), static_cast<int>(quality)));
  };
  run.stand(1);
  EXPECT_EQ(run.localization.quality(run.time), LocalizationQuality::superb); // a reset pose is known
  for(const Action action : explorationRoute(maze))
    run.perform(action);
  note("no tags: quality okay after " + std::to_string(okayAt) + " ms / " + std::to_string(static_cast<int>(okayDistance)) + " mm walked, poor after " +
       std::to_string(poorAt) + " ms / " + std::to_string(static_cast<int>(poorDistance)) + " mm (thresholds: superb " +
       std::to_string(static_cast<int>(p.superbMaxTime)) + " ms or " + std::to_string(static_cast<int>(p.superbMaxDistance)) + " mm, okay " +
       std::to_string(static_cast<int>(p.okayMaxTime)) + " ms or " + std::to_string(static_cast<int>(p.okayMaxDistance)) + " mm)");
  EXPECT_EQ(run.stats.fixes, 0u);
  EXPECT_FALSE(improved);
  EXPECT_EQ(worst, LocalizationQuality::poor);
  ASSERT_GT(poorAt, 0u);
  EXPECT_GT(poorAt, okayAt);
  // Poor as soon as one of the two limits is crossed (within a frame and the walk of a frame).
  EXPECT_LE(poorAt, static_cast<unsigned>(p.okayMaxTime) + 2 * config.frameMs);
  EXPECT_LE(poorDistance, p.okayMaxDistance + 50.f);
  EXPECT_TRUE(poorAt >= static_cast<unsigned>(p.okayMaxTime) || poorDistance >= p.okayMaxDistance);
}

GTEST_TEST(MazeLocalizerAcceptance, PickUpAndStandbyKeepTheMapAndStartTheSecondAttempt)
{
  const MazeDescription maze = loadMaze(1);
  WalkingRobot run(configFor(maze, layoutOf("allFaces", "north, east, south, west")));
  const std::vector<Action> route = explorationRoute(maze);
  run.stand(1);
  ASSERT_EQ(run.fusion.attempt, 1);
  for(std::size_t i = 0; i < 8 && i < route.size(); ++i)
    run.perform(route[i]);
  int decided = 0;
  for(int row = 0; row < size; ++row)
    for(int col = 0; col < size; ++col)
      for(int h = 0; h < headingCount; ++h)
        decided += run.fusion.graph.get(row, col, static_cast<Heading>(h)) != EdgeState::unknown ? 1 : 0;
  ASSERT_GT(decided, 4 * size);

  const MazeMapFusion before = run.fusion;
  // Pick-up: the game state leaves playing, the robot is carried across the maze and sees other walls meanwhile.
  run.setPlaying(false);
  run.carryTo(45, Heading::west);
  run.stand(100);
  EXPECT_TRUE(run.fusion == before) << "the map must not learn while carried";
  EXPECT_EQ(run.fusion.attempt, 1);
  // Put back at the start, Standby ends, playing again.
  run.carryTo(maze.startCell, maze.startHeading);
  // The frame of the transition itself sees nothing (dropout), so that the map can be compared before it learns again.
  run.config.noise.dropoutRate = 1.f;
  run.setPlaying(true);
  run.config.noise.dropoutRate = 0.f;
  EXPECT_TRUE(run.fusion == before);
  EXPECT_EQ(run.fusion.attempt, 2);
  EXPECT_EQ(run.resets, 2u);
  const Vector2f startCentre = Oracle::cellCentreOf(cellRow(maze.startCell), cellCol(maze.startCell));
  EXPECT_LT((run.localization.pose.translation - startCentre).norm(), 30.f);
  EXPECT_EQ(run.localization.heading(), maze.startHeading);
  EXPECT_TRUE(run.cellCorrect());
  note("pick-up and Standby after 8 actions: " + std::to_string(decided) + " edges known, map identical, attempt " + std::to_string(run.fusion.attempt) +
       ", pose reset to cell " + std::to_string(maze.startCell));

  // The second attempt keeps working on the same map.
  run.stand(1);
  for(std::size_t i = 0; i < 8 && i < route.size(); ++i)
    run.perform(route[i]);
  EXPECT_TRUE(run.cellCorrect());
}

GTEST_TEST(MazeLocalizerAcceptance, OracleLookAheadKeepsTheRobotOutOfTheDeadEndAtCell11)
{
  const MazeDescription maze = loadMaze(1);
  constexpr int deadEnd = 11;
  // Cell 11 is closed to the east and the south: entering it from cell 10 is the detour that the ticket wants to avoid.
  ASSERT_EQ(maze.graph.get(1, 3, Heading::west), EdgeState::open);
  ASSERT_EQ(maze.graph.get(1, 3, Heading::east), EdgeState::wall);
  ASSERT_EQ(maze.graph.get(1, 3, Heading::south), EdgeState::wall);

  // Baseline: learning the four edges of a cell on arrival (the model of MazeExplorationSim) walks into it.
  {
    MazeState state;
    state.row = cellRow(maze.startCell);
    state.col = cellCol(maze.startCell);
    state.heading = maze.startHeading;
    int cells = 1, enteredAt = -1;
    for(const Action action : explorationRoute(maze))
    {
      switch(action)
      {
        case Action::turnLeft: state.heading = turned(state.heading, -1); break;
        case Action::turnRight: state.heading = turned(state.heading, 1); break;
        case Action::turnAround: state.heading = turned(state.heading, 2); break;
        default:
          state.row += rowDelta[static_cast<std::size_t>(state.heading)];
          state.col += colDelta[static_cast<std::size_t>(state.heading)];
          if(cellId(state.row, state.col) == deadEnd && enteredAt < 0)
            enteredAt = cells;
          ++cells;
          break;
      }
    }
    note("baseline, edges learned on arrival: " + std::to_string(cells) + " cells, enters cell 11 as cell number " + std::to_string(enteredAt));
    EXPECT_GE(enteredAt, 0) << "the scenario is meaningless if the baseline avoids the detour anyway";
  }

  // With the oracle look-ahead (maxRange 2) collected by the fusion at every frame.
  WalkingRobot run(configFor(maze, layoutOf("allFaces", "north, east, south, west")));
  const Exploration exploration = explore(run, false, 0, deadEnd);
  ASSERT_TRUE(exploration.reachedGoal) << exploration.error;
  EXPECT_EQ(exploration.firstWatchStep, -1) << "the robot entered the dead end";
  EXPECT_EQ(std::count(exploration.visitedCells.begin(), exploration.visitedCells.end(), deadEnd), 0);
  EXPECT_EQ(exploration.cellMismatches, 0);
  EXPECT_EQ(run.stats.wrongCellFrames, 0u);

  // Every decided edge is right, and edges of cells the robot never entered were decided from afar.
  int decidedWrong = 0, decidedUnvisited = 0, decided = 0;
  for(int row = 0; row < size; ++row)
    for(int col = 0; col < size; ++col)
    {
      const bool visited = std::count(exploration.visitedCells.begin(), exploration.visitedCells.end(), cellId(row, col)) > 0;
      for(int h = 0; h < headingCount; ++h)
      {
        const Heading heading = static_cast<Heading>(h);
        const EdgeState state = run.fusion.graph.get(row, col, heading);
        if(state == EdgeState::unknown)
          continue;
        ++decided;
        decidedWrong += state != maze.graph.get(row, col, heading) ? 1 : 0;
        decidedUnvisited += visited ? 0 : 1;
      }
    }
  EXPECT_EQ(decidedWrong, 0);
  EXPECT_GT(decidedUnvisited, 0);
  EXPECT_EQ(run.fusion.graph.get(1, 3, Heading::east), EdgeState::wall);
  EXPECT_EQ(run.fusion.graph.get(1, 3, Heading::south), EdgeState::wall);
  note("oracle look-ahead (maxRange 2, every frame): " + std::to_string(exploration.visitedCells.size()) + " cells to the goal, never enters cell 11, " +
       std::to_string(decided) + " edge-sides decided, of which " + std::to_string(decidedUnvisited) + " belong to cells never entered, " +
       std::to_string(decidedWrong) + " wrong; the east and south walls of cell 11 are decided from outside");

  // How much observation does it take? The robot looks only while it stands, a number of frames at every cell centre.
  std::ostringstream table;
  int needed = -1;
  for(int frames = 1; frames <= 6; ++frames)
  {
    Config config = configFor(maze, layoutOf("allFaces", "north, east, south, west"));
    config.observeWhileMoving = false;
    WalkingRobot standing(config);
    standing.stand(1);
    // The vision runs every 100 ms: standing 3 odometry frames gives one percept.
    const Exploration result = explore(standing, false, frames * 3, deadEnd);
    const bool avoided = result.reachedGoal && result.firstWatchStep < 0;
    table << " " << frames << (avoided ? ":avoids" : ":enters");
    if(avoided)
    {
      needed = frames;
      break;
    }
  }
  note("vision frames (10 Hz) standing at every cell centre, no look while walking:" + table.str() + "; minimum " + std::to_string(needed));
  EXPECT_GT(needed, 0) << "even 6 frames per cell are not enough";
}

GTEST_TEST(MazeLocalizerAcceptance, FramesToCloseTheDeadEndFromCell10)
{
  const MazeDescription maze = loadMaze(1);
  // Cell 10 facing east: cell 11 is one ahead (500 mm, full weight); three observations of full weight decide an edge.
  Config config = configFor(maze, layoutOf("allFaces", "north, east, south, west"));
  config.maze.startCell = 10;
  config.maze.startHeading = Heading::east;
  WalkingRobot run(config);
  int frames = 0;
  const auto closed = [&]()
  {
    return run.fusion.graph.get(1, 3, Heading::east) == EdgeState::wall && run.fusion.graph.get(1, 3, Heading::south) == EdgeState::wall;
  };
  while(!closed() && frames < 20)
  {
    run.stand(1);
    frames = static_cast<int>(run.stats.perceptFrames);
  }
  note("standing in cell 10 facing east, cell 11 is seen closed to the east and south after " + std::to_string(frames) +
       " vision frames (threshold 3, one observation per edge every 250 ms, wall distance noise 10 mm)");
  EXPECT_TRUE(closed());
  // An edge takes one observation per 250 ms (minObservationInterval): 3 observations at 10 Hz are frames 1, 4 and 7, so 0.6 s.
  EXPECT_LE(frames, 7);
}

GTEST_TEST(MazeLocalizerAcceptance, TagFixRateForEveryLayoutMode)
{
  const MazeDescription maze = loadMaze(1);
  const std::vector<Action> route = explorationRoute(maze);

  // Half of the cells (a checkerboard) carry a tag on every face: an example of the `walls` mode.
  std::string walls;
  for(int cell = 0; cell < cellCount; ++cell)
    if((cellRow(cell) + cellCol(cell)) % 2 == 0)
      for(const char* face : {"north", "east", "south", "west"})
        walls += std::string(walls.empty() ? "" : ", ") + "{cell = " + std::to_string(cell) + "; face = " + face + ";}";

  struct Mode
  {
    const char* name;
    MazeTagLayout layout;
  };
  const std::vector<Mode> modes = {
    {"allFaces", layoutOf("allFaces", "north, east, south, west")},
    {"faces [north]", layoutOf("faces", "north")},
    {"walls (checkerboard cells, all faces)", layoutOf("walls", "", walls)},
  };
  for(const Mode& mode : modes)
  {
    WalkingRobot run(configFor(maze, mode.layout));
    const RouteResult result = walk(run, route);
    note(std::string("layout ") + mode.name + ": " + describe(result));
    EXPECT_EQ(result.stats.wrongCellFrames, 0u) << mode.name;
    EXPECT_TRUE(std::isfinite(result.finalPosition));
    if(std::string(mode.name) == "allFaces" || std::string(mode.name).rfind("walls", 0) == 0)
    {
      EXPECT_GT(result.stats.fixes, 0u) << mode.name;
      EXPECT_LE(result.finalPosition, 80.f) << mode.name;
      EXPECT_LE(result.finalHeading, deg(10.f)) << mode.name;
    }
  }
}

GTEST_TEST(MazeLocalizerAcceptance, NorthFaceOnlyKeepsTheCellWhileFacingNorthAndLosesCoverageElsewhere)
{
  const MazeDescription maze = loadMaze(1);
  const MazeTagLayout north = layoutOf("faces", "north");

  // Coverage by heading on the route of the other criteria.
  {
    WalkingRobot run(configFor(maze, north));
    const RouteResult result = walk(run, explorationRoute(maze));
    note("faces [north] on the route of maze_1: " + describe(result));
    note("faces [north] coverage by true heading:" + coverage(result.stats));
    WalkingRobot all(configFor(maze, layoutOf("allFaces", "north, east, south, west")));
    const RouteResult reference = walk(all, explorationRoute(maze));
    note("allFaces coverage by true heading:" + coverage(reference.stats));
    // A north face cannot be seen from behind: facing south there is no fix. Facing east or west only the faces of cells
    // ahead, seen at a flat angle, can be read, so the coverage is much lower than facing north.
    const auto fixShare = [&](Heading heading)
    {
      const std::size_t h = static_cast<std::size_t>(heading);
      return result.stats.framesByHeading[h] ? static_cast<float>(result.stats.fixFramesByHeading[h]) / static_cast<float>(result.stats.framesByHeading[h]) : 0.f;
    };
    EXPECT_EQ(result.stats.fixFramesByHeading[static_cast<std::size_t>(Heading::south)], 0u);
    EXPECT_GT(fixShare(Heading::north), 0.5f);
    EXPECT_LT(fixShare(Heading::east), fixShare(Heading::north));
    EXPECT_LT(result.stats.fixes, reference.stats.fixes);
  }

  // The longest straight run to the north in maze_1.
  int bestCell = 0, bestLength = 0;
  for(int cell = 0; cell < cellCount; ++cell)
  {
    int length = 0, row = cellRow(cell);
    while(row > 0 && maze.graph.get(row, cellCol(cell), Heading::north) == EdgeState::open)
    {
      --row;
      ++length;
    }
    if(length > bestLength)
    {
      bestLength = length;
      bestCell = cell;
    }
  }
  ASSERT_GE(bestLength, 3);
  note("longest corridor to the north in maze_1: " + std::to_string(bestLength) + " cells from cell " + std::to_string(bestCell));

  // Walk it north: the tags on the north faces ahead are in view; compare with walking it south (back) with the same layout.
  Config config = configFor(maze, north);
  config.maze.startCell = bestCell;
  config.maze.startHeading = Heading::north;
  WalkingRobot up(config);
  const RouteResult upResult = walk(up, std::vector<Action>(static_cast<std::size_t>(bestLength), Action::forward));
  note("facing north along that corridor: " + describe(upResult));
  EXPECT_EQ(upResult.cellsCorrect, upResult.checkpoints);
  EXPECT_EQ(upResult.stats.wrongCellFrames, 0u);
  EXPECT_GT(upResult.stats.fixes, 0u);
  EXPECT_LE(upResult.finalPosition, 80.f);

  Config down = config;
  down.maze.startCell = bestCell - bestLength * size;
  down.maze.startHeading = Heading::south;
  WalkingRobot downRun(down);
  const RouteResult downResult = walk(downRun, std::vector<Action>(static_cast<std::size_t>(bestLength), Action::forward));
  note("facing south back along the corridor (only north faces carry tags): " + describe(downResult));
  EXPECT_EQ(downResult.stats.fixes, 0u);

  // A head scan of +-60 degrees brings the north faces into view while the robot faces east or west.
  Config scan = configFor(maze, north);
  scan.headScanAmplitude = deg(60.f);
  WalkingRobot scanRun(scan);
  const RouteResult scanResult = walk(scanRun, explorationRoute(maze));
  note("faces [north] with a head scan of +-60 deg: " + describe(scanResult));
  note("faces [north] with head scan, coverage by true heading:" + coverage(scanResult.stats));
}
