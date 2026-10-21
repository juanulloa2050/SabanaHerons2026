/**
 * @file MazeMapFusionTest.cpp
 *
 * Tests of the evidence based wall map: thresholds, hysteresis, distance weights, look-ahead mapping,
 * tags, blocked edges, attempts and the timestamp filter.
 */

#include "Tools/Maze/MazeLocalization.h"
#include "Tools/Maze/MazeMapFusion.h"

#include <gtest/gtest.h>

namespace
{
  using namespace Maze;

  /** Observes the same edge `count` times with full weight; returns whether the last one changed it. */
  bool observeTimes(MazeMapFusion& fusion, int row, int col, Heading heading, bool isWall, int count)
  {
    bool changed = false;
    for(int i = 0; i < count; ++i)
      changed = fusion.observe(row, col, heading, isWall, 1.f);
    return changed;
  }
}

GTEST_TEST(MazeMapFusion, TwoObservationsKeepUnknownThreeDecide)
{
  MazeMapFusion fusion;
  EXPECT_FALSE(observeTimes(fusion, 2, 2, Heading::east, true, 2));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::unknown);
  EXPECT_TRUE(fusion.observe(2, 2, Heading::east, true, 1.f));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  // The neighbour reads the same edge.
  EXPECT_EQ(fusion.graph.get(2, 3, Heading::west), EdgeState::wall);

  EXPECT_FALSE(observeTimes(fusion, 4, 4, Heading::north, false, 2));
  EXPECT_TRUE(fusion.observe(4, 4, Heading::north, false, 1.f));
  EXPECT_EQ(fusion.graph.get(4, 4, Heading::north), EdgeState::open);
  EXPECT_EQ(fusion.graph.get(3, 4, Heading::south), EdgeState::open);
}

GTEST_TEST(MazeMapFusion, SharedEdgeAccumulatesFromBothSides)
{
  MazeMapFusion fusion;
  fusion.observe(2, 2, Heading::east, true, 1.f);
  fusion.observe(2, 3, Heading::west, true, 1.f);
  EXPECT_TRUE(fusion.observe(2, 2, Heading::east, true, 1.f));
}

GTEST_TEST(MazeMapFusion, FlipNeedsThreeContraryObservations)
{
  MazeMapFusion fusion;
  observeTimes(fusion, 2, 2, Heading::east, true, 10); // Lots of confirmation must not make the wall harder to flip.
  ASSERT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  EXPECT_FALSE(observeTimes(fusion, 2, 2, Heading::east, false, 2));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  EXPECT_TRUE(fusion.observe(2, 2, Heading::east, false, 1.f));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::open);
}

GTEST_TEST(MazeMapFusion, ConfirmationCancelsContraryEvidence)
{
  MazeMapFusion fusion;
  observeTimes(fusion, 2, 2, Heading::east, true, 3);
  observeTimes(fusion, 2, 2, Heading::east, false, 2);
  observeTimes(fusion, 2, 2, Heading::east, true, 2);
  EXPECT_FALSE(observeTimes(fusion, 2, 2, Heading::east, false, 2));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
}

GTEST_TEST(MazeMapFusion, BorderCannotBeOpened)
{
  MazeMapFusion fusion;
  EXPECT_FALSE(observeTimes(fusion, 0, 0, Heading::north, false, 10));
  EXPECT_EQ(fusion.graph.get(0, 0, Heading::north), EdgeState::wall);
  EXPECT_FALSE(observeTimes(fusion, 7, 7, Heading::east, false, 10));
  EXPECT_EQ(fusion.graph.get(7, 7, Heading::east), EdgeState::wall);
}

GTEST_TEST(MazeMapFusion, DistanceWeightDropsLinearly)
{
  MazeMapFusion fusion;
  EXPECT_FLOAT_EQ(fusion.wallWeight(0.f), 1.f);
  EXPECT_FLOAT_EQ(fusion.wallWeight(fusion.parameters.fullWeightDistance), 1.f);
  EXPECT_FLOAT_EQ(fusion.wallWeight(1500.f), 0.5f);
  EXPECT_FLOAT_EQ(fusion.wallWeight(fusion.parameters.zeroWeightDistance), 0.f);
  EXPECT_FLOAT_EQ(fusion.wallWeight(9000.f), 0.f);

  // Far observations need twice as many to decide.
  for(int i = 0; i < 5; ++i)
    EXPECT_FALSE(fusion.observe(3, 3, Heading::south, true, fusion.wallWeight(1500.f)));
  EXPECT_TRUE(fusion.observe(3, 3, Heading::south, true, fusion.wallWeight(1500.f)));
  // Without weight nothing is learned.
  EXPECT_FALSE(observeTimes(fusion, 5, 5, Heading::east, true, 0));
  EXPECT_FALSE(fusion.observe(5, 5, Heading::east, true, fusion.wallWeight(5000.f)));
}

