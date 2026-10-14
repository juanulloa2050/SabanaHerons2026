/**
 * @file OracledMazePerceptsProvider.cpp
 *
 * Implementation of the maze percept oracle, see the header for the model.
 */

#include "OracledMazePerceptsProvider.h"
#include "Debugging/Debugging.h"
#include "Math/Random.h"
#include "Math/RotationMatrix.h"
#include "Platform/File.h"
#include <algorithm>
#include <cmath>

MAKE_MODULE(OracledMazePerceptsProvider);

namespace
{
  constexpr float halfCell = Maze::cellSizeMm / 2.f;
  constexpr float quarterTurn = pi / 2.f;

  /** Unit vector of a heading in the maze frame (x east, y north). */
  Vector2f directionOf(Maze::Heading heading)
  {
    switch(heading)
    {
      case Maze::Heading::north: return Vector2f(0.f, 1.f);
      case Maze::Heading::east: return Vector2f(1.f, 0.f);
      case Maze::Heading::south: return Vector2f(0.f, -1.f);
      default: return Vector2f(-1.f, 0.f);
    }
  }

  /** The heading closest to an angle in the maze frame (0 is east, counterclockwise). */
  Maze::Heading snapToHeading(float angle)
  {
    const int quarters = ((static_cast<int>(std::lround(angle / quarterTurn)) % 4) + 4) % 4; // 0 east, 1 north, 2 west, 3 south
    constexpr Maze::Heading byQuarter[4] = {Maze::Heading::east, Maze::Heading::north, Maze::Heading::west, Maze::Heading::south};
    return byQuarter[quarters];
  }

  /** The angle of a heading in the maze frame. */
  float angleOf(Maze::Heading heading)
  {
    const Vector2f direction = directionOf(heading);
    return std::atan2(direction.y(), direction.x());
  }

  Vector2f cellCentreOf(int row, int col)
  {
    const Maze::CellCentre centre = Maze::cellCentre(row, col);
    return Vector2f(centre.x, centre.y);
  }

  float noisy(float value, float stdDev)
  {
    return stdDev > 0.f ? Random::normal(value, stdDev) : value;
  }
}

OracledMazePerceptsProvider::OracledMazePerceptsProvider()
{
  if(!Maze::MazeTagLayout::load("mazeTagLayout.cfg", layout))
    OUTPUT_WARNING("OracledMazePerceptsProvider: mazeTagLayout.cfg not found, using a tag on every wall face.");
}

bool OracledMazePerceptsProvider::dropped() const
{
  return dropoutRate > 0.f && Random::bernoulli(dropoutRate);
}

bool OracledMazePerceptsProvider::updateMaze()
{
  if(mazeFile == loadedMazeFile)
    return mazeAvailable;
  loadedMazeFile = mazeFile;
  std::string error;
  mazeAvailable = Maze::MazeFile::load(std::string(File::getBHDir()) + "/" + mazeFile, maze, error);
  if(!mazeAvailable)
  {
    // Printed once per path: the path stays remembered, so the next frames do not try again.
    OUTPUT_WARNING("OracledMazePerceptsProvider: " << mazeFile << " not found or invalid (" << error
                   << "), run Util/MazeTools/maze_to_scene.py --out Config/Scenes/Includes/Maze/current.rsi2");
  }
  return mazeAvailable;
}

Vector2f OracledMazePerceptsProvider::toRobot(const View& view, const Vector2f& mazePoint) const
{
  const float c = std::cos(-view.robotRotation), s = std::sin(-view.robotRotation);
  const Vector2f d = mazePoint - view.robot;
  return Vector2f(c * d.x() - s * d.y(), s * d.x() + c * d.y());
}

