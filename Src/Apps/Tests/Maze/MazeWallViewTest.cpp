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

  /** What a camera sees: the floor points of the wall boundary and the farthest floor points of the rays that hit no wall in reach. */
  struct Sensed
  {
    std::vector<Vector2f> boundary, clear;
  };

  /**
   * Casts rays from the camera over `halfFov` either side of the gaze, every 0.01 rad, against the walls of the maze. A ray
   * that hits a wall within `maxSeeMm` gives a boundary point, with noise of at most `noiseMm`; otherwise it gives a clear
   * point at `maxSeeMm`. In the robot frame.
   */
  Sensed castRays(const Maze::MazeGraph& maze, const Pose2f& robot, float gazeYaw, float halfFov, float noiseMm, float maxSeeMm = 3000.f)
  {
    struct Wall
    {
      Vector2f a, b;
    };
    std::vector<Wall> walls;
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
          walls.push_back({middle - along * Oracle::halfCellMm, middle + along * Oracle::halfCellMm});
        }

    Noise noise;
    Sensed sensed;
    const Vector2f camera = robot.translation + Oracle::rotated(cameraInRobot, robot.rotation);
    for(float angle = -halfFov; angle <= halfFov; angle += 0.01f)
    {
      const Vector2f direction = Oracle::rotated(Vector2f(1.f, 0.f), robot.rotation + gazeYaw + angle);
      float nearest = 1e9f;
      for(const Wall& wall : walls)
      {
        const Vector2f s = wall.b - wall.a;
        const float denominator = direction.x() * s.y() - direction.y() * s.x();
        if(std::abs(denominator) < 1e-6f)
          continue;
        const Vector2f d = wall.a - camera;
        const float t = (d.x() * s.y() - d.y() * s.x()) / denominator, u = (d.x() * direction.y() - d.y() * direction.x()) / denominator;
        if(t > 0.f && u >= 0.f && u <= 1.f)
          nearest = std::min(nearest, t);
      }
      const auto toRobot = [&](const Vector2f& point) { return Oracle::rotated(point - robot.translation, -robot.rotation); };
      if(nearest <= maxSeeMm)
        sensed.boundary.push_back(toRobot(camera + direction * nearest + noise.disc(noiseMm)));
      else
        sensed.clear.push_back(toRobot(camera + direction * maxSeeMm));
    }
    return sensed;
  }

  Oracle::View oracleView(const Maze::MazeGraph& maze, const Pose2f& robot, float gazeYaw, int range)
  {
    Oracle::View view;
    EXPECT_TRUE(Oracle::computeView(maze, robot, {cameraInRobot, gazeYaw, 1.f}, range, view));
    return view;
  }

  Result viewOf(const std::vector<Vector2f>& points, const Pose2f& robot, float gazeYaw, int range, const Params& params = Params(),
                const std::vector<Vector2f>& clearPoints = {})
  {
    Result result;
    EXPECT_TRUE(computeWallView(points, clearPoints, robot, cameraInRobot, gazeYaw, range, params, result));
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
  EXPECT_FALSE(computeWallView({}, {}, Pose2f(0.f, Vector2f(-100.f, 100.f)), cameraInRobot, 0.f, 2, Params(), result));
  EXPECT_TRUE(result.cellsAhead.empty());
}

namespace
{
  struct Comparison
  {
    int edges = 0, known = 0, contradictions = 0;
  };

