#include "Tools/Maze/MazeFile.h"
#include "Platform/File.h"

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>

namespace
{
  std::string mazePath(int index)
  {
    return std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_" + std::to_string(index) + ".maze";
  }

  std::string readFile(const std::string& path)
  {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
  }

  void replaceAt(std::string& text, std::size_t line, std::size_t col, char c)
  {
    text[line * 18 + col] = c; // 17 characters plus '\n' per line
  }

  /** Parses `text` and expects failure with a message containing `part`. */
  void expectRejected(const std::string& text, const std::string& part)
  {
    Maze::MazeDescription maze;
    std::string error;
    EXPECT_FALSE(Maze::MazeFile::parse(text, maze, error));
    EXPECT_NE(error.find(part), std::string::npos) << error;
  }
}

GTEST_TEST(MazeFile, RoundTripOfGeneratedMazes)
{
  for(int i = 1; i <= 20; ++i)
  {
    const std::string raw = readFile(mazePath(i));
    ASSERT_FALSE(raw.empty()) << mazePath(i);
    Maze::MazeDescription maze;
    std::string error;
    ASSERT_TRUE(Maze::MazeFile::load(mazePath(i), maze, error)) << "maze " << i << ": " << error;
    EXPECT_EQ(Maze::MazeFile::toText(maze), raw) << "maze " << i;
  }
}

GTEST_TEST(MazeFile, StartAndGoalFromFile)
{
  Maze::MazeDescription maze;
  std::string error;
  ASSERT_TRUE(Maze::MazeFile::load(mazePath(1), maze, error)) << error;
  const int row = Maze::cellRow(maze.startCell), col = Maze::cellCol(maze.startCell);
  EXPECT_TRUE(row == 0 || row == 7);
  EXPECT_TRUE(col == 0 || col == 7);
  EXPECT_EQ(maze.graph.get(row, col, maze.startHeading), Maze::EdgeState::open);
}

GTEST_TEST(MazeFile, Fnv1aChecksum)
{
  EXPECT_EQ(Maze::MazeFile::toHex(Maze::MazeFile::fnv1a32("")), "811c9dc5");
  EXPECT_EQ(Maze::MazeFile::toHex(Maze::MazeFile::fnv1a32(readFile(mazePath(1)))), "f9322482");
  EXPECT_EQ(Maze::MazeFile::toHex(Maze::MazeFile::fnv1a32(readFile(mazePath(10)))), "8da0a1c6");
}

GTEST_TEST(MazeFile, RejectsInvalidMazes)
{
  const std::string valid = readFile(mazePath(1));
  Maze::MazeDescription maze;
  std::string error;
  ASSERT_TRUE(Maze::MazeFile::parse(valid, maze, error)) << error;

  expectRejected("", "lines");
  expectRejected(valid.substr(0, valid.size() - 19), "lines");

  std::string noStart = valid;
  for(std::size_t i = 0; i < noStart.size(); ++i)
    if(noStart[i] == 'S')
      noStart[i] = ' ';
  expectRejected(noStart, "exactly one S");

  // Move S from its corner to cell 9 (row 1, col 1), which is not a corner.
  std::string movedStart = noStart;
  replaceAt(movedStart, 3, 3, 'S');
  expectRejected(movedStart, "not in a corner");

  // Open the top border above cell 0.
  std::string openBorder = valid;
  replaceAt(openBorder, 0, 1, ' ');
  expectRejected(openBorder, "border open");

  std::string badChar = valid;
  replaceAt(badChar, 0, 1, 'x');
  expectRejected(badChar, "invalid wall character");
}