bool OracledMazePerceptsProvider::computeView(View& view) const
{
  const Pose2f& pose = theGroundTruthRobotPose;
  view.robot = pose.translation - mazeOriginOnField;
  view.robotRotation = pose.rotation;
  const int col = static_cast<int>(std::floor(view.robot.x() / Maze::cellSizeMm));
  const int row = Maze::size - 1 - static_cast<int>(std::floor(view.robot.y() / Maze::cellSizeMm));
  if(view.robot.x() < 0.f || view.robot.y() < 0.f || !Maze::inside(row, col))
    return false;
  view.row = row;
  view.col = col;

  const float c = std::cos(view.robotRotation), s = std::sin(view.robotRotation);
  const Vector2f cameraInRobot = theCameraMatrix.translation.head<2>();
  view.camera = view.robot + Vector2f(c * cameraInRobot.x() - s * cameraInRobot.y(), s * cameraInRobot.x() + c * cameraInRobot.y());
  const Vector3f gazeInRobot = theCameraMatrix.rotation * Vector3f(1.f, 0.f, 0.f);
  view.cameraYaw = Angle::normalize(view.robotRotation + std::atan2(gazeInRobot.y(), gazeInRobot.x()));
  view.gaze = snapToHeading(view.cameraYaw);

  // Walk along the gaze: stop after a front wall or after maxRange cells ahead.
  view.cells.clear();
  int r = row, cl = col;
  for(int ahead = 0; ahead <= maxRange; ++ahead)
  {
    LookAheadCell cell;
    cell.ahead = ahead;
    cell.row = r;
    cell.col = cl;
    cell.left = maze.graph.get(r, cl, Maze::turned(view.gaze, -1));
    cell.right = maze.graph.get(r, cl, Maze::turned(view.gaze, 1));
    cell.front = maze.graph.get(r, cl, view.gaze);
    view.cells.push_back(cell);
    if(cell.front != Maze::EdgeState::open)
      break;
    r += Maze::rowDelta[static_cast<std::size_t>(view.gaze)];
    cl += Maze::colDelta[static_cast<std::size_t>(view.gaze)];
    if(!Maze::inside(r, cl))
      break;
  }
  return true;
}

bool OracledMazePerceptsProvider::observeTag(const View& view, int cell, Maze::Heading face, AprilTagPercept::Detection& detection) const
{
  const Vector2f centre = cellCentreOf(Maze::cellRow(cell), Maze::cellCol(cell));
  const Vector2f normal = -directionOf(face); // points into the cell, out of the wall
  const Vector2f tag = centre + directionOf(face) * halfCell;

  // Close enough, inside the horizontal field of view of the camera.
  const Vector2f tagInRobot = toRobot(view, tag);
  const Vector2f cameraInRobot = theCameraMatrix.translation.head<2>();
  const Vector2f toTag = tagInRobot - cameraInRobot;
  if(toTag.norm() > static_cast<float>(maxRange) * Maze::cellSizeMm + halfCell)
    return false;
  const Vector3f gazeInRobot = theCameraMatrix.rotation * Vector3f(1.f, 0.f, 0.f);
  const float bearing = Angle::normalize(std::atan2(toTag.y(), toTag.x()) - std::atan2(gazeInRobot.y(), gazeInRobot.x()));
  if(std::abs(bearing) > theCameraInfo.openingAngleWidth / 2.f)
    return false;

  // The tag faces the camera.
  const Vector2f toCamera = view.camera - tag;
  if(toCamera.norm() < 1.f || std::acos(std::clamp(normal.dot(toCamera.normalized()), -1.f, 1.f)) > static_cast<float>(maxTagViewAngle))
    return false;

  // Pose: x to the right and y up when looking at the tag, z along the normal.
  const float c = std::cos(-view.robotRotation), s = std::sin(-view.robotRotation);
  const Vector3f normalInRobot(c * normal.x() - s * normal.y(), s * normal.x() + c * normal.y(), 0.f);
  const Vector3f right(-normalInRobot.y(), normalInRobot.x(), 0.f);
  Matrix3f axes;
  axes.col(0) = right;
  axes.col(1) = Vector3f(0.f, 0.f, 1.f);
  axes.col(2) = normalInRobot;
  RotationMatrix rotation(axes);
  rotation.rotateX(noisy(0.f, tagRotationStdDev));
  rotation.rotateY(noisy(0.f, tagRotationStdDev));
  rotation.rotateZ(noisy(0.f, tagRotationStdDev));

  detection.id = cell;
  detection.corners.clear();
  detection.tagInRobot.rotation = rotation;
  detection.tagInRobot.translation = Vector3f(noisy(tagInRobot.x(), tagPositionStdDev),
                                              noisy(tagInRobot.y(), tagPositionStdDev),
                                              noisy(layout.centreHeight, tagPositionStdDev));
  detection.decisionMargin = 100.f;
  return true;
}

