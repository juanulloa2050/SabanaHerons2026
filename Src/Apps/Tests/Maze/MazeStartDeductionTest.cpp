/**
 * @file MazeStartDeductionTest.cpp
 *
 * Tests of the start-cell deduction (ticket MZ-024): the 8 hypotheses and their transforms, the pruning by tags,
 * boundary evidence and position, the given start, and the replay of the observation log under a hypothesis.
 */

#include "Platform/File.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeOracle.h"
#include "Tools/Maze/MazeStartDeduction.h"

#include <gtest/gtest.h>
#include <limits>

namespace
{
  using namespace Maze;

  constexpr float tolerance = 1e-3f;

  const std::array<int, StartDeduction::hypothesisCount> forwardCells = {1, 8, 15, 6, 48, 57, 55, 62};

  /** A tag as the robot sees it from the start: position and normal (out of the wall, into the cell) in the start-relative frame. */
  struct SeenTag
  {
    Vector2f position;
    Vector2f normal;
  };

  SeenTag seen(const Hypothesis& start, int id, Heading face)
  {
    const Vector2f inMaze = Oracle::cellCentreOf(cellRow(id), cellCol(id)) + Oracle::directionOf(face) * Oracle::halfCellMm;
    return {StartDeduction::startToMaze(start).inverse() * inMaze, Oracle::rotated(Oracle::directionOf(turned(face, 2)), -angleOf(start.heading))};
  }

  /** The tag on the far face of the cell in front of the robot, seen square-on from the start cell. */
  int observeForwardTag(StartDeduction& deduction, const Hypothesis& start, const MazeTagLayout& layout)
  {
    const GridCell ahead = StartDeduction::toMaze(start, {-1, 0});
    const int id = cellId(ahead.row, ahead.col);
    const SeenTag tag = seen(start, id, start.heading);
    return deduction.observeTag(id, tag.position, tag.normal, 0, layout);
  }

  std::vector<int> aliveIndices(const StartDeduction& deduction)
  {
    std::vector<int> result;
    for(int i = 0; i < StartDeduction::hypothesisCount; ++i)
      if(deduction.alive(i))
        result.push_back(i);
    return result;
  }
}

GTEST_TEST(MazeStartDeduction, EightHypothesesInFixedOrder)
{
  const auto& all = StartDeduction::hypotheses();
  const std::array<Hypothesis, 8> expected = {{{0, Heading::east}, {0, Heading::south}, {7, Heading::south}, {7, Heading::west},
                                              {56, Heading::north}, {56, Heading::east}, {63, Heading::north}, {63, Heading::west}}};
  EXPECT_EQ(all, expected);
  for(int i = 0; i < StartDeduction::hypothesisCount; ++i)
    EXPECT_EQ(StartDeduction::indexOf(all[static_cast<std::size_t>(i)].cell, all[static_cast<std::size_t>(i)].heading), i);
  EXPECT_EQ(StartDeduction::indexOf(0, Heading::north), -1); // Faces the border.
  EXPECT_EQ(StartDeduction::indexOf(9, Heading::east), -1);  // Not a corner.
  StartDeduction deduction;
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.working(), all[0]);
  EXPECT_FALSE(deduction.known());
}

GTEST_TEST(MazeStartDeduction, StartCellValidation)
{
  for(const int valid : {-1, 0, 7, 56, 63})
    EXPECT_TRUE(StartDeduction::isValidStartCell(valid)) << valid;
  for(const int invalid : {-2, 1, 8, 27, 55, 64, 100})
    EXPECT_FALSE(StartDeduction::isValidStartCell(invalid)) << invalid;
}

