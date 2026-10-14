#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeTagLayout.h"
#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Streaming/InStreams.h"

#include <gtest/gtest.h>
#include <algorithm>

namespace
{
  std::string mazePath(int index)
  {
    return std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_" + std::to_string(index) + ".maze";
  }

  Maze::MazeDescription loadMaze(int index)
  {
    Maze::MazeDescription maze;
    std::string error;
    EXPECT_TRUE(Maze::MazeFile::load(mazePath(index), maze, error)) << "maze " << index << ": " << error;
    return maze;
  }

  /** Registers the streamable types (enums) once; the application does this at start-up. */
  void registerTypes()
  {
    static const bool registered = (FunctionList::execute(), true);
    (void) registered;
  }

  /** A layout read from a complete config text; `faces` and `walls` are the contents of the two lists. */
  Maze::MazeTagLayout layoutFrom(const std::string& mode, const std::string& faces = "north, east, south, west", const std::string& walls = "")
  {
    registerTypes();
    const std::string text = "mode = " + mode + "; faces = [" + faces + "]; walls = [" + walls + "]; centreHeight = 300; size = 150;";
    Maze::MazeTagLayout layout;
    InMapMemory stream(text.c_str(), text.size());
    stream >> layout;
    return layout;
  }

  bool isWall(const Maze::MazeGraph& graph, int cell, Maze::Heading face)
  {
    return graph.get(Maze::cellRow(cell), Maze::cellCol(cell), face) == Maze::EdgeState::wall;
  }

  /** The faces and the (cell, face) pairs a layout assigns to every cell, over all generated mazes. */
  template<typename Check>
  void forEveryFace(Check check)
  {
    for(int index = 1; index <= 20; ++index)
    {
      const Maze::MazeDescription maze = loadMaze(index);
      for(int cell = 0; cell < Maze::cellCount; ++cell)
        for(int h = 0; h < Maze::headingCount; ++h)
          check(index, maze.graph, cell, static_cast<Maze::Heading>(h));
    }
  }
}

GTEST_TEST(MazeTagLayout, DefaultsMatchTheRules)
{
  const Maze::MazeTagLayout layout;
  EXPECT_EQ(layout.mode, Maze::allFaces);
  EXPECT_EQ(layout.faces.size(), 4u);
  EXPECT_TRUE(layout.walls.empty());
  EXPECT_FLOAT_EQ(layout.centreHeight, 300.f);
  EXPECT_FLOAT_EQ(layout.size, 150.f);
}

GTEST_TEST(MazeTagLayout, AllFacesPutsATagOnEveryWallFace)
{
  const Maze::MazeTagLayout layout = layoutFrom("allFaces");
  forEveryFace([&](int index, const Maze::MazeGraph& graph, int cell, Maze::Heading face)
  {
    EXPECT_EQ(layout.hasTag(cell, face, graph), isWall(graph, cell, face))
        << "maze " << index << " cell " << cell << " face " << static_cast<int>(face);
  });
}

GTEST_TEST(MazeTagLayout, FacesEastOnlyTagsEastWallFaces)
{
  const Maze::MazeTagLayout layout = layoutFrom("faces", "east");
  forEveryFace([&](int index, const Maze::MazeGraph& graph, int cell, Maze::Heading face)
  {
    const bool expected = face == Maze::Heading::east && isWall(graph, cell, face);
    EXPECT_EQ(layout.hasTag(cell, face, graph), expected)
        << "maze " << index << " cell " << cell << " face " << static_cast<int>(face);
  });
}