GTEST_TEST(MazeMapFusion, TagIsFullEvidenceForAWall)
{
  MazeMapFusion fusion;
  const int cell = cellId(2, 5);
  EXPECT_FALSE(fusion.observeTag(cell, Heading::north));
  EXPECT_FALSE(fusion.observeTag(cell, Heading::north));
  EXPECT_TRUE(fusion.observeTag(cell, Heading::north));
  EXPECT_EQ(fusion.graph.get(2, 5, Heading::north), EdgeState::wall);
  EXPECT_FALSE(fusion.observeTag(-1, Heading::north));
  EXPECT_FALSE(fusion.observeTag(cellCount, Heading::north));
}

GTEST_TEST(MazeMapFusion, TaggedFaceIgnoresOpenFromTheWallPercept)
{
  // Heading::east looking gaze: cell 0 left is north. Open for the north edge of (2, 5) seen from (2, 5), 5 times at 300 ms.
  const auto openNorth = [](MazeMapFusion& fusion, int count, unsigned start)
  {
    CellObservation cell;
    cell.left = EdgeState::open;
    cell.distance = 100.f;
    int changes = 0;
    for(int i = 0; i < count; ++i)
      changes += fusion.observeCellsAhead(2, 5, Heading::east, {cell}, start + 300u * static_cast<unsigned>(i));
    return changes;
  };

  // Without a tag the percept opens the edge.
  MazeMapFusion plain;
  EXPECT_EQ(openNorth(plain, 5, 1000), 1);
  EXPECT_EQ(plain.graph.get(2, 5, Heading::north), EdgeState::open);

  // One tag is not enough to decide the edge, but the face is tagged: the percept cannot open it.
  MazeMapFusion fusion;
  EXPECT_FALSE(fusion.observeTag(cellId(2, 5), Heading::north, 500));
  EXPECT_EQ(openNorth(fusion, 5, 1000), 0);
  EXPECT_EQ(fusion.graph.get(2, 5, Heading::north), EdgeState::unknown);
  // More tags decide the wall, and the percept still cannot flip it, however often it sees through it.
  EXPECT_FALSE(fusion.observeTag(cellId(2, 5), Heading::north, 5000));
  EXPECT_TRUE(fusion.observeTag(cellId(2, 5), Heading::north, 5300));
  EXPECT_EQ(openNorth(fusion, 20, 10000), 0);
  EXPECT_EQ(fusion.graph.get(2, 5, Heading::north), EdgeState::wall);

  // The edge is shared: a tag on the south face of the neighbour tags it as well.
  MazeMapFusion shared;
  shared.observeTag(cellId(1, 5), Heading::south, 500);
  EXPECT_EQ(openNorth(shared, 5, 1000), 0);
  EXPECT_EQ(shared.graph.get(2, 5, Heading::north), EdgeState::unknown);

  // The veto is for that edge and for open percepts only: walls still count, other edges are unaffected.
  MazeMapFusion others;
  others.observeTag(cellId(2, 5), Heading::north, 500);
  CellObservation cell;
  cell.left = EdgeState::wall;
  cell.right = EdgeState::open;
  cell.distance = 100.f;
  int changes = 0;
  for(unsigned i = 0; i < 4; ++i)
    changes += others.observeCellsAhead(2, 5, Heading::east, {cell}, 1000 + 300 * i);
  EXPECT_EQ(changes, 2); // the wall on the left (the tag counts too) and the open right edge
  EXPECT_EQ(others.graph.get(2, 5, Heading::north), EdgeState::wall);
  EXPECT_EQ(others.graph.get(2, 5, Heading::south), EdgeState::open);
  // Other open evidence than the percept, the robot's own position, still counts for the edge.
  MazeMapFusion walked;
  walked.observeTag(cellId(2, 5), Heading::north, 500);
  EXPECT_FALSE(observeTimes(walked, 2, 5, Heading::north, false, 3)); // the tag counted +1 for the wall
  EXPECT_TRUE(observeTimes(walked, 2, 5, Heading::north, false, 1));
  EXPECT_EQ(walked.graph.get(2, 5, Heading::north), EdgeState::open);
}