GTEST_TEST(MazeStartDeduction, GridTransformLandsOnTheRightCellAndHeading)
{
  for(int i = 0; i < StartDeduction::hypothesisCount; ++i)
  {
    const Hypothesis& h = StartDeduction::hypotheses()[static_cast<std::size_t>(i)];
    EXPECT_EQ(StartDeduction::toMaze(h, {0, 0}), (GridCell{cellRow(h.cell), cellCol(h.cell)}));
    const GridCell ahead = StartDeduction::toMaze(h, {-1, 0}); // Rel north is forward.
    EXPECT_EQ(cellId(ahead.row, ahead.col), forwardCells[static_cast<std::size_t>(i)]) << i;
    EXPECT_EQ(StartDeduction::toMaze(h, Heading::north), h.heading);
    const GridCell behind = StartDeduction::toMaze(h, {1, 0});
    EXPECT_FALSE(inside(behind.row, behind.col)) << "the robot starts with the border behind it " << i; // Representable, not clamped.
  }
  // Corner 0 facing east and facing south, as in the ticket.
  EXPECT_EQ(StartDeduction::toMaze(StartDeduction::hypotheses()[0], {0, 1}), (GridCell{1, 0})); // Right of east is south.
  EXPECT_EQ(StartDeduction::toMaze(StartDeduction::hypotheses()[1], {0, 1}), (GridCell{0, -1})); // Right of south is west.
}

GTEST_TEST(MazeStartDeduction, GridTransformRoundTrips)
{
  for(const Hypothesis& h : StartDeduction::hypotheses())
  {
    for(int row = -9; row <= 9; ++row)
      for(int col = -9; col <= 9; ++col)
        EXPECT_EQ(StartDeduction::toStart(h, StartDeduction::toMaze(h, {row, col})), (GridCell{row, col}));
    for(int q = 0; q < headingCount; ++q)
      EXPECT_EQ(StartDeduction::toStart(h, StartDeduction::toMaze(h, static_cast<Heading>(q))), static_cast<Heading>(q));
  }
}

GTEST_TEST(MazeStartDeduction, ContinuousTransformRoundTripsAndAgreesWithTheGrid)
{
  for(const Hypothesis& h : StartDeduction::hypotheses())
  {
    const Pose2f toMaze = StartDeduction::startToMaze(h);
    const Vector2f centre = Oracle::cellCentreOf(cellRow(h.cell), cellCol(h.cell));
    EXPECT_NEAR((toMaze.translation - centre).norm(), 0.f, tolerance);
    EXPECT_NEAR(Angle::normalize(toMaze.rotation - angleOf(h.heading)), 0.f, tolerance);

    for(const Vector2f& point : {Vector2f(0.f, 0.f), Vector2f(500.f, 0.f), Vector2f(1020.f, -480.f), Vector2f(-500.f, 0.f), Vector2f(1500.f, 520.f), Vector2f(-260.f, 740.f)})
    {
      const Vector2f back = toMaze.inverse() * (toMaze * point);
      EXPECT_NEAR((back - point).norm(), 0.f, tolerance);
      // The continuous and the grid transform name the same cell, also outside the maze.
      const Vector2f inMaze = toMaze * point;
      const GridCell viaGrid = StartDeduction::toMaze(h, StartDeduction::cellOf(point));
      int row = 0, col = 0;
      if(Oracle::cellContaining(inMaze, row, col))
        EXPECT_EQ(viaGrid, (GridCell{row, col}));
      else
        EXPECT_FALSE(inside(viaGrid.row, viaGrid.col));
    }
  }
  // Forward is the start heading and left is its counterclockwise neighbour.
  const Pose2f south = StartDeduction::startToMaze({0, Heading::south});
  EXPECT_NEAR((south * Vector2f(500.f, 0.f) - Oracle::cellCentreOf(1, 0)).norm(), 0.f, tolerance);
  EXPECT_NEAR((south * Vector2f(0.f, 500.f) - Oracle::cellCentreOf(0, 1)).norm(), 0.f, tolerance); // Left of south is east.
}