GTEST_TEST(MazeTagLayout, WallsModeTagsOnlyListedPairsThatAreWalls)
{
  const Maze::MazeDescription maze = loadMaze(1);
  // Cell 0 is a corner, so its north and west faces are walls. Find an open face to list as well.
  int openCell = -1;
  Maze::Heading openFace = Maze::Heading::north;
  for(int cell = 0; cell < Maze::cellCount && openCell < 0; ++cell)
    for(int h = 0; h < Maze::headingCount; ++h)
      if(!isWall(maze.graph, cell, static_cast<Maze::Heading>(h)))
      {
        openCell = cell;
        openFace = static_cast<Maze::Heading>(h);
        break;
      }
  ASSERT_GE(openCell, 0);
  ASSERT_TRUE(isWall(maze.graph, 0, Maze::Heading::north));
  ASSERT_TRUE(isWall(maze.graph, 0, Maze::Heading::west));

  const char* const faceNames[] = {"north", "east", "south", "west"};
  const Maze::MazeTagLayout layout = layoutFrom("walls", "", "{cell = 0; face = north;}, {cell = 0; face = west;}, {cell = " +
                                                std::to_string(openCell) + "; face = " + faceNames[static_cast<int>(openFace)] + ";}");
  ASSERT_EQ(layout.walls.size(), 3u);

  for(int cell = 0; cell < Maze::cellCount; ++cell)
    for(int h = 0; h < Maze::headingCount; ++h)
    {
      const Maze::Heading face = static_cast<Maze::Heading>(h);
      const bool expected = cell == 0 && (face == Maze::Heading::north || face == Maze::Heading::west);
      EXPECT_EQ(layout.hasTag(cell, face, maze.graph), expected) << "cell " << cell << " face " << h;
    }
  // The listed open face is a candidate but never carries a tag.
  const std::vector<Maze::Heading> candidates = layout.candidateFaces(openCell);
  EXPECT_NE(std::find(candidates.begin(), candidates.end(), openFace), candidates.end());
  EXPECT_FALSE(layout.hasTag(openCell, openFace, maze.graph));
}

GTEST_TEST(MazeTagLayout, NeverATagWhereThereIsNoWall)
{
  const Maze::MazeTagLayout layouts[] = {
    layoutFrom("allFaces"),
    layoutFrom("faces"),
    layoutFrom("faces", "west"),
    layoutFrom("walls", "", "{cell = 0; face = north;}, {cell = 9; face = east;}, {cell = 27; face = south;}"),
  };
  for(const Maze::MazeTagLayout& layout : layouts)
    forEveryFace([&](int index, const Maze::MazeGraph& graph, int cell, Maze::Heading face)
    {
      if(layout.hasTag(cell, face, graph))
        EXPECT_TRUE(isWall(graph, cell, face)) << "maze " << index << " cell " << cell << " face " << static_cast<int>(face);
    });
}

GTEST_TEST(MazeTagLayout, CandidateFacesFollowTheMode)
{
  using Maze::Heading;
  const std::vector<Heading> all = {Heading::north, Heading::east, Heading::south, Heading::west};

  EXPECT_EQ(layoutFrom("allFaces").candidateFaces(5), all);
  EXPECT_EQ(layoutFrom("faces", "east").candidateFaces(5), std::vector<Heading>{Heading::east});
  EXPECT_EQ(layoutFrom("faces", "west, north").candidateFaces(63), (std::vector<Heading>{Heading::north, Heading::west}));
  EXPECT_TRUE(layoutFrom("faces", "").candidateFaces(5).empty());

  const Maze::MazeTagLayout walls = layoutFrom("walls", "", "{cell = 12; face = south;}, {cell = 12; face = north;}, {cell = 13; face = east;}");
  EXPECT_EQ(walls.candidateFaces(12), (std::vector<Heading>{Heading::north, Heading::south}));
  EXPECT_EQ(walls.candidateFaces(13), std::vector<Heading>{Heading::east});
  EXPECT_TRUE(walls.candidateFaces(14).empty());

  // Cells outside the maze have no tag.
  EXPECT_TRUE(layoutFrom("allFaces").candidateFaces(-1).empty());
  EXPECT_TRUE(layoutFrom("allFaces").candidateFaces(Maze::cellCount).empty());
}

GTEST_TEST(MazeTagLayout, ShippedConfigIsTheDefaultLayout)
{
  registerTypes();
  for(const char* scenario : {"Maze", "MazeSafe"})
  {
    Maze::MazeTagLayout layout;
    layout.mode = Maze::walls;
    ASSERT_TRUE(Maze::MazeTagLayout::load(std::string("Scenarios/") + scenario + "/mazeTagLayout.cfg", layout)) << scenario;
    EXPECT_EQ(layout.mode, Maze::allFaces) << scenario;
    EXPECT_EQ(layout.faces.size(), 4u) << scenario;
    EXPECT_TRUE(layout.walls.empty()) << scenario;
    EXPECT_FLOAT_EQ(layout.centreHeight, 300.f) << scenario;
    EXPECT_FLOAT_EQ(layout.size, 150.f) << scenario;
  }
  Maze::MazeTagLayout missing;
  EXPECT_FALSE(Maze::MazeTagLayout::load("Scenarios/Maze/doesNotExist.cfg", missing));
}