  /** Compares the views of three mazes with the oracle; `useClearPoints` also gives the clear points of the rays that see no wall. */
  Comparison compareWithOracle(bool useClearPoints)
  {
    Comparison comparison;
    int& edges = comparison.edges;
    int& known = comparison.known;
    int& contradictions = comparison.contradictions;
    for(const int number : {1, 2, 3})
  {
    const Maze::MazeGraph maze = loadMaze(number);
    for(const auto& [row, col] : {std::pair(0, 5), std::pair(2, 3), std::pair(4, 1), std::pair(5, 6), std::pair(7, 2)})
      for(const float yaw : {0.f, pi / 2.f, pi})
        for(const float turn : {0.f, pi / 2.f, -pi / 2.f, pi})
        {
          const Pose2f robot(turn + 0.1f, Oracle::cellCentreOf(row, col) + Vector2f(30.f, -40.f));
          const Oracle::View truth = oracleView(maze, robot, yaw, 3);
          const Sensed sensed = castRays(maze, robot, yaw, 1.4f, 50.f, 2000.f);
          const Result result = viewOf(sensed.boundary, robot, yaw, 3, Params(), useClearPoints ? sensed.clear : std::vector<Vector2f>());
          const std::string where = "maze " + std::to_string(number) + " cell " + std::to_string(row) + "," + std::to_string(col) +
                                    " yaw " + std::to_string(yaw) + " turn " + std::to_string(turn);

          // The same gaze and the same cells; an unknown front edge ends the walk early.
          EXPECT_EQ(result.lookAheadTurns, static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(truth.gaze) - robot.rotation) / (pi / 2.f))) < -1 ?
                    static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(truth.gaze) - robot.rotation) / (pi / 2.f))) + 4 :
                    static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(truth.gaze) - robot.rotation) / (pi / 2.f)))) << where;
          if(result.cellsAhead.size() > truth.cells.size())
          {
            ADD_FAILURE() << where << ": more cells than the oracle";
            continue;
          }
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
    return comparison;
  }
}

GTEST_TEST(MazeWallView, ComparisonWithOracle)
{
  const Comparison without = compareWithOracle(false), with = compareWithOracle(true);
  std::printf("known edges: %d of %d without clear points, %d with\n", without.known, without.edges, with.known);
  EXPECT_EQ(without.contradictions, 0);
  EXPECT_EQ(with.contradictions, 0);
  EXPECT_GT(with.known, without.known); // the clear rays show the open edges of long corridors
  EXPECT_GE(with.known, with.edges * 9 / 10); // edges out of the field of view or too short to support stay unknown
}