GTEST_TEST(MazeStartDeduction, TagOnTheNeighbourOfEachCornerLeavesTheTrueHypothesis)
{
  const MazeTagLayout layout; // A tag on every face.
  for(int i = 0; i < StartDeduction::hypothesisCount; ++i)
  {
    const Hypothesis& truth = StartDeduction::hypotheses()[static_cast<std::size_t>(i)];
    StartDeduction deduction;
    EXPECT_EQ(observeForwardTag(deduction, truth, layout), 7) << i;
    EXPECT_TRUE(deduction.known()) << i;
    EXPECT_EQ(aliveIndices(deduction), std::vector<int>{i});
    EXPECT_EQ(deduction.working(), truth);
    EXPECT_EQ(deduction.conflicts, 0);
  }
}

GTEST_TEST(MazeStartDeduction, TagPruningRespectsTheLayoutAndTheFace)
{
  const Hypothesis& truth = StartDeduction::hypotheses()[0]; // Corner 0 facing east: cell 1, east face.
  MazeTagLayout northOnly;
  northOnly.mode = TagLayoutMode::faces;
  northOnly.faces = {TagFace::north};
  StartDeduction deduction;
  EXPECT_EQ(observeForwardTag(deduction, truth, northOnly), 0); // No hypothesis explains an east tag: a conflict, nothing removed.
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.conflicts, 1);

  // A normal that fits no face of cell 1 at that position explains the tag under no hypothesis.
  const SeenTag tilted = seen(truth, 1, Heading::east);
  StartDeduction wrongNormal;
  EXPECT_EQ(wrongNormal.observeTag(1, tilted.position, Vector2f(0.f, 1.f), 0, MazeTagLayout()), 0);
  EXPECT_EQ(wrongNormal.conflicts, 1);

  StartDeduction nonFinite;
  EXPECT_EQ(nonFinite.observeTag(1, Vector2f(NAN, 0.f), tilted.normal, 0, MazeTagLayout()), 0);
  EXPECT_EQ(nonFinite.observeTag(64, tilted.position, tilted.normal, 0, MazeTagLayout()), 0);
  EXPECT_EQ(nonFinite.conflicts, 0);
}

GTEST_TEST(MazeStartDeduction, OpenEvidenceOnABoundaryFlipsTheChiralityAfterTheThreshold)
{
  StartDeduction deduction;
  // The edge on the right of the start cell is the border under (0, south), (7, west), (56, east) and (63, north).
  EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, 1.f), 0);
  EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, 1.f), 0);
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, 1.f), 4);
  EXPECT_EQ(aliveIndices(deduction), (std::vector<int>{0, 2, 4, 7}));
  EXPECT_EQ(deduction.revision, 0); // The working hypothesis survived.

  // The other chirality: open to the left removes (0, east), which was the working hypothesis.
  StartDeduction left;
  for(int i = 0; i < 3; ++i)
    left.observeEdge({0, 0}, Heading::west, false, 1.f);
  EXPECT_EQ(aliveIndices(left), (std::vector<int>{1, 3, 5, 6}));
  EXPECT_EQ(left.working(), StartDeduction::hypotheses()[1]);
  EXPECT_EQ(left.revision, 1);
}

GTEST_TEST(MazeStartDeduction, WallEvidenceNeverPrunesAndTakesBackOpenEvidence)
{
  StartDeduction deduction;
  for(int i = 0; i < 20; ++i)
    EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, true, 1.f), 0);
  EXPECT_EQ(deduction.aliveCount(), 8);
  // 2 open, 1 wall, 1 open is 2 net: below the threshold.
  StartDeduction mixed;
  mixed.observeEdge({0, 1}, Heading::east, false, 1.f);
  mixed.observeEdge({0, 1}, Heading::east, false, 1.f);
  mixed.observeEdge({0, 1}, Heading::east, true, 1.f);
  mixed.observeEdge({0, 1}, Heading::east, false, 1.f);
  EXPECT_EQ(mixed.aliveCount(), 8);
}

GTEST_TEST(MazeStartDeduction, EdgeEvidenceIsRateLimitedAndSharedByBothSides)
{
  StartDeduction deduction;
  // The edge right of cell (0, 0) is the edge left of cell (0, 1) in the grid.
  for(unsigned time = 0; time < 100; time += 10)
    deduction.observeEdge({0, 0}, Heading::east, false, 1.f, time); // One accepted observation in 250 ms.
  EXPECT_EQ(deduction.aliveCount(), 8);
  deduction.observeEdge({0, 1}, Heading::west, false, 1.f, 300);
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, 1.f, 600), 4); // The third accepted one.
}

