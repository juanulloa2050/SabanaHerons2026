#include "Tools/Maze/MazeOracle.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeTagLayout.h"
#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Streaming/InStreams.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <utility>
#include <vector>

namespace
{
  using namespace Maze::Oracle;

  constexpr float opening = 1.f;     // rad, horizontal opening angle of the test camera
  const float viewAngle = Angle::fromDegrees(70.f);

  /** A maze without inner walls: only the border is a wall. */
  Maze::MazeGraph openMaze()
  {
    Maze::MazeGraph graph;
    for(int row = 0; row < Maze::size; ++row)
      for(int col = 0; col < Maze::size; ++col)
        for(int h = 0; h < Maze::headingCount; ++h)
          graph.set(row, col, static_cast<Maze::Heading>(h), Maze::EdgeState::open);
    return graph;
  }

  /** A camera at the robot origin looking forward. */
  Camera forwardCamera()
  {
    return {Vector2f::Zero(), 0.f, opening};
  }

  /** The robot in the middle of a cell, looking east unless `rotation` says otherwise. */
  Pose2f robotIn(int row, int col, float rotation = 0.f)
  {
    return Pose2f(rotation, Maze::Oracle::cellCentreOf(row, col));
  }

  View viewOf(const Maze::MazeGraph& graph, const Pose2f& pose, int maxRange = 2, const Camera& camera = forwardCamera())
  {
    View view;
    EXPECT_TRUE(computeView(graph, pose, camera, maxRange, view));
    return view;
  }

  /** Registers the streamable types (enums) once; the application does this at start-up. */
  Maze::MazeTagLayout layoutFrom(const std::string& mode, const std::string& faces)
  {
    static const bool registered = (FunctionList::execute(), true);
    (void) registered;
    const std::string text = "mode = " + mode + "; faces = [" + faces + "]; walls = []; centreHeight = 300; size = 150;";
    Maze::MazeTagLayout layout;
    InMapMemory stream(text.c_str(), text.size());
    stream >> layout;
    return layout;
  }

  using Seen = std::vector<std::pair<int, Maze::Heading>>;

  /** The (cell, face) pairs the oracle sees, like the provider does without dropout. */
  Seen seenTags(const Maze::MazeGraph& graph, const Maze::MazeTagLayout& layout, const View& view,
                float maxTagDistance, float maxTagViewAngle, const Camera& camera = forwardCamera())
  {
    Seen seen;
    for(const VisibleTag& tag : visibleTags(graph, layout, view, camera, maxTagDistance, maxTagViewAngle))
      seen.emplace_back(tag.cell, tag.face);
    return seen;
  }

  Maze::MazeTagLayout layoutFromAllFaces()
  {
    return layoutFrom("allFaces", "north, east, south, west");
  }

  bool contains(const Seen& seen, int cell, Maze::Heading face)
  {
    return std::find(seen.begin(), seen.end(), std::make_pair(cell, face)) != seen.end();
  }
}

GTEST_TEST(MazeOracle, PoseToCell)
{
  int row = -1, col = -1;
  ASSERT_TRUE(cellContaining(Vector2f(250.f, 1750.f), row, col));
  EXPECT_EQ(row, 4);
  EXPECT_EQ(col, 0);
  // The top-left cell holds the north-west corner of the maze; y grows towards the north.
  ASSERT_TRUE(cellContaining(Vector2f(1.f, 3999.f), row, col));
  EXPECT_EQ(row, 0);
  EXPECT_EQ(col, 0);
  ASSERT_TRUE(cellContaining(Vector2f(3999.f, 1.f), row, col));
  EXPECT_EQ(row, Maze::size - 1);
  EXPECT_EQ(col, Maze::size - 1);
  // The centre of a cell converts back to the cell.
  for(int r = 0; r < Maze::size; ++r)
    for(int c = 0; c < Maze::size; ++c)
    {
      int backRow = -1, backCol = -1;
      ASSERT_TRUE(cellContaining(cellCentreOf(r, c), backRow, backCol));
      EXPECT_EQ(backRow, r);
      EXPECT_EQ(backCol, c);
    }
}

