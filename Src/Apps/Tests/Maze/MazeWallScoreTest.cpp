#include "Tools/Maze/MazeWallScore.h"
#include "Tools/Maze/MazeOracle.h"

#include <gtest/gtest.h>
#include <vector>

namespace
{
  using namespace Maze::WallScore;
  namespace Oracle = Maze::Oracle;

  /** An open maze with a wall to the north and south of row 4 in the columns 1 to 3 and a dead end east of (4,3). */
  Maze::MazeGraph corridor()
  {
    Maze::MazeGraph maze;
    for(int row = 0; row < Maze::size; ++row)
      for(int col = 0; col < Maze::size; ++col)
        for(int h = 0; h < Maze::headingCount; ++h)
          maze.set(row, col, static_cast<Maze::Heading>(h), Maze::EdgeState::open);
    for(int col = 1; col <= 3; ++col)
    {
      maze.set(4, col, Maze::Heading::north, Maze::EdgeState::wall);
      maze.set(4, col, Maze::Heading::south, Maze::EdgeState::wall);
    }
    maze.set(4, 3, Maze::Heading::east, Maze::EdgeState::wall);
    return maze;
  }

  /** The perfect percept of a robot at `pose` looking east: the oracle's view as a frame. */
  Frame perfectFrame(const Maze::MazeGraph& maze, const Pose2f& pose, int range)
  {
    Oracle::View view;
    EXPECT_TRUE(Oracle::computeView(maze, pose, {Vector2f::Zero(), 0.f, 1.f}, range, view));
    Frame frame;
    frame.truePose = pose;
    frame.believedPosition = pose.translation;
    frame.believedRotation = pose.rotation;
    frame.range = range;
    for(const Oracle::LookAheadCell& cell : view.cells)
    {
      Maze::WallView::Cell entry;
      entry.ahead = cell.ahead;
      entry.left = cell.left;
      entry.right = cell.right;
      entry.front = cell.front;
      frame.cellsAhead.push_back(entry);
    }
    return frame;
  }
}

GTEST_TEST(MazeWallScore, PerfectPerceptsScoreEverythingCorrect)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  scorer.add(perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 1)), 2));
  const Summary summary = scorer.summary();
  EXPECT_EQ(summary.frames, 1);
  EXPECT_EQ(summary.walkingFrames, 1);
  EXPECT_EQ(summary.standingFrames, 0);
  EXPECT_EQ(summary.wallWrong + summary.openWrong, 0);
  EXPECT_EQ(summary.edgeAccuracy(), 1.f);
  EXPECT_EQ(summary.edgesUnknown, 0);
  EXPECT_EQ(summary.cellsBeyondRange, 0);
  // Visit: 3 of the 4 sides are known (the back is never reported), all right.
  EXPECT_EQ(summary.visits, 1);
  EXPECT_EQ(summary.visitSides, 4);
  EXPECT_EQ(summary.visitSidesKnown, 3);
  EXPECT_EQ(summary.visitSidesCorrect, 3);
  EXPECT_FLOAT_EQ(summary.visitCoverage(), 0.75f);
  EXPECT_FLOAT_EQ(summary.visitAccuracy(), 1.f);
  // The next cell ahead: both its sides are walls and known.
  EXPECT_EQ(summary.aheadSides, 2);
  EXPECT_EQ(summary.aheadSidesKnown, 2);
  EXPECT_EQ(summary.aheadSidesCorrect, 2);
}

GTEST_TEST(MazeWallScore, WrongEdgesAreCountedByKind)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  Frame frame = perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 1)), 2);
  frame.cellsAhead[0].left = Maze::EdgeState::open;     // a wall seen as open
  frame.cellsAhead[0].front = Maze::EdgeState::wall;    // an open edge seen as a wall
  frame.cellsAhead[1].right = Maze::EdgeState::unknown; // not known
  scorer.add(frame);
  const Summary summary = scorer.summary();
  EXPECT_EQ(summary.wallWrong, 1);
  EXPECT_EQ(summary.openWrong, 1);
  EXPECT_EQ(summary.edgesUnknown, 1);
  EXPECT_EQ(summary.aheadSidesKnown, 1);
  EXPECT_EQ(summary.aheadSidesCorrect, 1);
  EXPECT_EQ(summary.visitSidesKnown, 3);
  EXPECT_EQ(summary.visitSidesCorrect, 1); // only the right side of the cell is right

  // The finished visit names its wrong sides.
  scorer.finishVisit();
  const std::string text = scorer.text();
  EXPECT_NE(text.find("(a) wrong cell 4,1 north: votes wall 0 open 1, truth wall"), std::string::npos) << text;
  EXPECT_NE(text.find("(a) wrong cell 4,1 east: votes wall 1 open 0, truth open"), std::string::npos) << text;
}