GTEST_TEST(MazeStartDeduction, EdgeOutsideTheGridUnderAHypothesisPrunesIt)
{
  // Between two cells behind the start: outside the maze under every hypothesis, so open evidence there contradicts all of them.
  StartDeduction deduction;
  for(int i = 0; i < 3; ++i)
    deduction.observeEdge({2, 0}, Heading::north, false, 1.f);
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.conflicts, 1);
  // Between two cells ahead: inside the maze under every hypothesis, so it is no evidence.
  StartDeduction inner;
  for(int i = 0; i < 3; ++i)
    inner.observeEdge({-3, 0}, Heading::north, false, 1.f);
  EXPECT_EQ(inner.aliveCount(), 8);
  EXPECT_EQ(inner.conflicts, 0);
}

GTEST_TEST(MazeStartDeduction, PositionOutsideTheGridPrunesTheHypothesis)
{
  StartDeduction deduction;
  EXPECT_EQ(deduction.observePosition({0, 0}), 0);
  EXPECT_EQ(deduction.observePosition({-7, 0}), 0); // Seven cells ahead is inside under all of them.
  EXPECT_EQ(deduction.observePosition({0, 1}), 4);  // One cell to the right: outside under the right-border hypotheses.
  EXPECT_EQ(aliveIndices(deduction), (std::vector<int>{0, 2, 4, 7}));
  EXPECT_EQ(deduction.observePosition(StartDeduction::cellOf(Vector2f(-100.f, 0.f))), 0); // Still the start cell.
  EXPECT_EQ(deduction.aliveCount(), 4);
}

GTEST_TEST(MazeStartDeduction, TheLastHypothesisIsKeptAndTheConflictIsCounted)
{
  const MazeTagLayout layout;
  const Hypothesis& truth = StartDeduction::hypotheses()[6]; // Corner 63 facing north.
  StartDeduction deduction;
  observeForwardTag(deduction, truth, layout);
  ASSERT_TRUE(deduction.known());
  const int revision = deduction.revision;
  EXPECT_EQ(revision, 1);
  EXPECT_EQ(deduction.conflicts, 0);

  for(int i = 0; i < 3; ++i)
    deduction.observeEdge({0, 0}, Heading::south, false, 1.f); // Open behind the start: the border under the true hypothesis.
  EXPECT_TRUE(deduction.known());
  EXPECT_EQ(deduction.working(), truth);
  EXPECT_GT(deduction.conflicts, 0);
  const int conflicts = deduction.conflicts;
  EXPECT_EQ(deduction.observePosition({1, 0}), 0);
  EXPECT_EQ(deduction.conflicts, conflicts + 1);
  // A tag that fits another start does not move it either.
  const SeenTag other = seen(StartDeduction::hypotheses()[0], 1, Heading::east);
  EXPECT_EQ(deduction.observeTag(1, other.position, other.normal, 0, layout), 0);
  EXPECT_EQ(deduction.working(), truth);
  EXPECT_EQ(deduction.revision, revision);
}

GTEST_TEST(MazeStartDeduction, GivenStartNeverChanges)
{
  StartDeduction deduction;
  EXPECT_FALSE(deduction.setGiven(0, Heading::north));
  EXPECT_FALSE(deduction.setGiven(9, Heading::east));
  EXPECT_EQ(deduction.aliveCount(), 8);

  ASSERT_TRUE(deduction.setGiven(56, Heading::east));
  EXPECT_TRUE(deduction.known());
  EXPECT_EQ(deduction.working(), (Hypothesis{56, Heading::east}));
  const MazeTagLayout layout;
  const SeenTag contradicting = seen(StartDeduction::hypotheses()[0], 1, Heading::east);
  EXPECT_EQ(deduction.observeTag(1, contradicting.position, contradicting.normal, 0, layout), 0);
  EXPECT_EQ(deduction.conflicts, 1);
  for(int i = 0; i < 3; ++i)
    deduction.observeEdge({0, 0}, Heading::east, false, 1.f); // The right of the start is the south border under (56, east).
  EXPECT_EQ(deduction.observePosition({0, 1}), 0);
  EXPECT_GE(deduction.conflicts, 3);
  EXPECT_EQ(deduction.working(), (Hypothesis{56, Heading::east}));
  EXPECT_EQ(deduction.revision, 1); // Only the setGiven above changed the working hypothesis.
  EXPECT_EQ(observeForwardTag(deduction, {56, Heading::east}, layout), 0); // Its own tag fits.
}

