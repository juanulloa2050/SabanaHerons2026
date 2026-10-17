/**
 * @file MazeMapperBoundaryTest.cpp
 *
 * Regression for the map that erased known walls (ticket MZ-020, SimRobot report). The oracle describes the cell the robot
 * is TRUE in, while the mapper writes it on the cell it ESTIMATES the robot in. While the two differ (a robot dragged in the
 * scene, or an error near a cell boundary) an open edge of the true cell landed on the wrong cell and flipped a decided wall
 * within a few Cognition frames. These runs use the maze of the scene, a percept in every Cognition frame (about 60 Hz, each
 * with a new timestamp) and zero odometry.
 */

#include "MazeLocalizerSim.h"

#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Streaming/InStreams.h"

#include <gtest/gtest.h>
#include <iostream>

namespace
{
  using namespace Maze;
  using namespace Maze::Sim;

  constexpr int cell19 = 19; // row 2, col 3
  constexpr int cell20 = 20; // row 2, col 4: open to the west, wall to the east

  MazeDescription sceneMaze(int startCell)
  {
    MazeDescription maze;
    std::string error;
    EXPECT_TRUE(MazeFile::load(std::string(File::getBHDir()) + "/Config/Scenes/Includes/Maze/current.maze", maze, error)) << error;
    maze.startCell = startCell;
    maze.startHeading = Heading::south;
    return maze;
  }

  Config cognitionRate(const MazeDescription& maze)
  {
    static const bool registered = (FunctionList::execute(), true);
    (void) registered;
    const std::string text = layoutText("allFaces", "north, east, south, west");
    Config config;
    InMapMemory stream(text.c_str(), text.size());
    stream >> config.layout;
    config.maze = maze;
    config.frameMs = 16;
    config.perceptPeriodMs = 16;
    return config;
  }

  /** Number of decided edges that disagree with the real maze. */
  int wrongEdges(const WalkingRobot& run)
  {
    int wrong = 0;
    for(int cell = 0; cell < cellCount; ++cell)
      for(int h = 0; h < headingCount; ++h)
      {
        const EdgeState believed = run.fusion.graph.get(cellRow(cell), cellCol(cell), static_cast<Heading>(h));
        if(believed != EdgeState::unknown && believed != run.config.maze.graph.get(cellRow(cell), cellCol(cell), static_cast<Heading>(h)))
          ++wrong;
      }
    return wrong;
  }

  /** Stands in cell 20 until its walls are known. */
  void learnCell20(WalkingRobot& run)
  {
    run.stand(120);
    ASSERT_EQ(run.fusion.graph.get(2, 4, Heading::east), EdgeState::wall);
    ASSERT_EQ(run.fusion.graph.get(2, 4, Heading::west), EdgeState::open);
    ASSERT_EQ(wrongEdges(run), 0);
  }
}

GTEST_TEST(MazeMapperBoundary, DraggingTheRobotNextDoorDoesNotEraseTheWallItLearned)
{
  WalkingRobot run(cognitionRate(sceneMaze(cell20)));
  learnCell20(run);

  // Dragged in the scene from cell 20 to cell 19: no odometry, the estimate lags until a tag corrects it. The head needs
  // 20 frames (about 0.3 s) to bring a tag into view; until then the oracle describes cell 19 while the estimate is cell 20.
  int framesWithWrongEdge = 0;
  bool wallErased = false;
  run.onFrame = [&](const WalkingRobot& robot)
  {
    framesWithWrongEdge += wrongEdges(robot) > 0 ? 1 : 0;
    wallErased = wallErased || robot.fusion.graph.get(2, 4, Heading::east) != EdgeState::wall;
  };
  run.carryTo(cell19, Heading::south);
  const float tagViewAngle = run.config.maxTagViewAngle;
  run.config.maxTagViewAngle = -1.f;
  run.stand(20);
  run.config.maxTagViewAngle = tagViewAngle;
  run.stand(100);

  int row = 0, col = 0;
  ASSERT_TRUE(run.localization.cell(row, col));
  std::cout << "[MZ-020] drag 20 -> 19: estimated cell " << cellId(row, col) << " after 120 frames, frames with a wrong edge " << framesWithWrongEdge << std::endl;
  EXPECT_FALSE(wallErased) << "the east wall of cell 20 was erased at some frame";
  EXPECT_EQ(framesWithWrongEdge, 0);
}

GTEST_TEST(MazeMapperBoundary, AnEstimateAcrossTheBoundaryDoesNotWriteTheNeighbourOnTheCell)
{
  WalkingRobot run(cognitionRate(sceneMaze(cell20)));
  learnCell20(run);

  // The robot is 10 mm inside cell 19, the estimate 10 mm inside cell 20 (the tags are overruled every frame): the cell is ambiguous.
  const float boundaryX = 4.f * cellSizeMm;
  run.carryTo(cell19, Heading::south);
  run.truth.translation = Vector2f(boundaryX - 10.f, run.truth.translation.y());
  const Vector2f estimate(boundaryX + 10.f, run.truth.translation.y());
  for(int frame = 0; frame < 190; ++frame)
  {
    run.tick();
    run.localization.pose.translation = estimate;
  }
  std::cout << "[MZ-020] boundary: wrong edges after 3 s " << wrongEdges(run) << std::endl;
  EXPECT_EQ(run.fusion.graph.get(2, 4, Heading::east), EdgeState::wall) << "the east wall of cell 20 was erased";
  EXPECT_EQ(wrongEdges(run), 0);
}