void OracledMazePerceptsProvider::update(AprilTagPercept& aprilTagPercept)
{
  aprilTagPercept.detections.clear();
  aprilTagPercept.timestamp = theFrameInfo.time;
  View view;
  if(dropoutRate >= 1.f || !updateMaze() || !computeView(view))
    return;
  for(const LookAheadCell& cell : view.cells)
  {
    const int id = Maze::cellId(cell.row, cell.col);
    for(int h = 0; h < Maze::headingCount; ++h)
    {
      const Maze::Heading face = static_cast<Maze::Heading>(h);
      AprilTagPercept::Detection detection;
      if(layout.hasTag(id, face, maze.graph) && !dropped() && observeTag(view, id, face, detection))
        aprilTagPercept.detections.push_back(detection);
    }
  }
}

void OracledMazePerceptsProvider::update(WallPercept& wallPercept)
{
  wallPercept.segments.clear();
  wallPercept.cellsAhead.clear();
  wallPercept.frontDistance = wallPercept.leftDistance = wallPercept.rightDistance = -1.f;
  wallPercept.lookAheadTurns = 0;
  wallPercept.timestamp = theFrameInfo.time;
  View view;
  if(dropoutRate >= 1.f || !updateMaze() || !computeView(view))
    return;

  const auto toEdge = [](Maze::EdgeState state)
  {
    return state == Maze::EdgeState::wall ? WallPercept::wall : state == Maze::EdgeState::open ? WallPercept::open : WallPercept::unknown;
  };

  // Gaze relative to the robot in quarter turns, in [-1, 2].
  int turns = static_cast<int>(std::lround(Angle::normalize(angleOf(view.gaze) - view.robotRotation) / quarterTurn));
  if(turns < -1)
    turns += 4;
  wallPercept.lookAheadTurns = turns;

  const Vector2f forward = directionOf(view.gaze);
  const Maze::Heading leftHeading = Maze::turned(view.gaze, -1), rightHeading = Maze::turned(view.gaze, 1);
  for(const LookAheadCell& cell : view.cells)
  {
    const Vector2f centre = cellCentreOf(cell.row, cell.col);
    const bool last = &cell == &view.cells.back();

    // Distances to the nearest walls along and across the gaze.
    if(cell.ahead == 0)
    {
      if(cell.left == Maze::EdgeState::wall && !dropped())
        wallPercept.leftDistance = std::max(0.f, noisy((centre + directionOf(leftHeading) * halfCell - view.robot).dot(directionOf(leftHeading)), wallDistanceStdDev));
      if(cell.right == Maze::EdgeState::wall && !dropped())
        wallPercept.rightDistance = std::max(0.f, noisy((centre + directionOf(rightHeading) * halfCell - view.robot).dot(directionOf(rightHeading)), wallDistanceStdDev));
    }
    if(last && cell.front == Maze::EdgeState::wall && !dropped())
      wallPercept.frontDistance = std::max(0.f, noisy((centre + forward * halfCell - view.robot).dot(forward), wallDistanceStdDev));

    // The look-ahead entry of the cell.
    if(!dropped())
    {
      WallPercept::CellAhead entry;
      entry.ahead = cell.ahead;
      entry.left = toEdge(cell.left);
      entry.right = toEdge(cell.right);
      entry.front = toEdge(cell.front);
      entry.distance = std::max(0.f, noisy((centre - view.robot).norm(), wallDistanceStdDev));
      wallPercept.cellsAhead.push_back(entry);
    }

    // The wall segments of the cell: the 500 mm edge of every wall.
    const std::pair<Maze::Heading, Maze::EdgeState> edges[] = {{leftHeading, cell.left}, {rightHeading, cell.right}, {view.gaze, cell.front}};
    for(const auto& [heading, state] : edges)
    {
      if(state != Maze::EdgeState::wall || dropped())
        continue;
      const Vector2f direction = directionOf(heading);
      const Vector2f along(-direction.y(), direction.x());
      const Vector2f middle = centre + direction * halfCell;
      WallPercept::Segment segment;
      segment.start = toRobot(view, middle - along * halfCell);
      segment.end = toRobot(view, middle + along * halfCell);
      wallPercept.segments.push_back(segment);
    }
  }
}
