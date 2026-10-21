/**
 * @file MazeStartAnchoredMapTest.cpp
 *
 * Tests of the shared map-and-deduction glue of MazeMapper (ticket MZ-024), of the move of the localizer state into another
 * frame, and of the `start` setting of handleMaze.cfg.
 */

#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Streaming/InStreams.h"
#include "Tools/Maze/MazeBehavior.h"
#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeOracle.h"
#include "Tools/Maze/MazeStartAnchoredMap.h"
#include "Tools/Maze/MazeStartConfig.h"

#include <gtest/gtest.h>
#include <limits>

namespace
{
  using namespace Maze;

  /** A maze without inner walls, so that every tag is visible. */
  MazeGraph openMaze()
  {
    MazeGraph graph;
    for(int row = 0; row < size; ++row)
      for(int col = 0; col < size; ++col)
        for(int h = 0; h < headingCount; ++h)
          graph.set(row, col, static_cast<Heading>(h), EdgeState::open);
    return graph;
  }

  /** The tag on `face` of `cell` as the oracle shows it to a robot standing at `truth` (maze frame of the truth). */
  Pose3f tagSeenFrom(const Pose2f& truth, int cell, Heading face)
  {
    const Oracle::Camera camera{Vector2f::Zero(), 0.f, 1.2f};
    Oracle::View view;
    Pose3f tagInRobot;
    EXPECT_TRUE(Oracle::computeView(openMaze(), truth, camera, 2, view));
    EXPECT_TRUE(Oracle::observeTag(view, camera, 1250.f, Angle::fromDegrees(80.f), cell, face, 300.f, tagInRobot));
    return tagInRobot;
  }

  /** The truth start moved into the frame of the working hypothesis: where the localizer believes the robot is. */
  Pose2f believed(const Pose2f& truth, const Hypothesis& truthStart, const Hypothesis& working)
  {
    return StartDeduction::startToMaze(working) * StartDeduction::startToMaze(truthStart).inverse() * truth;
  }

  Pose2f startPose(const Hypothesis& start)
  {
    return StartDeduction::startToMaze(start);
  }

  /** The look-ahead of the start cell of a corner: walls left and right, nothing known in front. */
  std::vector<CellObservation> startCellWalls()
  {
    return {{0, EdgeState::wall, EdgeState::wall, EdgeState::open, 100.f}};
  }
}

GTEST_TEST(MazeStartAnchoredMap, AGivenStartRebuildsTheMapInItsFrame)
{
  StartAnchoredMap map;
  EXPECT_EQ(map.anchor(), StartDeduction::hypotheses()[0]);
  map.newAttempt();
  EXPECT_FALSE(map.setGiven(0, Heading::north)); // Faces the border.
  EXPECT_EQ(map.anchor(), StartDeduction::hypotheses()[0]);
  EXPECT_FALSE(map.status().given);

  ASSERT_TRUE(map.setGiven(56, Heading::north));
  const StartStatus status = map.status();
  EXPECT_TRUE(status.given);
  EXPECT_TRUE(status.known);
  EXPECT_EQ(status.working, (Hypothesis{56, Heading::north}));
  EXPECT_EQ(map.anchor(), status.working);
  EXPECT_EQ(map.map().startCell, 56);
  EXPECT_EQ(map.map().attempt, 2) << "the attempt counter survives the rebuild";
  EXPECT_FALSE(map.deduction().logging()) << "a given start never logs";
}