GTEST_TEST(MazeStartDeduction, RevisionGrowsOnlyWhenTheWorkingHypothesisChanges)
{
  const MazeTagLayout layout;
  StartDeduction deduction;
  EXPECT_EQ(deduction.revision, 0);
  deduction.observePosition({0, 1}); // Removes the hypotheses with the border on the right; the working one survives.
  EXPECT_EQ(deduction.working(), StartDeduction::hypotheses()[0]);
  EXPECT_EQ(deduction.revision, 0);
  observeForwardTag(deduction, StartDeduction::hypotheses()[7], layout); // Removes everything but (63, west) and so the working one.
  EXPECT_EQ(deduction.working(), StartDeduction::hypotheses()[7]);
  EXPECT_EQ(deduction.revision, 1);
}

namespace
{
  /** The maze of `maze_1.maze` turned clockwise `quarterTurns` times; the start moves with it. */
  MazeDescription rotatedMaze(int quarterTurns)
  {
    MazeDescription maze;
    std::string error;
    EXPECT_TRUE(MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_1.maze", maze, error)) << error;
    for(int i = 0; i < quarterTurns; ++i)
    {
      maze.graph = maze.graph.rotated90();
      maze.startCell = cellId(cellCol(maze.startCell), size - 1 - cellRow(maze.startCell));
      maze.startHeading = turned(maze.startHeading, 1);
    }
    return maze;
  }

  /** Feeds the same observations of `maze` to a reference fusion in the maze frame and to a deduction in the start-relative frame. */
  void feed(const MazeDescription& maze, const Hypothesis& truth, StartDeduction& deduction, MazeMapFusion& reference, const MazeTagLayout& layout)
  {
    for(unsigned round = 0; round < 3; ++round)
    {
      const unsigned time = 1000 + round * 300;
      // Tags first: an edge takes one observation per interval, so the walls on the tagged faces below are dropped.
      for(int cell = 0; cell < cellCount; cell += 5)
        for(int h = 0; h < headingCount; ++h)
        {
          const Heading face = static_cast<Heading>(h);
          if(!layout.hasTag(cell, face, maze.graph))
            continue;
          const SeenTag tag = seen(truth, cell, face);
          reference.observeTag(cell, face, time);
          deduction.observeTag(cell, tag.position, tag.normal, time, layout);
        }
      for(int cell = 0; cell < cellCount; ++cell)
        for(int h = 0; h < headingCount; ++h)
        {
          const Heading side = static_cast<Heading>(h);
          const EdgeState state = maze.graph.get(cellRow(cell), cellCol(cell), side);
          const float weight = 1.f + 0.5f * static_cast<float>(round);
          reference.observe(cellRow(cell), cellCol(cell), side, state == EdgeState::wall, weight, time);
          deduction.observeEdge(StartDeduction::toStart(truth, {cellRow(cell), cellCol(cell)}), StartDeduction::toStart(truth, side),
                                state == EdgeState::wall, weight, time);
        }
      for(int cell = 0; cell < cellCount; cell += 7)
      {
        reference.markVisited(cellRow(cell), cellCol(cell));
        deduction.observePosition(StartDeduction::toStart(truth, {cellRow(cell), cellCol(cell)}));
      }
    }
  }
}

GTEST_TEST(MazeStartDeduction, ReplayUnderTheTrueHypothesisEqualsFusingInThatFrame)
{
  const MazeTagLayout layout;
  for(int quarterTurns = 0; quarterTurns < headingCount; ++quarterTurns)
  {
    const MazeDescription maze = rotatedMaze(quarterTurns);
    const Hypothesis truth = {maze.startCell, maze.startHeading};
    const int index = StartDeduction::indexOf(truth.cell, truth.heading);
    ASSERT_GE(index, 0) << quarterTurns;

    StartDeduction deduction;
    MazeMapFusion reference;
    reference.startCell = truth.cell;
    feed(maze, truth, deduction, reference, layout);
    EXPECT_TRUE(deduction.alive(index)) << quarterTurns;
    EXPECT_EQ(deduction.conflicts, 0) << quarterTurns;

    MazeMapFusion replayed;
    deduction.replay(truth, replayed, layout);
    EXPECT_TRUE(replayed == reference) << quarterTurns;
    EXPECT_TRUE(replayed.graph == maze.graph) << quarterTurns;
  }
}

GTEST_TEST(MazeStartDeduction, ReplayMatchesTheLookAheadOfTheFusion)
{
  const MazeTagLayout layout;
  const Hypothesis truth = {63, Heading::west};
  MazeMapFusion reference;
  reference.startCell = truth.cell;
  StartDeduction deduction;
  MazeMapFusion weights;
  // The robot walks two cells west along the bottom row and looks ahead from each cell; the same percept in both frames.
  for(int step = 0; step < 6; ++step)
  {
    const int col = 7 - step / 3;
    const std::vector<CellObservation> cells = {{0, EdgeState::open, EdgeState::wall, EdgeState::unknown, 300.f},
                                                {1, EdgeState::wall, EdgeState::open, EdgeState::open, 800.f},
                                                {2, EdgeState::unknown, EdgeState::open, EdgeState::wall, 1500.f}};
    const unsigned time = 500 + static_cast<unsigned>(step) * 100;
    reference.observeCellsAhead(7, col, Heading::west, cells, time);
    deduction.observeCellsAhead(StartDeduction::toStart(truth, {7, col}), StartDeduction::toStart(truth, Heading::west), cells, weights, time);
  }
  MazeMapFusion replayed;
  deduction.replay(truth, replayed, layout);
  EXPECT_TRUE(replayed.graph == reference.graph);
  EXPECT_TRUE(replayed == reference);
}

GTEST_TEST(MazeStartDeduction, ReplayUnderAnotherHypothesisDropsEdgesOutsideTheMaze)
{
  const MazeTagLayout layout;
  const MazeDescription maze = rotatedMaze(0);
  const Hypothesis truth = {maze.startCell, maze.startHeading};
  StartDeduction deduction;
  MazeMapFusion reference;
  feed(maze, truth, deduction, reference, layout);

  for(const Hypothesis& other : StartDeduction::hypotheses())
  {
    MazeMapFusion replayed;
    deduction.replay(other, replayed, layout);
    EXPECT_EQ(replayed.startCell, other.cell);
    // The border is a wall whatever was replayed, and the replay never invents an open border.
    for(int i = 0; i < size; ++i)
    {
      EXPECT_EQ(replayed.graph.get(0, i, Heading::north), EdgeState::wall);
      EXPECT_EQ(replayed.graph.get(i, size - 1, Heading::east), EdgeState::wall);
    }
  }
}

GTEST_TEST(MazeStartDeduction, NonFiniteWeightsAreIgnored)
{
  StartDeduction deduction;
  const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
  for(const float weight : {nan, inf, -inf, 0.f, -1.f})
    EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, weight), 0) << weight;
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.conflicts, 0);

  // A NaN distance gives a NaN weight in the look-ahead.
  const MazeMapFusion weights;
  const std::vector<CellObservation> cells = {{0, EdgeState::open, EdgeState::open, EdgeState::open, nan}};
  EXPECT_EQ(deduction.observeCellsAhead({0, 0}, Heading::north, cells, weights), 0);
  EXPECT_EQ(deduction.aliveCount(), 8);
  // The garbage is not in the log either.
  MazeMapFusion replayed;
  deduction.replay(StartDeduction::hypotheses()[0], replayed, MazeTagLayout());
  EXPECT_TRUE(replayed == MazeMapFusion());
}

