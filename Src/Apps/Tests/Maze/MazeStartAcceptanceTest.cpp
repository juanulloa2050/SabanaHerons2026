/**
 * @file MazeStartAcceptanceTest.cpp
 *
 * Offline acceptance runs of the start deduction (ticket MZ-024): a simulated robot explores mazes that start in each
 * corner with the start unknown, and the shared `StartAnchoredMap` (the logic of MazeMapper) has to find the start. The
 * measured numbers are printed with the prefix "[MZ-024]" for the pull request. See MazeLocalizerSim.h for what is modelled.
 */

#include "MazeLocalizerSim.h"

#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Streaming/InStreams.h"

#include <gtest/gtest.h>
#include <iostream>
#include <set>

namespace
{
  using namespace Maze;
  using namespace Maze::Sim;

  void note(const std::string& text)
  {
    std::cout << "[MZ-024] " << text << std::endl;
  }

  /** A maze of Config/Mazes/fixtures that starts in `hypothesis`: start_<cell>_<heading>.maze, made with generate_maze.py --start <cell>. */
  MazeDescription loadFixture(const Hypothesis& hypothesis)
  {
    constexpr const char* names[] = {"north", "east", "south", "west"};
    MazeDescription maze;
    std::string error;
    const std::string path = std::string(File::getBHDir()) + "/Config/Mazes/fixtures/start_" + std::to_string(hypothesis.cell) + "_" + names[static_cast<int>(hypothesis.heading)] + ".maze";
    EXPECT_TRUE(MazeFile::load(path, maze, error)) << path << ": " << error;
    EXPECT_EQ(maze.startCell, hypothesis.cell);
    EXPECT_EQ(maze.startHeading, hypothesis.heading);
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
    config.startGiven = false;
    return config;
  }

  const MazeTagLayout& everyFace()
  {
    static const MazeTagLayout layout = layoutOf("allFaces", "north, east, south, west");
    return layout;
  }
}

namespace
{
  /** What it took to find the start. */
  struct Deduction
  {
    bool known = false;
    int cells = 0;           /**< Distinct true cells the robot had stood in when the start became known. */
    unsigned frames = 0;     /**< Frames until then. */
    StartStatus status;
    Exploration exploration;
  };

  /** Explores with the planner from an unknown start and notes when the start became known. */
  Deduction exploreUntilKnown(const Config& config)
  {
    WalkingRobot run(config);
    std::set<int> cells;
    Deduction result;
    run.onFrame = [&](const WalkingRobot& robot)
    {
      int row = 0, col = 0;
      if(robot.trueCell(row, col))
        cells.insert(cellId(row, col));
      if(!result.known && robot.anchored.status().known)
      {
        result.known = true;
        result.cells = static_cast<int>(cells.size());
        result.frames = robot.stats.frames;
      }
    };
    result.exploration = explore(run, false);
    result.status = run.anchored.status();
    return result;
  }

  /** One hypothesis per corner, alternating the open side; the others are covered by the first test. */
  std::vector<Hypothesis> onePerCorner()
  {
    const auto& all = StartDeduction::hypotheses();
    return {all[1], all[3], all[4], all[7]};
  }

  std::string describe(const Hypothesis& truth, const Deduction& d)
  {
    constexpr const char* names[] = {"north", "east", "south", "west"};
    return "start " + std::to_string(truth.cell) + " " + names[static_cast<int>(truth.heading)] + ": known after " + std::to_string(d.cells) + " cell(s), " +
           std::to_string(d.frames) + " frame(s), deduced " + std::to_string(d.status.working.cell) + " " + names[static_cast<int>(d.status.working.heading)] +
           ", conflicts " + std::to_string(d.status.conflicts);
  }
}

GTEST_TEST(MazeStartAcceptance, ExplorationDeducesTheStartOnEveryCornerAndHeading)
{
  // Nominal noise, a tag on every face: the walls of the start cell are the first thing the robot sees.
  for(const Hypothesis& truth : StartDeduction::hypotheses())
  {
    const Deduction d = exploreUntilKnown(configFor(loadFixture(truth), everyFace()));
    note("all faces tagged, " + describe(truth, d) + ", reached the goal " + std::to_string(d.exploration.reachedGoal));
    EXPECT_TRUE(d.known) << describe(truth, d);
    EXPECT_EQ(d.status.working, truth);
    EXPECT_EQ(d.status.conflicts, 0);
    EXPECT_EQ(d.status.aliveCount, 1);
    EXPECT_TRUE(d.exploration.reachedGoal) << d.exploration.error;
    EXPECT_LE(d.cells, 1);
  }
}