GTEST_TEST(MazeStartAnchoredMap, ATagMovesTheMapIntoTheFrameOfTheDeducedStartAndTheLogStops)
{
  const Hypothesis truthStart = {63, Heading::north};
  const Pose2f truth = startPose(truthStart);
  StartAnchoredMap map; // The working hypothesis is (0, east), which is wrong.
  map.newAttempt();
  ASSERT_EQ(map.anchor(), StartDeduction::hypotheses()[0]);

  // The localizer believes the robot is at the start of the working hypothesis. It sees the walls of its cell
  // three times, then the tag of the cell ahead.
  const Pose2f estimate = believed(truth, truthStart, map.anchor());
  for(unsigned i = 0; i < 3; ++i)
  {
    map.markVisited(estimate);
    EXPECT_TRUE(map.lookAheadUsable(estimate));
    // Left and right of the gaze, which is the heading of the estimate.
    map.observeCellsAhead(estimate, headingOf(estimate.rotation), startCellWalls(), 1000 + 300 * i);
    EXPECT_FALSE(map.endFrame()) << "walls alone do not tell the corner";
  }
  EXPECT_EQ(map.status().aliveCount, 8);
  EXPECT_TRUE(map.deduction().logging());
  EXPECT_EQ(map.map().graph.get(0, 0, Heading::north), EdgeState::wall) << "in the frame of the working hypothesis, (0, east) faces east";
  EXPECT_EQ(map.map().graph.get(0, 0, Heading::south), EdgeState::wall);

  const Pose3f tag = tagSeenFrom(truth, 55, Heading::north);
  EXPECT_EQ(map.observeTag(estimate, 55, tag, 2000), 0) << "the tag of cell 55 is not on any face of the working hypothesis' cell ahead";
  EXPECT_TRUE(map.endFrame());
  EXPECT_EQ(map.anchor(), truthStart);
  EXPECT_TRUE(map.status().known);
  EXPECT_EQ(map.status().revision, 1);
  EXPECT_FALSE(map.deduction().logging()) << "once the start is known nothing is logged";
  EXPECT_EQ(map.map().startCell, 63);
  EXPECT_EQ(map.map().attempt, 2);
  // The walls are now on the edges of cell 63: left of north is west, right is east (the border).
  EXPECT_EQ(map.map().graph.get(7, 7, Heading::west), EdgeState::wall);
  EXPECT_EQ(map.map().graph.get(7, 7, Heading::north), EdgeState::open);
  EXPECT_EQ(map.map().graph.get(0, 0, Heading::south), EdgeState::unknown) << "nothing is left of the wrong frame";
  EXPECT_TRUE(map.map().visited(7, 7));
  EXPECT_FALSE(map.endFrame()) << "one rebuild per change";
}

GTEST_TEST(MazeStartAnchoredMap, ABlockedEdgeIsAWallAndSurvivesTheRebuildUnderAnotherHypothesis)
{
  const Hypothesis truthStart = {63, Heading::north};
  const Pose2f truth = startPose(truthStart);
  StartAnchoredMap map; // The working hypothesis is (0, east), which is wrong.
  map.newAttempt();
  const Pose2f estimate = believed(truth, truthStart, map.anchor());
  map.markVisited(estimate);
  map.endFrame();

  // The robot is blocked by the edge in front of the start cell, in the frame of the working hypothesis.
  EXPECT_TRUE(map.observeBlocked(0, 0, Heading::east));
  EXPECT_EQ(map.map().graph.get(0, 0, Heading::east), EdgeState::wall);
  EXPECT_FALSE(map.observeBlocked(0, 0, Heading::east));
  EXPECT_FALSE(map.observeBlocked(-1, 0, Heading::east)) << "outside the maze";
  EXPECT_EQ(map.status().aliveCount, 8) << "a wall removes no hypothesis";

  // A tag shows the true start: the map is rebuilt and the blocked edge is in front of cell 63, facing north.
  EXPECT_EQ(map.observeTag(estimate, 55, tagSeenFrom(truth, 55, Heading::north), 2000), 0);
  ASSERT_TRUE(map.endFrame());
  ASSERT_EQ(map.anchor(), truthStart);
  EXPECT_EQ(map.map().graph.get(7, 7, Heading::north), EdgeState::wall);
  EXPECT_EQ(map.map().graph.get(0, 0, Heading::east), EdgeState::unknown) << "nothing is left of the wrong frame";

  // The wall percept sees through it, many times: it stays a wall.
  CellObservation ahead;
  ahead.front = EdgeState::open;
  ahead.distance = 100.f;
  const Pose2f startNow = startPose(truthStart);
  ASSERT_TRUE(map.lookAheadUsable(startNow));
  for(unsigned i = 0; i < 30; ++i)
    map.observeCellsAhead(startNow, Heading::north, {ahead}, 3000 + 300 * i);
  EXPECT_EQ(map.map().graph.get(7, 7, Heading::north), EdgeState::wall);
}

GTEST_TEST(MazeStartAnchoredMap, ABlockedEdgeOfAGivenStartIsKeptAndNeverOpened)
{
  StartAnchoredMap map;
  ASSERT_TRUE(map.setGiven(56, Heading::north));
  EXPECT_TRUE(map.observeBlocked(6, 0, Heading::north));
  CellObservation ahead;
  ahead.front = EdgeState::open;
  ahead.distance = 100.f;
  const Pose2f start = startPose(map.anchor());
  const Pose2f here(start.rotation, start * Vector2f(500.f, 0.f)); // The cell ahead of the start, (6, 0).
  ASSERT_TRUE(map.lookAheadUsable(here));
  for(unsigned i = 0; i < 30; ++i)
    map.observeCellsAhead(here, Heading::north, {ahead}, 1000 + 300 * i);
  EXPECT_EQ(map.map().graph.get(6, 0, Heading::north), EdgeState::wall);
}

