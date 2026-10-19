#include "Tools/Maze/MazeWallView.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeOracle.h"
#include "Platform/File.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
  using namespace Maze::WallView;
  namespace Oracle = Maze::Oracle;

  const Vector2f cameraInRobot(50.f, 0.f);

  Maze::MazeGraph loadMaze(int number)
  {
    Maze::MazeDescription maze;
    std::string error;
    EXPECT_TRUE(Maze::MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_" + std::to_string(number) + ".maze", maze, error)) << error;
    return maze.graph;
  }

  /** Deterministic noise in a disc of radius `radius`. */
  struct Noise
  {
    std::uint32_t state = 4711;
    float next()
    {
      state = state * 1664525u + 1013904223u;
      return static_cast<float>(state >> 8) / static_cast<float>(1 << 24);
    }
    Vector2f disc(float radius)
    {
      const float angle = next() * 2.f * pi, length = radius * std::sqrt(next());
      return Vector2f(std::cos(angle), std::sin(angle)) * length;
    }
  };

  /**
   * The boundary points a camera with a horizontal field of view of `halfFov` either side of its gaze would
   * measure: the walls of the maze sampled every `spacingMm`, as far as no other wall blocks the line of sight,
   * with noise of at most `noiseMm`. In the robot frame.
   */
  std::vector<Vector2f> boundaryPoints(const Maze::MazeGraph& maze, const Pose2f& robot, float gazeYaw, float halfFov, float noiseMm, float spacingMm = 50.f)
  {
    Noise noise;
    std::vector<Vector2f> points;
    const Vector2f camera = robot.translation + Oracle::rotated(cameraInRobot, robot.rotation);
    for(int row = 0; row < Maze::size; ++row)
      for(int col = 0; col < Maze::size; ++col)
        for(int h = 0; h < Maze::headingCount; ++h)
        {
          const Maze::Heading heading = static_cast<Maze::Heading>(h);
          const int r = row + Maze::rowDelta[static_cast<std::size_t>(h)], c = col + Maze::colDelta[static_cast<std::size_t>(h)];
          if(maze.get(row, col, heading) != Maze::EdgeState::wall || ((heading == Maze::Heading::south || heading == Maze::Heading::east) && Maze::inside(r, c)))
            continue;
          const Vector2f direction = Oracle::directionOf(heading), along(-direction.y(), direction.x());
          const Vector2f middle = Oracle::cellCentreOf(row, col) + direction * Oracle::halfCellMm;
          for(float offset = -Oracle::halfCellMm + spacingMm / 2.f; offset < Oracle::halfCellMm; offset += spacingMm)
          {
            const Vector2f point = middle + along * offset;
            const Vector2f inRobot = Oracle::rotated(point - robot.translation, -robot.rotation);
            const Vector2f toPoint = inRobot - cameraInRobot;
            if(std::abs(Angle::normalize(std::atan2(toPoint.y(), toPoint.x()) - gazeYaw)) > halfFov ||
               !Oracle::lineOfSightClear(maze, camera, point, Maze::cellId(row, col), heading))
              continue;
            points.push_back(Oracle::rotated(point + noise.disc(noiseMm) - robot.translation, -robot.rotation));
          }
        }
    return points;
  }

  Oracle::View oracleView(const Maze::MazeGraph& maze, const Pose2f& robot, float gazeYaw, int range)
  {
    Oracle::View view;
    EXPECT_TRUE(Oracle::computeView(maze, robot, {cameraInRobot, gazeYaw, 1.f}, range, view));
    return view;
  }

  Result viewOf(const std::vector<Vector2f>& points, const Pose2f& robot, float gazeYaw, int range, const Params& params = Params())
  {
    Result result;
    EXPECT_TRUE(computeWallView(points, robot, cameraInRobot, gazeYaw, range, params, result));
    return result;
  }
}