GTEST_TEST(MazeWallScore, CellMismatchIsNotAPerceptionError)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  Frame frame = perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 1)), 2);
  frame.believedPosition = Oracle::cellCentreOf(4, 2); // the perceptor believes it is a cell ahead
  frame.cellsAhead[0].left = Maze::EdgeState::open;
  scorer.add(frame);
  const Summary summary = scorer.summary();
  EXPECT_EQ(summary.poseMismatchFrames, 1);
  EXPECT_EQ(summary.frames, 0);
  EXPECT_EQ(summary.wallWrong, 0);
  EXPECT_EQ(summary.visits, 0);

  // The believed heading counts as well: a perceptor that believes it looks north while it looks east.
  frame = perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 1)), 2);
  frame.believedRotation = pi / 2.f;
  scorer.add(frame);
  EXPECT_EQ(scorer.summary().poseMismatchFrames, 2);
  frame.believedRotation = 0.2f; // a small error is fine
  scorer.add(frame);
  EXPECT_EQ(scorer.summary().poseMismatchFrames, 2);
  EXPECT_EQ(scorer.summary().frames, 1);

  // A true pose outside the maze is skipped.
  frame.truePose = Pose2f(0.f, Vector2f(-50.f, 100.f));
  scorer.add(frame);
  EXPECT_EQ(scorer.summary().skippedFrames, 1);
}

GTEST_TEST(MazeWallScore, CellsBeyondTheActiveRangeAreCounted)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  Frame frame = perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(2, 0)), 3); // an open row: 4 cells reported
  frame.range = 2;                                                              // but the active range is 2
  scorer.add(frame);
  EXPECT_EQ(scorer.summary().cellsBeyondRange, 1);
}

GTEST_TEST(MazeWallScore, VisitsAccumulateTheMajorityOverTheVisit)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  const Pose2f pose(0.f, Oracle::cellCentreOf(4, 1));
  // Three frames in the cell: the left wall is seen as open once and as a wall twice.
  for(int i = 0; i < 3; ++i)
  {
    Frame frame = perfectFrame(maze, pose, 0);
    if(i == 0)
      frame.cellsAhead[0].left = Maze::EdgeState::open;
    scorer.add(frame);
  }
  // The robot moves to the next cell: the first visit ends.
  scorer.add(perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 2)), 0));
  const Summary summary = scorer.summary();
  EXPECT_EQ(summary.visits, 2);
  // 3 sides known in the first cell; in the second one also the west side, which the first cell saw as its front edge.
  EXPECT_EQ(summary.visitSidesKnown, 7);
  EXPECT_EQ(summary.visitSidesCorrect, 7);
  EXPECT_EQ(summary.frames, 4);
}

GTEST_TEST(MazeWallScore, BoundaryPointErrorsAreBinnedByDistance)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  Frame frame = perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 1)), 2);
  // Robot at x = 750; the dead-end wall is at x = 2000, 1250 mm ahead. Two points 30 mm and 50 mm in front of it (distance
  // about 1.2 m: the bin 1.0 m), one on it, 20 mm to the side (1.25 m: the bin 1.5 m) and one at 3 m.
  frame.boundaryPoints = {Vector2f(1250.f - 30.f, 10.f), Vector2f(1250.f, -20.f), Vector2f(1250.f - 50.f, 0.f), Vector2f(3000.f, 0.f)};
  frame.standing = true;
  scorer.add(frame);
  frame.standing = false;
  scorer.add(frame);
  const Summary summary = scorer.summary();
  const ErrorStats& bin = summary.standingErrors[1]; // 1.0 m +- 250 mm
  EXPECT_EQ(bin.count, 2);
  EXPECT_NEAR(bin.mean, 40.f, 0.1f);
  EXPECT_NEAR(bin.standardDeviation, 10.f, 0.1f);
  EXPECT_NEAR(bin.percentile95, 50.f, 0.1f);
  EXPECT_EQ(summary.standingErrors[2].count, 1); // the point on the wall, 1.25 m away
  EXPECT_NEAR(summary.standingErrors[2].mean, 0.f, 0.1f);
  EXPECT_EQ(summary.walkingErrors[1].count, 2);
  EXPECT_EQ(summary.standingErrors[0].count, 0);
  EXPECT_EQ(summary.standingErrors[4].count, 0); // the point at 3 m is outside every bin
}

GTEST_TEST(MazeWallScore, SummaryDoesNotEndTheVisit)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  const Pose2f pose(0.f, Oracle::cellCentreOf(4, 1));
  scorer.add(perfectFrame(maze, pose, 1));
  EXPECT_EQ(scorer.summary().visits, 1);
  scorer.add(perfectFrame(maze, pose, 1)); // the same visit goes on
  EXPECT_EQ(scorer.summary().visits, 1);
  EXPECT_EQ(scorer.summary().visitSidesKnown, 3);
  scorer.finishVisit();
  scorer.add(perfectFrame(maze, pose, 1)); // a new one
  EXPECT_EQ(scorer.summary().visits, 2);
}

GTEST_TEST(MazeWallScore, TextNamesEveryMetric)
{
  const Maze::MazeGraph maze = corridor();
  Scorer scorer(maze);
  scorer.add(perfectFrame(maze, Pose2f(0.f, Oracle::cellCentreOf(4, 1)), 2));
  const std::string text = scorer.text();
  for(const char* key : {"(a)", "(b)", "(c)", "(d) 0.5 m", "(d) 2.5 m", "accuracy"})
    EXPECT_NE(text.find(key), std::string::npos) << key;
}