GTEST_TEST(MazeWallView, CorridorWithSideOpeningEqualsOracle)
{
  // Gaze east from (6,1) (outside the goal block, whose inner edges are never walls): walls on both sides, an opening to the north at (6,2), a dead end at (6,3).
  Maze::MazeGraph maze;
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
      for(int h = 0; h < Maze::headingCount; ++h)
        maze.set(row, col, static_cast<Maze::Heading>(h), Maze::EdgeState::open);
  for(const int col : {1, 3})
    maze.set(6, col, Maze::Heading::north, Maze::EdgeState::wall);
  for(const int col : {1, 2, 3})
    maze.set(6, col, Maze::Heading::south, Maze::EdgeState::wall);
  maze.set(6, 3, Maze::Heading::east, Maze::EdgeState::wall);

  for(const float noise : {0.f, 50.f})
  {
    const Pose2f robot(0.f, Oracle::cellCentreOf(6, 1));
    const Oracle::View truth = oracleView(maze, robot, 0.f, 3);
    const Sensed sensed = castRays(maze, robot, 0.f, 1.4f, noise);
    const Result result = viewOf(sensed.boundary, robot, 0.f, 3, Params(), sensed.clear);
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

GTEST_TEST(MazeWallView, LongOpenCorridorIsOpenWithinRangeFromClearRays)
{
  // A corridor along row 6 with a wall on both sides and the border far behind the sight of 2500 mm.
  Maze::MazeGraph maze;
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
      for(int h = 0; h < Maze::headingCount; ++h)
        maze.set(row, col, static_cast<Maze::Heading>(h), Maze::EdgeState::open);
  for(int col = 0; col < Maze::size; ++col)
  {
    maze.set(6, col, Maze::Heading::north, Maze::EdgeState::wall);
    maze.set(6, col, Maze::Heading::south, Maze::EdgeState::wall);
  }

  const Pose2f robot(0.f, Oracle::cellCentreOf(6, 0));
  const Oracle::View truth = oracleView(maze, robot, 0.f, 3);
  ASSERT_EQ(truth.cells.size(), 4u);
  // Looking straight down the corridor all rays see no wall in reach: no boundary points, so only the clear rays tell
  // that the fronts are open.
  const Sensed narrow = castRays(maze, robot, 0.f, 0.05f, 0.f, 2500.f);
  ASSERT_TRUE(narrow.boundary.empty());
  ASSERT_FALSE(narrow.clear.empty());
  EXPECT_EQ(viewOf(narrow.boundary, robot, 0.f, 3).cellsAhead.size(), 1u);
  const Result straight = viewOf(narrow.boundary, robot, 0.f, 3, Params(), narrow.clear);
  ASSERT_EQ(straight.cellsAhead.size(), 4u);
  for(const Cell& cell : straight.cellsAhead)
    EXPECT_EQ(cell.front, Maze::EdgeState::open) << cell.ahead;

  // With the wide field of view the view equals the oracle's.
  const Sensed sensed = castRays(maze, robot, 0.f, 1.4f, 0.f, 2500.f);
  const Result result = viewOf(sensed.boundary, robot, 0.f, 3, Params(), sensed.clear);
  ASSERT_EQ(result.cellsAhead.size(), 4u);
  for(std::size_t i = 0; i < 4; ++i)
  {
    EXPECT_EQ(static_cast<int>(result.cellsAhead[i].left), static_cast<int>(truth.cells[i].left)) << i;
    EXPECT_EQ(static_cast<int>(result.cellsAhead[i].right), static_cast<int>(truth.cells[i].right)) << i;
    EXPECT_EQ(static_cast<int>(result.cellsAhead[i].front), static_cast<int>(truth.cells[i].front)) << i;
  }
  // The range still limits the look-ahead.
  EXPECT_EQ(viewOf(sensed.boundary, robot, 0.f, 1, Params(), sensed.clear).cellsAhead.size(), 2u);
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
  const Pose2f robot(0.f, Oracle::cellCentreOf(1, 0));
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
  const Pose2f robot(0.f, Oracle::cellCentreOf(1, 0));
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

GTEST_TEST(MazeWallView, LookAheadRangeFollowsTheMotion)
{
  EXPECT_EQ(lookAheadRange(true, 2, 3), 3);
  EXPECT_EQ(lookAheadRange(false, 2, 3), 2);
}

GTEST_TEST(MazeWallView, NoWallsBetweenTheCellsOfTheGoalBlock)
{
  // Points along the edge between the goal cells (3,3) and (3,4), which the patch edges can fake.
  const Pose2f robot(0.f, Oracle::cellCentreOf(3, 3));
  std::vector<Vector2f> inner;
  for(int i = -5; i <= 5; ++i)
    inner.emplace_back(250.f, 40.f * static_cast<float>(i));
  EXPECT_NE(viewOf(inner, robot, 0.f, 1).cellsAhead[0].front, Maze::EdgeState::wall);
  Params withoutPrior;
  withoutPrior.noGoalBlockWalls = false;
  EXPECT_EQ(viewOf(inner, robot, 0.f, 1, withoutPrior).cellsAhead[0].front, Maze::EdgeState::wall);

  // A floor seen past the edge still makes it open, the prior only forbids the wall.
  EXPECT_EQ(viewOf(inner, robot, 0.f, 1, Params(), {Vector2f(1000.f, 0.f)}).cellsAhead[0].front, Maze::EdgeState::open);

  // The edge between the goal cell and the cell outside it is an ordinary one.
  std::vector<Vector2f> outer;
  for(int i = -5; i <= 5; ++i)
    outer.emplace_back(40.f * static_cast<float>(i), 250.f);
  EXPECT_EQ(viewOf(outer, robot, pi / 2.f, 1).cellsAhead[0].front, Maze::EdgeState::wall);

  // Outside the goal block nothing changes: the wall between (3,2) and the goal cell (3,3), and the north wall of (3,2).
  const Pose2f outside(0.f, Oracle::cellCentreOf(3, 2));
  EXPECT_EQ(viewOf(outer, outside, pi / 2.f, 1).cellsAhead[0].front, Maze::EdgeState::wall);
  std::vector<Vector2f> outsideWall;
  for(int i = -5; i <= 5; ++i)
    outsideWall.emplace_back(250.f, 40.f * static_cast<float>(i));
  EXPECT_EQ(viewOf(outsideWall, outside, 0.f, 1).cellsAhead[0].front, Maze::EdgeState::wall);
}