GTEST_TEST(MazeWallView, EmptyPointsGiveUnknownEdgesAndTheOwnCell)
{
  const Result result = viewOf({}, Pose2f(0.f, Oracle::cellCentreOf(3, 3)), 0.f, 2);
  ASSERT_EQ(result.cellsAhead.size(), 1u);
  EXPECT_EQ(result.cellsAhead[0].ahead, 0);
  EXPECT_EQ(result.cellsAhead[0].front, Maze::EdgeState::unknown);
  EXPECT_EQ(result.cellsAhead[0].distance, 0.f);
  EXPECT_EQ(result.lookAheadTurns, 0);
  EXPECT_TRUE(result.segments.empty());
  EXPECT_EQ(result.frontDistance, -1.f);
}

GTEST_TEST(MazeWallView, RobotOutsideTheMaze)
{
  Result result;
  EXPECT_FALSE(computeWallView({}, Pose2f(0.f, Vector2f(-100.f, 100.f)), cameraInRobot, 0.f, 2, Params(), result));
  EXPECT_TRUE(result.cellsAhead.empty());
}

GTEST_TEST(MazeWallView, ComparisonWithOracle)
{
  int edges = 0, known = 0, contradictions = 0;
  for(const int number : {1, 2, 3})
  {
    const Maze::MazeGraph maze = loadMaze(number);
    for(const auto& [row, col] : {std::pair(0, 5), std::pair(2, 3), std::pair(4, 1), std::pair(5, 6), std::pair(7, 2)})
      for(const float yaw : {0.f, pi / 2.f, pi})
        for(const float turn : {0.f, pi / 2.f, -pi / 2.f, pi})
        {
          const Pose2f robot(turn + 0.1f, Oracle::cellCentreOf(row, col) + Vector2f(30.f, -40.f));
          const Oracle::View truth = oracleView(maze, robot, yaw, 3);
          const Result result = viewOf(boundaryPoints(maze, robot, yaw, 1.4f, 50.f), robot, yaw, 3);
          const std::string where = "maze " + std::to_string(number) + " cell " + std::to_string(row) + "," + std::to_string(col) +
                                    " yaw " + std::to_string(yaw) + " turn " + std::to_string(turn);

          // The same gaze and the same cells; an unknown front edge ends the walk early.
          EXPECT_EQ(result.lookAheadTurns, static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(truth.gaze) - robot.rotation) / (pi / 2.f))) < -1 ?
                    static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(truth.gaze) - robot.rotation) / (pi / 2.f))) + 4 :
                    static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(truth.gaze) - robot.rotation) / (pi / 2.f)))) << where;
          ASSERT_LE(result.cellsAhead.size(), truth.cells.size()) << where;
          int walls = 0;
          for(std::size_t i = 0; i < result.cellsAhead.size(); ++i)
          {
            const Cell& cell = result.cellsAhead[i];
            const Oracle::LookAheadCell& expected = truth.cells[i];
            EXPECT_EQ(cell.ahead, expected.ahead) << where;
            EXPECT_NEAR(cell.distance, (Oracle::cellCentreOf(expected.row, expected.col) - robot.translation).norm(), 0.1f) << where;
            const Maze::EdgeState seen[] = {cell.left, cell.right, cell.front}, real[] = {expected.left, expected.right, expected.front};
            for(int e = 0; e < 3; ++e)
            {
              ++edges;
              known += seen[e] != Maze::EdgeState::unknown;
              walls += seen[e] == Maze::EdgeState::wall;
              if(seen[e] != Maze::EdgeState::unknown && seen[e] != real[e])
              {
                ++contradictions;
                ADD_FAILURE() << where << ", ahead " << cell.ahead << ", edge " << e << ": seen " << static_cast<int>(seen[e]) << ", real " << static_cast<int>(real[e]);
              }
            }
          }
          EXPECT_EQ(result.segments.size(), static_cast<std::size_t>(walls)) << where;
        }
  }
  EXPECT_EQ(contradictions, 0);
  EXPECT_GE(known, edges * 9 / 10); // edges out of the field of view or too short to support stay unknown
}