GTEST_TEST(MazeOracle, PoseOutsideTheMazeHasNoCell)
{
  int row = 0, col = 0;
  EXPECT_FALSE(cellContaining(Vector2f(-1.f, 1000.f), row, col));
  EXPECT_FALSE(cellContaining(Vector2f(1000.f, -1.f), row, col));
  EXPECT_FALSE(cellContaining(Vector2f(4000.f, 1000.f), row, col));
  EXPECT_FALSE(cellContaining(Vector2f(1000.f, 4000.f), row, col));

  View view;
  EXPECT_FALSE(computeView(openMaze(), Pose2f(0.f, Vector2f(-300.f, 1750.f)), forwardCamera(), 2, view));
  EXPECT_FALSE(computeView(openMaze(), Pose2f(0.f, Vector2f(5000.f, 5000.f)), forwardCamera(), 2, view));
}

GTEST_TEST(MazeOracle, GazeIsSnappedToTheNearestHeading)
{
  EXPECT_EQ(snapToHeading(0.f), Maze::Heading::east);
  EXPECT_EQ(snapToHeading(0.4f), Maze::Heading::east);
  EXPECT_EQ(snapToHeading(1.2f), Maze::Heading::north);
  EXPECT_EQ(snapToHeading(pi), Maze::Heading::west);
  EXPECT_EQ(snapToHeading(-1.5f), Maze::Heading::south);
  for(int h = 0; h < Maze::headingCount; ++h)
    EXPECT_EQ(snapToHeading(angleOf(static_cast<Maze::Heading>(h))), static_cast<Maze::Heading>(h));

  const Maze::MazeGraph graph = openMaze();
  EXPECT_EQ(viewOf(graph, robotIn(4, 3, 0.3f)).gaze, Maze::Heading::east);
  EXPECT_EQ(viewOf(graph, robotIn(4, 3, pi / 2.f + 0.2f)).gaze, Maze::Heading::north);
  // The camera yaw adds to the robot rotation: a robot looking east with the head turned left looks north.
  EXPECT_EQ(viewOf(graph, robotIn(4, 3), 2, Camera{Vector2f::Zero(), pi / 2.f, opening}).gaze, Maze::Heading::north);
}

GTEST_TEST(MazeOracle, LookAheadStopsAtMaxRange)
{
  const Maze::MazeGraph graph = openMaze();
  const View view = viewOf(graph, robotIn(4, 0), 2);
  EXPECT_EQ(view.row, 4);
  EXPECT_EQ(view.col, 0);
  ASSERT_EQ(view.cells.size(), 3u); // the robot's cell and two cells ahead
  for(int ahead = 0; ahead < 3; ++ahead)
  {
    EXPECT_EQ(view.cells[static_cast<std::size_t>(ahead)].ahead, ahead);
    EXPECT_EQ(view.cells[static_cast<std::size_t>(ahead)].row, 4);
    EXPECT_EQ(view.cells[static_cast<std::size_t>(ahead)].col, ahead);
  }
  EXPECT_EQ(viewOf(graph, robotIn(4, 0), 0).cells.size(), 1u);
  EXPECT_EQ(viewOf(graph, robotIn(4, 0), 5).cells.size(), 6u);
}

GTEST_TEST(MazeOracle, LookAheadStopsAtAWall)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  const View view = viewOf(graph, robotIn(4, 0), 5);
  ASSERT_EQ(view.cells.size(), 2u);
  EXPECT_EQ(view.cells[0].front, Maze::EdgeState::open);
  EXPECT_EQ(view.cells[1].front, Maze::EdgeState::wall);
  EXPECT_EQ(view.cells[1].col, 1);

  // Looking at the border ends the walk at the last cell.
  const View atBorder = viewOf(graph, robotIn(4, 7), 5);
  ASSERT_EQ(atBorder.cells.size(), 1u);
  EXPECT_EQ(atBorder.cells[0].front, Maze::EdgeState::wall);
}