GTEST_TEST(MazeStartDeduction, SettingTheGivenStartStartsOver)
{
  StartDeduction deduction;
  for(int i = 0; i < 2; ++i)
    deduction.observeEdge({0, 0}, Heading::east, false, 1.f, static_cast<unsigned>(i) * 300);
  deduction.observePosition({0, 1});
  const MazeTagLayout layout;
  const SeenTag contradicting = seen(StartDeduction::hypotheses()[2], 6, Heading::south);
  deduction.observeTag(6, contradicting.position, contradicting.normal, 0, layout);
  ASSERT_NE(deduction.working(), (Hypothesis{63, Heading::north}));
  const int revision = deduction.revision;

  ASSERT_TRUE(deduction.setGiven(63, Heading::north));
  EXPECT_EQ(deduction.revision, revision + 1);
  EXPECT_EQ(deduction.conflicts, 0);
  MazeMapFusion replayed;
  deduction.replay(deduction.working(), replayed, layout);
  MazeMapFusion fresh;
  fresh.startCell = 63;
  EXPECT_TRUE(replayed == fresh); // Nothing carried over.

  // Giving the hypothesis that already works does not change the revision.
  ASSERT_TRUE(deduction.setGiven(63, Heading::north));
  EXPECT_EQ(deduction.revision, revision + 1);
}