GTEST_TEST(MazeGraph, BorderIsWallAndInnerEdgesUnknown)
{
  const Maze::MazeGraph graph;
  for(int i = 0; i < Maze::size; ++i)
  {
    EXPECT_EQ(graph.get(0, i, Maze::Heading::north), Maze::EdgeState::wall);
    EXPECT_EQ(graph.get(Maze::size - 1, i, Maze::Heading::south), Maze::EdgeState::wall);
    EXPECT_EQ(graph.get(i, 0, Maze::Heading::west), Maze::EdgeState::wall);
    EXPECT_EQ(graph.get(i, Maze::size - 1, Maze::Heading::east), Maze::EdgeState::wall);
  }
  EXPECT_EQ(graph.get(3, 3, Maze::Heading::north), Maze::EdgeState::unknown);
  EXPECT_EQ(graph.get(3, 3, Maze::Heading::east), Maze::EdgeState::unknown);
}

GTEST_TEST(MazeGraph, NeighboursShareEdges)
{
  Maze::MazeGraph graph;
  graph.set(2, 2, Maze::Heading::east, Maze::EdgeState::open);
  EXPECT_EQ(graph.get(2, 3, Maze::Heading::west), Maze::EdgeState::open);
  graph.set(2, 2, Maze::Heading::south, Maze::EdgeState::wall);
  EXPECT_EQ(graph.get(3, 2, Maze::Heading::north), Maze::EdgeState::wall);
  EXPECT_EQ(graph.get(2, 2, Maze::Heading::north), Maze::EdgeState::unknown);
}

GTEST_TEST(MazeGraph, UnknownPolicy)
{
  Maze::MazeGraph graph;
  graph.set(4, 4, Maze::Heading::east, Maze::EdgeState::open);
  graph.set(4, 4, Maze::Heading::south, Maze::EdgeState::wall);
  EXPECT_TRUE(graph.canPass(4, 4, Maze::Heading::east, Maze::UnknownPolicy::asWall));
  EXPECT_FALSE(graph.canPass(4, 4, Maze::Heading::south, Maze::UnknownPolicy::asOpen));
  EXPECT_TRUE(graph.canPass(4, 4, Maze::Heading::north, Maze::UnknownPolicy::asOpen));
  EXPECT_FALSE(graph.canPass(4, 4, Maze::Heading::north, Maze::UnknownPolicy::asWall));
}

GTEST_TEST(MazeGraph, CellIdsGoalsAndCentres)
{
  EXPECT_EQ(Maze::cellId(3, 4), 28);
  EXPECT_EQ(Maze::cellRow(35), 4);
  EXPECT_EQ(Maze::cellCol(35), 3);
  int goals = 0;
  for(int id = 0; id < Maze::cellCount; ++id)
    if(Maze::isGoal(Maze::cellRow(id), Maze::cellCol(id)))
    {
      ++goals;
      EXPECT_TRUE(id == 27 || id == 28 || id == 35 || id == 36);
    }
  EXPECT_EQ(goals, 4);
  EXPECT_FLOAT_EQ(Maze::cellCentre(0, 0).x, 250.f);
  EXPECT_FLOAT_EQ(Maze::cellCentre(0, 0).y, 3750.f);
  EXPECT_FLOAT_EQ(Maze::cellCentre(7, 7).x, 3750.f);
  EXPECT_FLOAT_EQ(Maze::cellCentre(7, 7).y, 250.f);
}

GTEST_TEST(MazeGraph, RotationClockwise)
{
  Maze::MazeDescription maze;
  std::string error;
  ASSERT_TRUE(Maze::MazeFile::load(mazePath(1), maze, error)) << error;
  const Maze::MazeGraph rotated = maze.graph.rotated90();
  EXPECT_NE(rotated, maze.graph);
  EXPECT_EQ(rotated.rotated90().rotated90().rotated90(), maze.graph);
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
      for(int h = 0; h < Maze::headingCount; ++h)
      {
        const Maze::Heading heading = static_cast<Maze::Heading>(h);
        EXPECT_EQ(rotated.get(col, Maze::size - 1 - row, Maze::turned(heading, 1)), maze.graph.get(row, col, heading));
      }
}