GTEST_TEST(MazeOracle, EdgesAreRelativeToTheGaze)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 0, Maze::Heading::north, Maze::EdgeState::wall);
  // Looking east, north is on the left.
  const View east = viewOf(graph, robotIn(4, 0), 0);
  EXPECT_EQ(east.cells[0].left, Maze::EdgeState::wall);
  EXPECT_EQ(east.cells[0].right, Maze::EdgeState::open);
  // Looking south, north is behind and east is on the left.
  const View south = viewOf(graph, robotIn(4, 0, -pi / 2.f), 0);
  EXPECT_EQ(south.gaze, Maze::Heading::south);
  EXPECT_EQ(south.cells[0].left, Maze::EdgeState::open);
  EXPECT_EQ(south.cells[0].right, Maze::EdgeState::wall); // west, the border
}

GTEST_TEST(MazeOracle, NothingIsSeenPastACorner)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 0, Maze::Heading::east, Maze::EdgeState::wall);
  const View view = viewOf(graph, robotIn(4, 0), 3);
  ASSERT_EQ(view.cells.size(), 1u);
  const Maze::MazeTagLayout layout = layoutFrom("allFaces", "north, east, south, west");
  // Only the wall in front is seen; the tags behind it are hidden by it.
  const auto seen = seenTags(graph, layout, view, 1750.f, viewAngle);
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(seen[0], std::make_pair(Maze::cellId(4, 0), Maze::Heading::east));
}

GTEST_TEST(MazeOracle, SeesTheTagOnTheFrontWall)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  const View view = viewOf(graph, robotIn(4, 0), 2);

  Pose3f pose;
  const int cell = Maze::cellId(4, 1);
  ASSERT_TRUE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, pose));
  // The wall is 750 mm ahead of the robot's cell centre, 300 mm above the floor.
  EXPECT_NEAR(pose.translation.x(), 750.f, 0.01f);
  EXPECT_NEAR(pose.translation.y(), 0.f, 0.01f);
  EXPECT_FLOAT_EQ(pose.translation.z(), 300.f);
  // z points out of the wall towards the robot, y up, x to the right of an observer in front of the tag.
  EXPECT_LT((pose.rotation * Vector3f(0.f, 0.f, 1.f) - Vector3f(-1.f, 0.f, 0.f)).norm(), 1e-4f);
  EXPECT_LT((pose.rotation * Vector3f(0.f, 1.f, 0.f) - Vector3f(0.f, 0.f, 1.f)).norm(), 1e-4f);
  EXPECT_LT((pose.rotation * Vector3f(1.f, 0.f, 0.f) - Vector3f(0.f, -1.f, 0.f)).norm(), 1e-4f);

  const auto seen = seenTags(graph, layoutFrom("allFaces", "north, east, south, west"), view, 1250.f, viewAngle);
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(seen[0], std::make_pair(cell, Maze::Heading::east));
}

GTEST_TEST(MazeOracle, TagPoseFollowsTheRobotPose)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  // 100 mm north of the cell centre and turned to look south, with the camera turned to the left.
  const Pose2f pose(-pi / 2.f, cellCentreOf(4, 1) + Vector2f(0.f, 100.f));
  View view;
  ASSERT_TRUE(computeView(graph, pose, Camera{Vector2f::Zero(), pi / 2.f, 2.f}, 2, view));
  Pose3f tag;
  ASSERT_TRUE(observeTag(view, Camera{Vector2f::Zero(), pi / 2.f, 2.f}, 1250.f, viewAngle, Maze::cellId(4, 1), Maze::Heading::east, 300.f, tag));
  // Robot looks south, so the east wall is 100 mm ahead and 250 mm to its left (+y).
  EXPECT_NEAR(tag.translation.x(), 100.f, 0.01f);
  EXPECT_NEAR(tag.translation.y(), 250.f, 0.01f);
}