GTEST_TEST(MazeMapFusion, ABlockedEdgeIsAWallAtOnce)
{
  MazeMapFusion fusion;
  EXPECT_TRUE(fusion.observeBlocked(3, 3, Heading::east));
  EXPECT_EQ(fusion.graph.get(3, 3, Heading::east), EdgeState::wall);
  EXPECT_EQ(fusion.graph.get(3, 4, Heading::west), EdgeState::wall) << "the neighbour reads the same edge";
  EXPECT_FALSE(fusion.observeBlocked(3, 3, Heading::east)) << "nothing changes the second time";
  EXPECT_FALSE(fusion.observeBlocked(3, 4, Heading::west));

  // An edge that is a wall already stays one, and it is blocked from then on.
  MazeMapFusion decided;
  observeTimes(decided, 2, 2, Heading::north, true, 3);
  EXPECT_FALSE(decided.observeBlocked(2, 2, Heading::north));
  EXPECT_FALSE(observeTimes(decided, 2, 2, Heading::north, false, 10));
  EXPECT_EQ(decided.graph.get(2, 2, Heading::north), EdgeState::wall);

  // An edge that was open is a wall at once, without contrary evidence building up first.
  MazeMapFusion open;
  observeTimes(open, 5, 5, Heading::south, false, 3);
  ASSERT_EQ(open.graph.get(5, 5, Heading::south), EdgeState::open);
  EXPECT_TRUE(open.observeBlocked(6, 5, Heading::north));
  EXPECT_EQ(open.graph.get(5, 5, Heading::south), EdgeState::wall);

  // The border is a wall already and out-of-range cells are ignored.
  MazeMapFusion border;
  EXPECT_FALSE(border.observeBlocked(0, 0, Heading::north));
  EXPECT_FALSE(border.observeBlocked(-1, 0, Heading::south));
  EXPECT_FALSE(border.observeBlocked(0, size, Heading::west));
}

GTEST_TEST(MazeMapFusion, ABlockedEdgeIsNeverOpenedByThePerceptNorByOtherEvidence)
{
  // The north edge of (2, 5) is the left of the gaze east from (2, 5) and the south edge of (1, 5), the right of the same gaze from (1, 5).
  const auto openFromBothCells = [](MazeMapFusion& fusion, int rounds)
  {
    CellObservation fromBelow, fromAbove;
    fromBelow.left = EdgeState::open;
    fromBelow.distance = 100.f;
    fromAbove.right = EdgeState::open;
    fromAbove.distance = 100.f;
    int changes = 0;
    for(int i = 0; i < rounds; ++i)
    {
      changes += fusion.observeCellsAhead(2, 5, Heading::east, {fromBelow}, 1000u + 600u * static_cast<unsigned>(i));
      changes += fusion.observeCellsAhead(1, 5, Heading::east, {fromAbove}, 1300u + 600u * static_cast<unsigned>(i));
    }
    return changes;
  };

  MazeMapFusion fusion;
  ASSERT_TRUE(fusion.observeBlocked(2, 5, Heading::north));
  EXPECT_EQ(openFromBothCells(fusion, 30), 0) << "from both cells of the shared edge, over a long time";
  EXPECT_EQ(fusion.graph.get(2, 5, Heading::north), EdgeState::wall);
  EXPECT_EQ(fusion.graph.get(1, 5, Heading::south), EdgeState::wall);

  // Without the block the same percepts open the edge.
  MazeMapFusion plain;
  EXPECT_GT(openFromBothCells(plain, 30), 0);
  EXPECT_EQ(plain.graph.get(2, 5, Heading::north), EdgeState::open);

  // Open evidence that is not the percept (untimed, like the position) does not open it either, from either side.
  EXPECT_FALSE(observeTimes(fusion, 2, 5, Heading::north, false, 20));
  EXPECT_FALSE(observeTimes(fusion, 1, 5, Heading::south, false, 20));
  EXPECT_EQ(fusion.graph.get(2, 5, Heading::north), EdgeState::wall);

  // Only that edge is vetoed: the other edges of the cell still take open evidence, and walls keep counting.
  EXPECT_TRUE(observeTimes(fusion, 2, 5, Heading::south, false, 3));
  EXPECT_FALSE(fusion.observe(2, 5, Heading::north, true, 1.f));
}