GTEST_TEST(MazeStartAnchoredMap, ATagThatTheLayoutCannotExplainChangesNothingButIsCounted)
{
  // Only the east faces carry tags, so a tag on a south face fits no hypothesis: the map ignores it and the deduction counts a conflict.
  const Hypothesis truthStart = {7, Heading::south};
  MazeTagLayout layout;
  layout.mode = TagLayoutMode::faces;
  layout.faces = {TagFace::east};
  StartAnchoredMap map;
  map.layout = layout;
  const Pose2f truth = startPose(truthStart);
  const Pose3f tag = tagSeenFrom(truth, 15, Heading::south);
  EXPECT_EQ(map.observeTag(believed(truth, truthStart, map.anchor()), 15, tag, 1000), 0);
  EXPECT_EQ(map.status().aliveCount, 8);
  EXPECT_EQ(map.status().conflicts, 1);
  EXPECT_FALSE(map.endFrame());
}

GTEST_TEST(MazeStartAnchoredMap, ConflictsAreReportedAtMostOncePerInterval)
{
  StartAnchoredMap map;
  ASSERT_TRUE(map.setGiven(63, Heading::north));
  const Pose2f truth = startPose({7, Heading::south}); // The robot is really somewhere else: its tag contradicts the given start.
  const Pose2f estimate = believed(truth, {7, Heading::south}, map.anchor());
  EXPECT_FALSE(map.takeConflictWarning(0));

  const Pose3f tag = tagSeenFrom(truth, 15, Heading::south);
  map.observeTag(estimate, 15, tag, 1000);
  EXPECT_GT(map.status().conflicts, 0);
  EXPECT_EQ(map.anchor(), (Hypothesis{63, Heading::north})) << "the given start does not change";
  EXPECT_TRUE(map.takeConflictWarning(1000));
  EXPECT_FALSE(map.takeConflictWarning(1000)) << "nothing new";

  map.observeTag(estimate, 15, tag, 1500);
  EXPECT_FALSE(map.takeConflictWarning(1500)) << "too soon";
  map.observeTag(estimate, 15, tag, 3500);
  EXPECT_TRUE(map.takeConflictWarning(3500));
  EXPECT_FALSE(map.takeConflictWarning(3600));
  EXPECT_TRUE(map.status().given);
}

GTEST_TEST(MazeStartAnchoredMap, ALogThatNeverResolvesStopsGrowingAndWarnsOnce)
{
  StartAnchoredMap map;
  const Pose2f pose = startPose(StartDeduction::hypotheses()[0]);
  // Wall evidence never prunes, so the start stays unknown however long the robot looks at walls.
  for(unsigned i = 0; i < StartDeduction::maxLogEntries / 3 + 10; ++i)
    map.observeCellsAhead(pose, headingOf(pose.rotation), startCellWalls(), i * 300);
  EXPECT_FALSE(map.status().known);
  EXPECT_TRUE(map.status().logOverflowed);
  EXPECT_TRUE(map.takeLogOverflowWarning());
  EXPECT_FALSE(map.takeLogOverflowWarning());
  EXPECT_TRUE(map.deduction().logging()) << "the log keeps its entries; only new ones are dropped";
}

GTEST_TEST(MazeStartAnchoredMap, PositionEvidenceNeedsThePoseNearTheCentreOfACell)
{
  const Hypothesis working = StartDeduction::hypotheses()[0]; // Cell 0 facing east.
  const Pose2f start = startPose(working);
  // One cell to the left of the start is north of the maze under the working hypothesis, and so outside it.
  const Pose2f nearCentre(start.rotation, start * Vector2f(0.f, 500.f - 100.f));
  const Pose2f nearBoundary(start.rotation, start * Vector2f(0.f, 500.f - 200.f));

  StartAnchoredMap boundary;
  EXPECT_FALSE(boundary.lookAheadUsable(nearBoundary));
  boundary.observePosition(nearBoundary);
  EXPECT_EQ(boundary.status().aliveCount, 8) << "a pose 200 mm from the centre of the cell does not count";

  StartAnchoredMap centre;
  EXPECT_TRUE(centre.lookAheadUsable(nearCentre));
  centre.observePosition(nearCentre);
  EXPECT_EQ(centre.status().aliveCount, 4);
  EXPECT_TRUE(centre.endFrame()) << "the working hypothesis was one of those removed";
  EXPECT_NE(centre.anchor(), working);
  EXPECT_EQ(centre.status().aliveMask, 0b01101010u);
}