GTEST_TEST(MazeStartDeduction, ReplayMarksOnlyCellsInsideTheGrid)
{
  StartDeduction deduction;
  deduction.observePosition({0, 0});
  deduction.observePosition({0, 1}); // Inside the maze only under (0, east), (7, south), (56, north) and (63, west).
  deduction.observePosition({1, 0}); // Behind the start: outside under every hypothesis.
  MazeMapFusion replayed;
  deduction.replay(StartDeduction::hypotheses()[1], replayed, MazeTagLayout()); // (0, south): the right of the start is outside.
  int visited = 0;
  for(int cell = 0; cell < cellCount; ++cell)
    visited += replayed.visited(cellRow(cell), cellCol(cell)) ? 1 : 0;
  EXPECT_EQ(visited, 1);
  EXPECT_TRUE(replayed.visited(0, 0));
}

GTEST_TEST(MazeStartDeduction, ClockFilterDropsOutOfOrderObservationsWithoutMovingTheClock)
{
  StartDeduction deduction;
  // 1000 is accepted; 800 is out of order and must not move the edge's clock back, or 1100 (300 after it) would be accepted.
  for(const unsigned time : {1000u, 800u, 1100u, 1400u})
    deduction.observeEdge({0, 0}, Heading::east, false, 1.f, time);
  EXPECT_EQ(deduction.aliveCount(), 8); // Accepted: 1000 and 1400.
  EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, 1.f, 1700), 4);
}

GTEST_TEST(MazeStartDeduction, ClockFilterAcceptsAClockRestart)
{
  StartDeduction deduction;
  deduction.observeEdge({0, 0}, Heading::east, false, 1.f, 20000);
  deduction.observeEdge({0, 0}, Heading::east, false, 1.f, 300); // More than clockRestartThreshold older: a new session.
  EXPECT_EQ(deduction.aliveCount(), 8);
  EXPECT_EQ(deduction.observeEdge({0, 0}, Heading::east, false, 1.f, 600), 4);

  // A jump back of less than the threshold is only out of order.
  StartDeduction outOfOrder;
  outOfOrder.observeEdge({0, 0}, Heading::east, false, 1.f, 20000);
  outOfOrder.observeEdge({0, 0}, Heading::east, false, 1.f, 15000);
  outOfOrder.observeEdge({0, 0}, Heading::east, false, 1.f, 10000);
  EXPECT_EQ(outOfOrder.aliveCount(), 8);
}