GTEST_TEST(MazeWallView, CorridorWithSideOpeningEqualsOracle)
{
  // Gaze east from (4,1): walls on both sides, an opening to the north at (4,2), a dead end at (4,3).
  Maze::MazeGraph maze;
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
      for(int h = 0; h < Maze::headingCount; ++h)
        maze.set(row, col, static_cast<Maze::Heading>(h), Maze::EdgeState::open);
  for(const int col : {1, 3})
    maze.set(4, col, Maze::Heading::north, Maze::EdgeState::wall);
  for(const int col : {1, 2, 3})
    maze.set(4, col, Maze::Heading::south, Maze::EdgeState::wall);
  maze.set(4, 3, Maze::Heading::east, Maze::EdgeState::wall);

  for(const float noise : {0.f, 50.f})
  {
    const Pose2f robot(0.f, Oracle::cellCentreOf(4, 1));
    const Oracle::View truth = oracleView(maze, robot, 0.f, 3);
    const Result result = viewOf(boundaryPoints(maze, robot, 0.f, 1.4f, noise, 20.f), robot, 0.f, 3);
    ASSERT_EQ(truth.cells.size(), 3u);
    ASSERT_EQ(result.cellsAhead.size(), truth.cells.size()) << noise;
    for(std::size_t i = 0; i < truth.cells.size(); ++i)
    {
      EXPECT_EQ(static_cast<int>(result.cellsAhead[i].left), static_cast<int>(truth.cells[i].left)) << "noise " << noise << ", ahead " << i;
      EXPECT_EQ(static_cast<int>(result.cellsAhead[i].right), static_cast<int>(truth.cells[i].right)) << "noise " << noise << ", ahead " << i;
      EXPECT_EQ(static_cast<int>(result.cellsAhead[i].front), static_cast<int>(truth.cells[i].front)) << "noise " << noise << ", ahead " << i;
      EXPECT_NEAR(result.cellsAhead[i].distance, 500.f * static_cast<float>(i), 0.1f);
    }
    EXPECT_EQ(result.lookAheadTurns, 0);
    EXPECT_NEAR(result.frontDistance, 1250.f, 0.1f);
    EXPECT_NEAR(result.leftDistance, 250.f, 0.1f);
    EXPECT_NEAR(result.rightDistance, 250.f, 0.1f);
    EXPECT_EQ(result.segments.size(), 6u); // the left wall of two cells, the right wall of three and the front wall
  }
}

GTEST_TEST(MazeWallView, PointsOffTheGridAreDropped)
{
  // A wall in front of the robot's cell, points along it, offset along the gaze.
  const Pose2f robot(0.f, Oracle::cellCentreOf(3, 1));
  const auto wallPoints = [&](float offset)
  {
    std::vector<Vector2f> points;
    for(int i = -5; i <= 5; ++i)
      points.emplace_back(250.f + offset, 20.f * static_cast<float>(i));
    return points;
  };
  EXPECT_EQ(viewOf(wallPoints(0.f), robot, 0.f, 2).cellsAhead[0].front, Maze::EdgeState::wall);
  EXPECT_EQ(viewOf(wallPoints(-140.f), robot, 0.f, 2).cellsAhead[0].front, Maze::EdgeState::wall);
  EXPECT_NE(viewOf(wallPoints(200.f), robot, 0.f, 2).cellsAhead[0].front, Maze::EdgeState::wall);
  EXPECT_NE(viewOf(wallPoints(-200.f), robot, 0.f, 2).cellsAhead[0].front, Maze::EdgeState::wall);
  // Two points are not enough, the support threshold is three.
  EXPECT_NE(viewOf({Vector2f(250.f, 0.f), Vector2f(250.f, 20.f)}, robot, 0.f, 2).cellsAhead[0].front, Maze::EdgeState::wall);
}