GTEST_TEST(MazeStartAnchoredMap, InvalidPosesAndTagsAreIgnored)
{
  StartAnchoredMap map;
  const Pose2f nan(std::numeric_limits<float>::quiet_NaN(), Vector2f(1.f, 1.f));
  EXPECT_FALSE(map.lookAheadUsable(Pose2f(0.f, Vector2f(std::numeric_limits<float>::infinity(), 0.f))));
  EXPECT_EQ(map.observeTag(nan, 5, Pose3f(), 1000), 0);
  EXPECT_EQ(map.observeTag(Pose2f(), 64, Pose3f(), 1000), 0);
  EXPECT_EQ(map.status().aliveCount, 8);
  EXPECT_EQ(map.status().conflicts, 0);
}

GTEST_TEST(MazeStartConfig, OnlyACornerFacingIntoTheMazeIsAGivenStart)
{
  static const bool registered = (FunctionList::execute(), true); // The error text names the enum value.
  (void) registered;
  std::string error;
  StartConfig config;
  EXPECT_FALSE(resolveStart(config, error).given) << "the default deduces";
  EXPECT_TRUE(error.empty());

  config.cell = 56;
  config.heading = TagFace::east;
  StartChoice choice = resolveStart(config, error);
  EXPECT_TRUE(choice.given);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(choice.cell, 56);
  EXPECT_EQ(choice.heading, Heading::east);

  for(const int cell : {1, 8, 27, 64, -2})
  {
    config.cell = cell;
    choice = resolveStart(config, error);
    EXPECT_FALSE(choice.given) << cell;
    EXPECT_FALSE(error.empty()) << cell;
    EXPECT_EQ(choice.cell, -1);
  }
  config.cell = 56;
  config.heading = TagFace::south; // Faces the border.
  choice = resolveStart(config, error);
  EXPECT_FALSE(choice.given);
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("south"), std::string::npos);

  config.cell = -1; // The heading is ignored when the start is deduced.
  config.heading = TagFace::west;
  EXPECT_FALSE(resolveStart(config, error).given);
  EXPECT_TRUE(error.empty());
}

GTEST_TEST(MazeStartConfig, TheSharedFileIsReadByBothReadersWithoutComplaints)
{
  static const bool registered = (FunctionList::execute(), true);
  (void) registered;
  {
    // Control: a nested entry that is not read is reported, so the silence below means something.
    const std::string text = "start = {cell = 7; heading = east; extra = 1;};";
    testing::internal::CaptureStderr();
    testing::internal::CaptureStdout();
    StartFile file;
    InMapMemory stream(text.c_str(), text.size());
    stream >> file;
    const std::string output = testing::internal::GetCapturedStdout() + testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("unused attribute extra"), std::string::npos) << output;
  }
  for(const char* scenario : {"Maze", "MazeSafe"})
  {
    const std::string path = std::string(File::getBHDir()) + "/Config/Scenarios/" + scenario + "/handleMaze.cfg";
    testing::internal::CaptureStderr();
    testing::internal::CaptureStdout();
    StartFile start;
    EXPECT_TRUE(StartFile::load(path, start)) << path;
    AnnounceFile announce;
    EXPECT_TRUE(AnnounceFile::load(path, announce)) << path;
    const std::string output = testing::internal::GetCapturedStdout() + testing::internal::GetCapturedStderr();
    EXPECT_EQ(output.find("rror"), std::string::npos) << path << ": " << output;
    EXPECT_EQ(start.start.cell, -1);
    EXPECT_TRUE(announce.announce.enabled);
    std::string error;
    EXPECT_FALSE(resolveStart(start.start, error).given);
    EXPECT_TRUE(error.empty()) << error;
  }
  StartFile missing;
  EXPECT_FALSE(StartFile::load(std::string(File::getBHDir()) + "/Config/Scenarios/Maze/doesNotExist.cfg", missing));
  EXPECT_EQ(missing.start.cell, -1);
}