GTEST_TEST(MazeOracle, TagOutsideTheFieldOfViewIsNotSeen)
{
  const Maze::MazeGraph graph = openMaze();
  // Looking north from a cell whose east face is a wall: the tag is 90 degrees to the right.
  const Pose2f pose = robotIn(4, 0, pi / 2.f);
  const View view = viewOf(graph, pose, 2);
  Pose3f tag;
  const int cell = Maze::cellId(4, 0);
  EXPECT_FALSE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
  // A wide enough camera would see it.
  EXPECT_TRUE(observeTag(view, Camera{Vector2f::Zero(), 0.f, 3.2f}, 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
  // The camera yaw counts: turning the camera right brings it into view.
  EXPECT_TRUE(observeTag(view, Camera{Vector2f::Zero(), -pi / 2.f, opening}, 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
}

GTEST_TEST(MazeOracle, TagSeenTooObliquelyIsNotSeen)
{
  const Maze::MazeGraph graph = openMaze();
  // In the cell south of the tag's cell, 490 mm from the wall in x, looking north: the angle between the tag
  // normal and the direction to the camera is about 88.9 degrees.
  const Pose2f pose(pi / 2.f, Vector2f(990.f, 1250.f));
  const View view = viewOf(graph, pose, 2);
  Pose3f tag;
  const int cell = Maze::cellId(4, 1);
  EXPECT_FALSE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
  EXPECT_TRUE(observeTag(view, forwardCamera(), 1250.f, Angle::fromDegrees(120.f), cell, Maze::Heading::east, 300.f, tag));
}

GTEST_TEST(MazeOracle, TagBeyondTheMaximumRangeIsNotSeen)
{
  const Maze::MazeGraph graph = openMaze();
  const View view = viewOf(graph, robotIn(4, 0), 1);
  Pose3f tag;
  // The east face of cell (4, 2) is 1250 mm from the camera.
  const int cell = Maze::cellId(4, 2);
  EXPECT_FALSE(observeTag(view, forwardCamera(), 750.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
  EXPECT_TRUE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
}

GTEST_TEST(MazeOracle, TagDistanceLimitIsIndependentOfTheLookAhead)
{
  // The east face of cell (4, 3) is 1750 mm from a camera at the centre of cell (4, 0) looking east.
  const Maze::MazeGraph graph = openMaze();
  const View view = viewOf(graph, robotIn(4, 0), 2);
  Pose3f tag;
  const int cell = Maze::cellId(4, 3);
  EXPECT_FALSE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
  EXPECT_TRUE(observeTag(view, forwardCamera(), 2000.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
  EXPECT_FALSE(observeTag(view, forwardCamera(), 1700.f, viewAngle, cell, Maze::Heading::east, 300.f, tag));
}

GTEST_TEST(MazeOracle, NoTagsWhenTheLayoutHasNone)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  const View view = viewOf(graph, robotIn(4, 0), 2);

  // The only wall in sight is the east one, so a layout without the east face sees nothing.
  EXPECT_TRUE(seenTags(graph, layoutFrom("faces", "north, south, west"), view, 1250.f, viewAngle).empty());
  EXPECT_TRUE(seenTags(graph, layoutFrom("faces", ""), view, 1250.f, viewAngle).empty());
  const auto east = seenTags(graph, layoutFrom("faces", "east"), view, 1250.f, viewAngle);
  ASSERT_EQ(east.size(), 1u);
  EXPECT_EQ(east[0], std::make_pair(Maze::cellId(4, 1), Maze::Heading::east));
}

GTEST_TEST(MazeOracle, GeneratedMazeDeadEnd)
{
  Maze::MazeDescription maze;
  std::string error;
  ASSERT_TRUE(Maze::MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_1.maze", maze, error)) << error;

  // The first cell (not a corner start) with a single opening is a dead end.
  int deadEnd = -1;
  Maze::Heading opening = Maze::Heading::north;
  for(int cell = 0; cell < Maze::cellCount && deadEnd < 0; ++cell)
  {
    if(cell == maze.startCell)
      continue;
    int openSides = 0;
    for(int h = 0; h < Maze::headingCount; ++h)
      if(maze.graph.get(Maze::cellRow(cell), Maze::cellCol(cell), static_cast<Maze::Heading>(h)) == Maze::EdgeState::open)
      {
        ++openSides;
        opening = static_cast<Maze::Heading>(h);
      }
    if(openSides == 1)
      deadEnd = cell;
  }
  ASSERT_GE(deadEnd, 0);

  // Standing in the dead end and looking at the wall opposite of the opening, the look-ahead is the
  // cell alone and the tag on the wall in front is seen; the face towards the opening has no tag.
  const Maze::Heading front = Maze::turned(opening, 2);
  const View view = viewOf(maze.graph, Pose2f(angleOf(front), cellCentreOf(Maze::cellRow(deadEnd), Maze::cellCol(deadEnd))), 3);
  ASSERT_EQ(view.cells.size(), 1u);
  const auto seen = seenTags(maze.graph, layoutFrom("allFaces", "north, east, south, west"), view, 1750.f, viewAngle);
  ASSERT_FALSE(seen.empty());
  bool frontSeen = false;
  for(const auto& [id, face] : seen)
  {
    EXPECT_EQ(id, deadEnd);
    EXPECT_NE(face, opening);
    frontSeen = frontSeen || face == front;
  }
  EXPECT_TRUE(frontSeen);
}

namespace
{
  /** The maze pose for a first-team robot: SimRobot rotates its ground truth by pi. */
  const Pose2f firstTeamMazePose(pi, Vector2f(2000.f, 2000.f));
  /** The maze pose for a second-team robot: the ground truth is the simulator pose. */
  const Pose2f secondTeamMazePose(0.f, Vector2f(-2000.f, -2000.f));

  /** What the simulator reports for a robot of the first team standing at `simulatorPose`. */
  Pose2f firstTeamGroundTruth(const Pose2f& simulatorPose)
  {
    return Pose2f(pi) + simulatorPose;
  }

  void expectCellAndHeading(const Pose2f& mazePose, const Pose2f& groundTruth, int row, int col, Maze::Heading heading)
  {
    View view;
    ASSERT_TRUE(computeView(openMaze(), robotInMaze(groundTruth, mazePose), forwardCamera(), 2, view));
    EXPECT_EQ(view.row, row);
    EXPECT_EQ(view.col, col);
    EXPECT_EQ(view.gaze, heading);
  }
}

GTEST_TEST(MazeOracle, FirstTeamGroundTruthInAllCorners)
{
  // Simulator poses of the generated scene: the maze is centred on the origin, cell (0, 0) is at (-1750, 1750).
  expectCellAndHeading(firstTeamMazePose, firstTeamGroundTruth(Pose2f(-pi / 2.f, -1750.f, 1750.f)), 0, 0, Maze::Heading::south);
  expectCellAndHeading(firstTeamMazePose, firstTeamGroundTruth(Pose2f(0.f, 1750.f, 1750.f)), 0, 7, Maze::Heading::east);
  expectCellAndHeading(firstTeamMazePose, firstTeamGroundTruth(Pose2f(pi / 2.f, -1750.f, -1750.f)), 7, 0, Maze::Heading::north);
  expectCellAndHeading(firstTeamMazePose, firstTeamGroundTruth(Pose2f(pi, 1750.f, -1750.f)), 7, 7, Maze::Heading::west);
}

GTEST_TEST(MazeOracle, SecondTeamGroundTruthInAllCorners)
{
  expectCellAndHeading(secondTeamMazePose, Pose2f(-pi / 2.f, -1750.f, 1750.f), 0, 0, Maze::Heading::south);
  expectCellAndHeading(secondTeamMazePose, Pose2f(0.f, 1750.f, 1750.f), 0, 7, Maze::Heading::east);
  expectCellAndHeading(secondTeamMazePose, Pose2f(pi / 2.f, -1750.f, -1750.f), 7, 0, Maze::Heading::north);
  expectCellAndHeading(secondTeamMazePose, Pose2f(pi, 1750.f, -1750.f), 7, 7, Maze::Heading::west);
}

GTEST_TEST(MazeOracle, RobotInMazeInverseOfMazePose)
{
  const Pose2f inMaze = robotInMaze(firstTeamGroundTruth(Pose2f(-pi / 2.f, -1750.f, 1750.f)), firstTeamMazePose);
  EXPECT_NEAR(inMaze.translation.x(), 250.f, 0.01f);
  EXPECT_NEAR(inMaze.translation.y(), 3750.f, 0.01f);
  EXPECT_NEAR(Angle::normalize(inMaze.rotation - angleOf(Maze::Heading::south)), 0.f, 1e-4f);
}

GTEST_TEST(MazeOracle, GeneratedMazeStartCorridor)
{
  Maze::MazeDescription maze;
  std::string error;
  ASSERT_TRUE(Maze::MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_1.maze", maze, error)) << error;
  ASSERT_EQ(maze.startCell, 0);

  // maze_1: robot1 starts in cell 0 facing south; the simulator pose is (-1750, 1750), -90deg.
  const Pose2f groundTruth = firstTeamGroundTruth(Pose2f(-pi / 2.f, -1750.f, 1750.f));
  EXPECT_NEAR(groundTruth.translation.x(), 1750.f, 0.01f);
  EXPECT_NEAR(groundTruth.translation.y(), -1750.f, 0.01f);
  EXPECT_NEAR(Angle::normalize(groundTruth.rotation - pi / 2.f), 0.f, 1e-4f);

  View view;
  ASSERT_TRUE(computeView(maze.graph, robotInMaze(groundTruth, firstTeamMazePose), forwardCamera(), 3, view));
  EXPECT_EQ(view.gaze, Maze::Heading::south);
  ASSERT_EQ(view.cells.size(), 3u); // cells 0, 8 and 16; the south face of cell 16 is a wall
  const int expected[3] = {0, 8, 16};
  for(std::size_t i = 0; i < 3; ++i)
    EXPECT_EQ(Maze::cellId(view.cells[i].row, view.cells[i].col), expected[i]);
  EXPECT_EQ(view.cells[0].front, Maze::EdgeState::open);
  EXPECT_EQ(view.cells[1].front, Maze::EdgeState::open);
  EXPECT_EQ(view.cells[2].front, Maze::EdgeState::wall);
}

GTEST_TEST(MazeOracle, SeesBothTagsOfTheGoalWall)
{
  Maze::MazeDescription maze;
  std::string error;
  ASSERT_TRUE(Maze::MazeFile::load(std::string(File::getBHDir()) + "/Config/Mazes/generated/maze_1.maze", maze, error)) << error;
  // The goal block (cells 27, 28, 35, 36) has no inner walls and a wall to its south that carries tags 35 and 36.
  ASSERT_EQ(maze.graph.get(4, 3, Maze::Heading::south), Maze::EdgeState::wall);
  ASSERT_EQ(maze.graph.get(4, 4, Maze::Heading::south), Maze::EdgeState::wall);
  ASSERT_EQ(maze.graph.get(3, 3, Maze::Heading::east), Maze::EdgeState::open);
  ASSERT_EQ(maze.graph.get(3, 3, Maze::Heading::south), Maze::EdgeState::open);
  ASSERT_EQ(maze.graph.get(3, 4, Maze::Heading::south), Maze::EdgeState::open);

  // 822 mm from the wall (y = 1500), 238 mm west of the centre of tag 36 (x = 2250), looking south: tag 36
  // is left of the camera axis, tag 35 right of it, in the neighbouring column of the gaze corridor.
  const View view = viewOf(maze.graph, Pose2f(-pi / 2.f, Vector2f(2012.f, 2322.f)), 2);
  ASSERT_EQ(view.row, 3);
  ASSERT_EQ(view.col, 4);
  ASSERT_EQ(view.gaze, Maze::Heading::south);
  const auto tags = visibleTags(maze.graph, layoutFrom("allFaces", "north, east, south, west"), view, forwardCamera(), 1250.f, viewAngle);

  int found = 0;
  for(const VisibleTag& tag : tags)
  {
    if(tag.face != Maze::Heading::south || (tag.cell != 35 && tag.cell != 36))
      continue;
    ++found;
    EXPECT_NEAR(tag.tagInRobot.translation.x(), 822.f, 0.01f);
    EXPECT_NEAR(tag.tagInRobot.translation.y(), tag.cell == 36 ? 238.f : -262.f, 0.01f);
  }
  EXPECT_EQ(found, 2);
}

GTEST_TEST(MazeOracle, TagBehindAWallIsNotSeen)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  graph.set(4, 2, Maze::Heading::east, Maze::EdgeState::wall);
  const View view = viewOf(graph, robotIn(4, 0), 2);
  const auto seen = seenTags(graph, layoutFromAllFaces(), view, 1250.f, viewAngle);
  // The second wall is in range (1250 mm) and straight ahead, but the first one hides it.
  EXPECT_TRUE(contains(seen, Maze::cellId(4, 1), Maze::Heading::east));
  EXPECT_FALSE(contains(seen, Maze::cellId(4, 2), Maze::Heading::east));
}

GTEST_TEST(MazeOracle, TagPartlyOutsideTheFieldOfViewIsNotSeen)
{
  Maze::MazeGraph graph = openMaze();
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  // The tag centre is 300 mm ahead and 150 mm to the left (26.6 degrees); its far edge is at 36.9 degrees.
  const Pose2f pose(0.f, Vector2f(700.f, 1600.f));
  const View view = viewOf(graph, pose, 2);
  const int cell = Maze::cellId(4, 1);
  Pose3f tag;
  EXPECT_TRUE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag)); // centre only: +-28.6 degrees
  EXPECT_FALSE(observeTag(view, forwardCamera(), 1250.f, viewAngle, cell, Maze::Heading::east, 300.f, tag, 150.f));
  EXPECT_FALSE(contains(seenTags(graph, layoutFromAllFaces(), view, 1250.f, viewAngle), cell, Maze::Heading::east));
  // A wider camera takes the whole tag in.
  EXPECT_TRUE(contains(seenTags(graph, layoutFromAllFaces(), view, 1250.f, viewAngle, Camera{Vector2f::Zero(), 0.f, 1.4f}), cell, Maze::Heading::east));
}

GTEST_TEST(MazeOracle, TagPastTheEndOfAWallCountsAsBlockedWhenGrazing)
{
  Maze::MazeGraph graph = openMaze();
  // A wall at x = 1000 from y = 1500 to y = 2000; the tag is on the wall at x = 2000 (cell (4, 3), east face).
  graph.set(4, 1, Maze::Heading::east, Maze::EdgeState::wall);
  graph.set(4, 3, Maze::Heading::east, Maze::EdgeState::wall);
  const int cell = Maze::cellId(4, 3);
  const auto seenFrom = [&](float y)
  {
    const View view = viewOf(graph, Pose2f(0.f, Vector2f(500.f, y)), 4);
    return contains(seenTags(graph, layoutFromAllFaces(), view, 2250.f, viewAngle), cell, Maze::Heading::east);
  };
  EXPECT_FALSE(seenFrom(1750.f)); // straight through the wall
  EXPECT_FALSE(seenFrom(2170.f)); // the ray to the near tag edge passes 5 mm past the wall end: inside the 7.5 mm margin
  EXPECT_TRUE(seenFrom(2200.f));  // all three rays clear the wall end by at least 17 mm
}