GTEST_TEST(MazeStartAcceptance, TagsOnOneFaceAndNoneInTheStartCellTakeLongerButStillFindTheStart)
{
  // Tags on the north faces only, or the west faces only, and none while the robot is in the start cell: the corner has to
  // be told from tags several cells away, seen at an angle. The planner may still lead it into a wall with these layouts
  // when it is given the start (a pre-existing limit of the localizer), so only the deduction is asserted.
  int worstCells = 0;
  unsigned worstFrames = 0;
  for(const char* face : {"north", "west"})
    for(const Hypothesis& truth : std::string(face) == "north" ? std::vector<Hypothesis>(StartDeduction::hypotheses().begin(), StartDeduction::hypotheses().end()) : onePerCorner())
    {
      Config config = configFor(loadFixture(truth), layoutOf("faces", face));
      config.noTagsInStartCell = true;
      const Deduction d = exploreUntilKnown(config);
      note(std::string("tags on the ") + face + " faces only, none in the start cell, " + describe(truth, d));
      EXPECT_TRUE(d.known) << face << " " << describe(truth, d);
      EXPECT_EQ(d.status.working, truth) << face;
      EXPECT_EQ(d.status.conflicts, 0) << face;
      worstCells = std::max(worstCells, d.cells);
      worstFrames = std::max(worstFrames, d.frames);
    }
  note("worst case: " + std::to_string(worstCells) + " cells, " + std::to_string(worstFrames) + " frames");
  EXPECT_LE(worstCells, 12);
}

namespace
{
  /** No error at all, so that two runs can be compared exactly. */
  Config exact(const MazeDescription& maze, const MazeTagLayout& layout)
  {
    Config config = configFor(maze, layout);
    Noise& noise = config.noise;
    noise.odometryTranslationBias = 0.f;
    noise.odometryTranslationNoise = 0.f;
    noise.odometryRotationPerMetre = 0.f;
    noise.gyroDriftDegPerMinute = 0.f;
    noise.gyroNoiseDeg = 0.f;
    noise.tagPositionStdDev = 0.f;
    noise.tagRotationStdDev = 0.f;
    noise.wallDistanceStdDev = 0.f;
    return config;
  }

  /** Executes `route`, standing a moment first and at the end. */
  void walkRoute(WalkingRobot& run, const std::vector<Action>& route)
  {
    run.stand(1);
    for(const Action action : route)
      run.perform(action);
    run.stand(10);
  }
}

GTEST_TEST(MazeStartAcceptance, WithoutATagAtTheStartTheReanchoredMapEqualsTheMapOfTheGivenStart)
{
  for(const Hypothesis& truth : onePerCorner())
  {
    const MazeDescription maze = loadFixture(truth);
    const std::vector<Action> route = explorationRoute(maze); // In the frame of the true maze, executed as it is by both runs.
    ASSERT_FALSE(route.empty());

    Config given = exact(maze, everyFace());
    given.startGiven = true;
    given.noTagsInStartCell = true;
    WalkingRobot reference(given);
    walkRoute(reference, route);

    Config unknown = given;
    unknown.startGiven = false;
    WalkingRobot run(unknown);
    int revisionsSeen = 0;
    run.onFrame = [&](const WalkingRobot& robot) { revisionsSeen = robot.anchored.status().revision; };
    walkRoute(run, route);

    ASSERT_TRUE(run.anchored.status().known) << "start " << truth.cell;
    EXPECT_EQ(run.anchored.anchor(), truth);
    EXPECT_EQ(run.localizerStart(), truth);
    EXPECT_TRUE(run.fusion() == reference.fusion()) << "start " << truth.cell << " heading " << static_cast<int>(truth.heading);
    EXPECT_TRUE(run.fusion().graph == reference.fusion().graph);
    EXPECT_EQ(run.fusion().startCell, truth.cell);
    EXPECT_EQ(run.anchored.status().conflicts, 0);
    // The localizer followed: its pose is the true pose, with nothing but float error left.
    EXPECT_LT(run.positionError(), 1.f);
    EXPECT_LT(run.headingError(), 0.01f);
    EXPECT_TRUE(run.cellCorrect());
    int decided = 0;
    for(int cell = 0; cell < cellCount; ++cell)
      for(int h = 0; h < headingCount; ++h)
        decided += run.fusion().graph.get(cellRow(cell), cellCol(cell), static_cast<Heading>(h)) != EdgeState::unknown ? 1 : 0;
    note("no tag in the start cell, exact sensors, start " + std::to_string(truth.cell) + " heading " + std::to_string(static_cast<int>(truth.heading)) + ": working hypothesis changed " +
         std::to_string(revisionsSeen) + " time(s), " + std::to_string(decided) + " edge slots decided, the map equals the given-start map");
  }
}

