/**
 * @file MazeEdgeIndexTest.cpp
 *
 * Tests of the flat edge storage that MazeMap uses: shared edges, border slots, bounds and the graph round trip.
 */

#include "Platform/File.h"
#include "Tools/Maze/MazeEdgeIndex.h"
#include "Tools/Maze/MazeFile.h"

#include <gtest/gtest.h>
#include <string>

namespace
{
  using namespace Maze;

  struct Edges
  {
    std::array<EdgeState, EdgeIndex::horizontalCount> horizontal = {};
    std::array<EdgeState, EdgeIndex::verticalCount> vertical = {};

    EdgeState get(int row, int col, Heading heading) const { return EdgeIndex::get(horizontal, vertical, row, col, heading); }
    void set(int row, int col, Heading heading, EdgeState state) { EdgeIndex::set(horizontal, vertical, row, col, heading, state); }
  };
}

GTEST_TEST(MazeEdgeIndex, NeighboursShareTheSameEdge)
{
  Edges edges;
  edges.set(3, 4, Heading::east, EdgeState::wall);
  EXPECT_EQ(edges.get(3, 5, Heading::west), EdgeState::wall);
  edges.set(3, 4, Heading::north, EdgeState::open);
  EXPECT_EQ(edges.get(2, 4, Heading::south), EdgeState::open);
  // Nothing else changed.
  EXPECT_EQ(edges.get(3, 4, Heading::west), EdgeState::unknown);
  EXPECT_EQ(edges.get(3, 4, Heading::south), EdgeState::unknown);
}

GTEST_TEST(MazeEdgeIndex, SouthAndEastBordersUseTheExtraRowAndColumn)
{
  Edges edges;
  edges.set(size - 1, 2, Heading::south, EdgeState::wall);
  EXPECT_EQ(EdgeIndex::slotOf(size - 1, 2, Heading::south).index, static_cast<std::size_t>(size * size + 2));
  EXPECT_TRUE(EdgeIndex::slotOf(size - 1, 2, Heading::south).horizontal);
  EXPECT_EQ(edges.horizontal[static_cast<std::size_t>(size * size + 2)], EdgeState::wall);
  edges.set(5, size - 1, Heading::east, EdgeState::wall);
  EXPECT_EQ(EdgeIndex::slotOf(5, size - 1, Heading::east).index, static_cast<std::size_t>(5 * (size + 1) + size));
  EXPECT_FALSE(EdgeIndex::slotOf(5, size - 1, Heading::east).horizontal);
  EXPECT_EQ(edges.vertical[static_cast<std::size_t>(5 * (size + 1) + size)], EdgeState::wall);
}

GTEST_TEST(MazeEdgeIndex, InnerEdgesAreReachedFromTwoCellsAndBorderEdgesFromOne)
{
  std::array<int, EdgeIndex::horizontalCount> horizontalUses = {};
  std::array<int, EdgeIndex::verticalCount> verticalUses = {};
  for(int row = 0; row < size; ++row)
    for(int col = 0; col < size; ++col)
      for(int h = 0; h < headingCount; ++h)
      {
        const EdgeIndex::Slot slot = EdgeIndex::slotOf(row, col, static_cast<Heading>(h));
        ASSERT_TRUE(slot.valid);
        ++(slot.horizontal ? horizontalUses[slot.index] : verticalUses[slot.index]);
      }
  int once = 0, twice = 0, other = 0;
  for(const int uses : horizontalUses)
    (uses == 1 ? once : uses == 2 ? twice : other) += 1;
  for(const int uses : verticalUses)
    (uses == 1 ? once : uses == 2 ? twice : other) += 1;
  EXPECT_EQ(other, 0);
  EXPECT_EQ(once, 4 * size);
  EXPECT_EQ(twice, 2 * size * (size - 1));
}

GTEST_TEST(MazeEdgeIndex, OutOfRangeReadsUnknownAndIgnoresWrites)
{
  Edges edges;
  edges.set(-1, 0, Heading::south, EdgeState::wall);
  edges.set(0, size, Heading::west, EdgeState::wall);
  edges.set(size, 0, Heading::north, EdgeState::wall);
  EXPECT_EQ(edges.get(-1, 0, Heading::south), EdgeState::unknown);
  EXPECT_EQ(edges.get(0, size, Heading::west), EdgeState::unknown);
  for(const EdgeState state : edges.horizontal)
    EXPECT_EQ(state, EdgeState::unknown);
  for(const EdgeState state : edges.vertical)
    EXPECT_EQ(state, EdgeState::unknown);
}

GTEST_TEST(MazeEdgeIndex, GraphRoundTripOnARealMaze)
{
  MazeDescription maze;
  std::string error;
  ASSERT_TRUE(MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_1.maze", maze, error)) << error;
  Edges edges;
  EdgeIndex::setEdges(edges.horizontal, edges.vertical, maze.graph);
  EXPECT_TRUE(EdgeIndex::toGraph(edges.horizontal, edges.vertical) == maze.graph);
  // The border is stored as wall.
  EXPECT_EQ(edges.get(0, 0, Heading::north), EdgeState::wall);
  EXPECT_EQ(edges.get(size - 1, size - 1, Heading::south), EdgeState::wall);
  EXPECT_EQ(edges.get(3, size - 1, Heading::east), EdgeState::wall);
  EXPECT_EQ(EdgeIndex::unknownCount(edges.horizontal, edges.vertical), 0);
}

GTEST_TEST(MazeEdgeIndex, UnknownCountStartsWithEveryInnerEdgeAndCountsEachOnce)
{
  Edges edges;
  EdgeIndex::setEdges(edges.horizontal, edges.vertical, MazeGraph()); // inner edges unknown, border wall
  const int inner = 2 * size * (size - 1);
  EXPECT_EQ(EdgeIndex::unknownCount(edges.horizontal, edges.vertical), inner);
  edges.set(2, 2, Heading::east, EdgeState::wall);
  edges.set(2, 2, Heading::north, EdgeState::open);
  EXPECT_EQ(EdgeIndex::unknownCount(edges.horizontal, edges.vertical), inner - 2);
}