GTEST_TEST(MazeMapFusion, ABlockedEdgeIsBlockedEvenIfTheWallWasNotKnownWhenOpenEvidenceCame)
{
  // Open evidence came first and decided the edge; blocking it afterwards overrules that.
  MazeMapFusion fusion;
  observeTimes(fusion, 4, 4, Heading::west, false, 5);
  ASSERT_EQ(fusion.graph.get(4, 4, Heading::west), EdgeState::open);
  EXPECT_TRUE(fusion.observeBlocked(4, 4, Heading::west));
  EXPECT_EQ(fusion.graph.get(4, 4, Heading::west), EdgeState::wall);
  EXPECT_FALSE(observeTimes(fusion, 4, 4, Heading::west, false, 10));
  EXPECT_EQ(fusion.graph.get(4, 3, Heading::east), EdgeState::wall);
}

GTEST_TEST(MazeMapFusion, CellsAheadAreMappedForEveryGaze)
{
  for(int g = 0; g < headingCount; ++g)
  {
    const Heading gaze = static_cast<Heading>(g);
    MazeMapFusion fusion;
    fusion.parameters.threshold = 1.f;
    // Two cells ahead of (4, 4): left wall at the robot's cell, right open and front wall at cell 1.
    std::vector<CellObservation> cells(2);
    cells[0].ahead = 0;
    cells[0].left = EdgeState::wall;
    cells[0].right = EdgeState::open;
    cells[0].distance = 100.f;
    cells[1].ahead = 1;
    cells[1].front = EdgeState::wall;
    cells[1].distance = 400.f;
    EXPECT_EQ(fusion.observeCellsAhead(4, 4, gaze, cells), 3);

    const std::size_t i = static_cast<std::size_t>(gaze);
    EXPECT_EQ(fusion.graph.get(4, 4, turned(gaze, -1)), EdgeState::wall) << g;
    EXPECT_EQ(fusion.graph.get(4, 4, turned(gaze, 1)), EdgeState::open) << g;
    EXPECT_EQ(fusion.graph.get(4, 4, gaze), EdgeState::unknown) << g;
    EXPECT_EQ(fusion.graph.get(4 + rowDelta[i], 4 + colDelta[i], gaze), EdgeState::wall) << g;
    EXPECT_EQ(fusion.graph.get(4 + rowDelta[i], 4 + colDelta[i], turned(gaze, -1)), EdgeState::unknown) << g;
  }
}

GTEST_TEST(MazeMapFusion, CellsAheadLeftAndRightFollowTheCompass)
{
  MazeMapFusion fusion;
  fusion.parameters.threshold = 1.f;
  std::vector<CellObservation> cells(1);
  cells[0].left = EdgeState::wall;
  cells[0].right = EdgeState::open;
  // Looking east, the left side is north.
  fusion.observeCellsAhead(4, 4, Heading::east, cells);
  EXPECT_EQ(fusion.graph.get(4, 4, Heading::north), EdgeState::wall);
  EXPECT_EQ(fusion.graph.get(4, 4, Heading::south), EdgeState::open);
}

GTEST_TEST(MazeMapFusion, CellsOutsideTheMazeAreSkipped)
{
  MazeMapFusion fusion;
  fusion.parameters.threshold = 1.f;
  std::vector<CellObservation> cells(3);
  for(int i = 0; i < 3; ++i)
  {
    cells[static_cast<std::size_t>(i)].ahead = i;
    cells[static_cast<std::size_t>(i)].front = EdgeState::open;
    cells[static_cast<std::size_t>(i)].distance = 100.f;
  }
  // Cell 0 ends at the east border, which cannot be opened; cells 1 and 2 are outside.
  EXPECT_EQ(fusion.observeCellsAhead(0, 7, Heading::east, cells), 0);
  EXPECT_EQ(fusion.graph.get(0, 7, Heading::east), EdgeState::wall);
}

GTEST_TEST(MazeMapFusion, NewAttemptKeepsTheMap)
{
  MazeMapFusion fusion;
  observeTimes(fusion, 2, 2, Heading::east, true, 3);
  fusion.markVisited(2, 2);
  MazeMapFusion before = fusion;
  EXPECT_EQ(fusion.attempt, 1);
  fusion.newAttempt();
  EXPECT_EQ(fusion.attempt, 2);
  EXPECT_TRUE(fusion == before);
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  EXPECT_TRUE(fusion.visited(2, 2));
  EXPECT_FALSE(fusion.visited(2, 3));
  fusion.markVisited(2, 3);
  EXPECT_FALSE(fusion == before);
}

GTEST_TEST(MazeMapFusion, GoalCells)
{
  int goals = 0;
  for(int id = 0; id < cellCount; ++id)
    if(isGoal(cellRow(id), cellCol(id)))
      ++goals;
  EXPECT_EQ(goals, 4);
  EXPECT_TRUE(isGoal(cellRow(27), cellCol(27)));
  EXPECT_TRUE(isGoal(cellRow(36), cellCol(36)));
}