GTEST_TEST(MazeWallView, NothingBeyondTheRange)
{
  // A wall 3.25 cells ahead: it only shows once the range reaches it.
  const Pose2f robot(0.f, Oracle::cellCentreOf(3, 0));
  std::vector<Vector2f> points;
  for(int i = -5; i <= 5; ++i)
    points.emplace_back(250.f + 3.f * 500.f, 40.f * static_cast<float>(i));
  for(int range = 0; range < 3; ++range)
  {
    const Result result = viewOf(points, robot, 0.f, range);
    ASSERT_EQ(result.cellsAhead.size(), static_cast<std::size_t>(range + 1)) << range;
    EXPECT_EQ(result.cellsAhead.back().ahead, range);
    for(const Cell& cell : result.cellsAhead)
      EXPECT_NE(cell.front, Maze::EdgeState::wall) << "range " << range << ", ahead " << cell.ahead;
    EXPECT_EQ(result.frontDistance, -1.f);
  }
  const Result result = viewOf(points, robot, 0.f, 3);
  ASSERT_EQ(result.cellsAhead.size(), 4u);
  EXPECT_EQ(result.cellsAhead.back().front, Maze::EdgeState::wall);
  EXPECT_NEAR(result.frontDistance, 1750.f, 0.1f);
}

GTEST_TEST(MazeWallView, WalkStopsAfterTheFirstFrontWall)
{
  // Walls at 1.25 and 2.25 cells ahead in the same corridor: the view ends at the first one.
  const Pose2f robot(0.f, Oracle::cellCentreOf(3, 0));
  std::vector<Vector2f> points;
  for(const float x : {250.f + 500.f, 250.f + 1000.f})
    for(int i = -5; i <= 5; ++i)
      points.emplace_back(x, 40.f * static_cast<float>(i));
  const Result result = viewOf(points, robot, 0.f, 3);
  ASSERT_EQ(result.cellsAhead.size(), 2u);
  EXPECT_EQ(result.cellsAhead[0].front, Maze::EdgeState::open);
  EXPECT_EQ(result.cellsAhead[1].front, Maze::EdgeState::wall);
  EXPECT_NEAR(result.frontDistance, 750.f, 0.1f);
}

GTEST_TEST(MazeWallView, GazeInQuarterTurns)
{
  const Pose2f robot(0.2f, Oracle::cellCentreOf(3, 3));
  const std::pair<float, int> gazes[] = {{0.f, 0}, {pi / 2.f, 1}, {-pi / 2.f, -1}, {pi, 2}, {-pi, 2}, {0.3f, 0}, {1.3f, 1}, {-1.8f, -1}};
  for(const auto& [yaw, turns] : gazes)
    EXPECT_EQ(viewOf({}, robot, yaw, 1).lookAheadTurns, turns) << yaw;

  // The robot looks east, the camera a quarter turn to the left: the wall to the left of the robot is in front of the gaze.
  const Pose2f eastRobot(0.f, Oracle::cellCentreOf(3, 3));
  std::vector<Vector2f> points;
  for(int i = -5; i <= 5; ++i)
    points.emplace_back(40.f * static_cast<float>(i), 250.f);
  const Result result = viewOf(points, eastRobot, pi / 2.f, 1);
  EXPECT_EQ(result.lookAheadTurns, 1);
  EXPECT_EQ(result.cellsAhead[0].front, Maze::EdgeState::wall);
  EXPECT_NEAR(result.frontDistance, 250.f, 0.1f);
  ASSERT_EQ(result.segments.size(), 1u);
  EXPECT_NEAR(result.segments[0].start.y(), 250.f, 0.1f);
  EXPECT_NEAR(result.segments[0].end.y(), 250.f, 0.1f);
  EXPECT_NEAR(std::abs(result.segments[0].start.x() - result.segments[0].end.x()), 500.f, 0.1f);
}