GTEST_TEST(MazeStartAcceptance, AContradictingTagLeavesAGivenStartAloneAndIsCounted)
{
  // The operator says corner 63 facing north; the robot is really in corner 7 facing south.
  const Hypothesis really = {7, Heading::south}, said = {63, Heading::north};
  Config config = exact(loadFixture(really), everyFace());
  config.startGiven = false;
  WalkingRobot run(config);
  ASSERT_TRUE(run.giveStart(said.cell, said.heading));
  walkRoute(run, explorationRoute(config.maze));
  const StartStatus status = run.anchored.status();
  note("given 63 north, really 7 south: conflicts " + std::to_string(status.conflicts) + ", revision " + std::to_string(status.revision));
  EXPECT_TRUE(status.given);
  EXPECT_EQ(status.working, said);
  EXPECT_EQ(run.anchored.anchor(), said);
  EXPECT_EQ(run.localizerStart(), said);
  EXPECT_GT(status.conflicts, 0);
  EXPECT_LE(status.revision, 1); // Only the setGiven; nothing re-anchored afterwards.
  EXPECT_EQ(run.anchored.deduction().aliveCount(), 1);
}

GTEST_TEST(MazeStartAcceptance, AWrongChiralityIsPrunedByTheBoundaryAloneAndTheLocalizerFollows)
{
  // No tag anywhere, so only the walls speak. The robot starts in corner 56 facing east: the maze is on its left. The
  // working hypothesis (corner 0 facing east) puts the maze on the right, so the open side on the left of the start cell
  // would be the border: the four hypotheses with the maze on the right fall, and the working hypothesis moves on.
  const Hypothesis truth = {56, Heading::east};
  Config config = exact(loadFixture(truth), layoutOf("walls", "", ""));
  config.startGiven = false;
  WalkingRobot run(config);
  ASSERT_EQ(run.anchored.anchor(), StartDeduction::hypotheses()[0]);
  run.stand(60);

  const StartStatus status = run.anchored.status();
  note("no tags, 60 frames standing in corner 56 facing east: alive mask " + std::to_string(status.aliveMask) + ", working " + std::to_string(status.working.cell) + " " +
       std::to_string(static_cast<int>(status.working.heading)) + ", revision " + std::to_string(status.revision));
  const int truthIndex = StartDeduction::indexOf(truth.cell, truth.heading);
  EXPECT_NE(status.aliveMask & (1u << truthIndex), 0u) << "the true hypothesis must survive";
  for(const int rightChirality : {0, 2, 4, 7})
    EXPECT_EQ(status.aliveMask & (1u << rightChirality), 0u) << rightChirality;
  EXPECT_EQ(status.aliveCount, 4);
  EXPECT_FALSE(status.known) << "without tags the corner is not told";
  EXPECT_EQ(status.conflicts, 0);
  EXPECT_GE(status.revision, 1);
  EXPECT_NE(status.working, StartDeduction::hypotheses()[0]);
  EXPECT_EQ(run.localizerStart(), status.working);
  EXPECT_LT(run.positionError(), 1.f) << "the localizer moved into the new frame with the robot";
  EXPECT_LT(run.headingError(), 0.01f);
}

GTEST_TEST(MazeStartAcceptance, AfterAPickUpTheLocalizerIsPutAtTheStartOfTheWorkingHypothesis)
{
  const Hypothesis truth = {63, Heading::west};
  Config config = exact(loadFixture(truth), everyFace());
  config.startGiven = false;
  WalkingRobot run(config);
  run.stand(2); // Every face is tagged: the first frame tells the corner, the localizer follows in the next one.
  ASSERT_TRUE(run.anchored.status().known);
  ASSERT_EQ(run.localizerStart(), truth);
  run.perform(Action::forward);
  run.perform(Action::turnLeft);

  run.setPlaying(false);       // Picked up ...
  run.carryTo(truth.cell, truth.heading); // ... and put down on the start cell facing the open side ...
  run.setPlaying(true);        // ... and active again.
  EXPECT_EQ(run.resets, 2u) << "the first frame and the return to active";
  EXPECT_LT(run.positionError(), 1.f);
  EXPECT_LT(run.headingError(), 0.01f);
  EXPECT_EQ(run.anchored.map().attempt, 2);
}