GTEST_TEST(MazeMapFusion, TimestampFilterIgnoresRepeatedPercepts)
{
  TimestampFilter filter;
  EXPECT_TRUE(filter.accept(0));
  EXPECT_FALSE(filter.accept(0));
  EXPECT_TRUE(filter.accept(100));
  EXPECT_FALSE(filter.accept(100));
  EXPECT_TRUE(filter.accept(133));
  filter.reset();
  EXPECT_TRUE(filter.accept(133));
}

GTEST_TEST(MazeMapFusion, AnEdgeTakesOneTimedObservationPerInterval)
{
  MazeMapFusion fusion; // 250 ms
  // 60 Hz frames for 400 ms: only t = 1000 and t = 1264 count (the first frame 250 ms or more later), so the edge stays unknown.
  for(unsigned t = 1000; t <= 1400; t += 16)
    fusion.observe(2, 2, Heading::east, true, 1.f, t);
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::unknown);
  // A third observation, 250 ms after the second, decides it.
  EXPECT_TRUE(fusion.observe(2, 2, Heading::east, true, 1.f, 1520));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  // The neighbour shares the edge and its interval.
  EXPECT_FALSE(fusion.observe(2, 3, Heading::west, false, 1.f, 1600)); // 80 ms after the last one
}

GTEST_TEST(MazeMapFusion, ARepeatedFrameCannotFlipADecidedWall)
{
  MazeMapFusion fusion;
  observeTimes(fusion, 2, 2, Heading::east, true, 3);
  ASSERT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  for(unsigned t = 5000; t < 5240; t += 16)
    fusion.observe(2, 2, Heading::east, false, 1.f, t);
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
}

GTEST_TEST(MazeMapFusion, TheIntervalAppliesToTags)
{
  MazeMapFusion fusion;
  EXPECT_FALSE(fusion.observeTag(cellId(2, 2), Heading::east, 100));
  EXPECT_FALSE(fusion.observeTag(cellId(2, 2), Heading::east, 150)); // too soon: dropped
  EXPECT_FALSE(fusion.observeTag(cellId(2, 2), Heading::east, 350));
  EXPECT_TRUE(fusion.observeTag(cellId(2, 2), Heading::east, 600));
}

GTEST_TEST(MazeMapFusion, AnOutOfOrderObservationDoesNotShortenTheInterval)
{
  MazeMapFusion fusion;
  EXPECT_FALSE(fusion.observe(2, 2, Heading::east, true, 1.f, 1000)); // wall percept
  EXPECT_FALSE(fusion.observeTag(cellId(2, 2), Heading::east, 900)); // older tag percept: rejected, the clock stays at 1000
  EXPECT_FALSE(fusion.observe(2, 2, Heading::east, true, 1.f, 1150)); // still blocked, needs >= 1250
  EXPECT_FALSE(fusion.observe(2, 2, Heading::east, true, 1.f, 1250)); // second counted observation
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::unknown);
  EXPECT_TRUE(fusion.observe(2, 2, Heading::east, true, 1.f, 1500)); // third decides
}

GTEST_TEST(MazeMapFusion, ALargeBackwardJumpIsANewSession)
{
  MazeMapFusion fusion;
  fusion.observe(2, 2, Heading::east, true, 1.f, 100000);
  fusion.observe(2, 2, Heading::east, true, 1.f, 100250);
  // The clock restarted: accepted, as the third observation, and the edge is decided.
  EXPECT_TRUE(fusion.observe(2, 2, Heading::east, true, 1.f, 0));
  EXPECT_EQ(fusion.graph.get(2, 2, Heading::east), EdgeState::wall);
  // The new clock rules from here on.
  EXPECT_FALSE(fusion.observe(2, 2, Heading::east, false, 1.f, 100)); // too soon after 0
}

GTEST_TEST(MazeMapFusion, NearCellCentreIsMeasuredPerAxis)
{
  const CellCentre centre = cellCentre(2, 3);
  EXPECT_TRUE(nearCellCentre(centre.x, centre.y, 2, 3, 150.f));
  EXPECT_TRUE(nearCellCentre(centre.x + 149.f, centre.y - 149.f, 2, 3, 150.f));
  EXPECT_FALSE(nearCellCentre(centre.x + 151.f, centre.y, 2, 3, 150.f));
  EXPECT_FALSE(nearCellCentre(centre.x, centre.y - 151.f, 2, 3, 150.f));
  EXPECT_FALSE(nearCellCentre(centre.x, centre.y, 2, 4, 150.f));
}
