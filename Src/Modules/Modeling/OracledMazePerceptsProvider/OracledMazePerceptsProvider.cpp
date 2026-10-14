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
#include <system_error>

MAKE_MODULE(OracledMazePerceptsProvider);

namespace
{
  using namespace Maze::Oracle;

  /**
   * Decision margin of every oracle detection. The oracle has no image, so it reports the value of
   * a clean detection, far above what a consumer can reasonably demand.
   */
  constexpr float perfectDecisionMargin = 100.f;

  constexpr float quarterTurn = pi / 2.f;

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
  const bool pathChanged = mazeFile != loadedMazeFile;
  if(!pathChanged && theFrameInfo.getTimeSince(lastLookTime) < static_cast<int>(reloadIntervalMs))
    return mazeAvailable;
  lastLookTime = theFrameInfo.time;

  const std::string path = std::string(File::getBHDir()) + "/" + mazeFile;
  std::error_code code;
  const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, code);
  // A maze that was read and is unchanged stays; everything else (new path, new write, no maze yet) is read now.
  if(!pathChanged && mazeAvailable && !code && writeTime == loadedWriteTime)
    return true;

  if(pathChanged)
    warned = false;
  loadedMazeFile = mazeFile;
  std::string error;
  Maze::MazeDescription loaded;
  if(Maze::MazeFile::load(path, loaded, error))
  {
    maze = loaded;
    loadedWriteTime = writeTime;
    mazeAvailable = true;
    warned = false;
  }
  else if(!mazeAvailable || pathChanged)
  {
    // A failed reload of a changed file keeps the last good maze; with none, the oracle is silent.
    mazeAvailable = false;
    if(!warned)
    {
      // Once per failure episode: the file is looked at again every second, but not reported again.
      OUTPUT_WARNING("OracledMazePerceptsProvider: " << mazeFile << " not found or invalid (" << error
                     << "), run python3 Util/MazeTools/maze_to_scene.py Config/Mazes/generated/maze_<N>.maze --out "
                     << std::filesystem::path(mazeFile).replace_extension(".rsi2").string());
      warned = true;
    }
  }
  else if(!warned)
  {
    OUTPUT_WARNING("OracledMazePerceptsProvider: " << mazeFile << " changed but is invalid (" << error
                   << "), keeping the previous maze.");
    warned = true;
  }
  return mazeAvailable;
}

Maze::Oracle::Camera OracledMazePerceptsProvider::camera() const
{
  const Vector3f gazeInRobot = theCameraMatrix.rotation * Vector3f(1.f, 0.f, 0.f);
  return {theCameraMatrix.translation.head<2>(), std::atan2(gazeInRobot.y(), gazeInRobot.x()), theCameraInfo.openingAngleWidth};
}

bool OracledMazePerceptsProvider::computeView(View& view) const
{
  const Pose2f robotInMaze(theGroundTruthRobotPose.rotation, theGroundTruthRobotPose.translation - mazeOriginOnField);
  return Maze::Oracle::computeView(maze.graph, robotInMaze, camera(), maxRange, view);
}

bool OracledMazePerceptsProvider::observeTag(const View& view, int cell, Maze::Heading face, AprilTagPercept::Detection& detection) const
{
  Pose3f tagInRobot;
  if(!Maze::Oracle::observeTag(view, camera(), maxRange, static_cast<float>(maxTagViewAngle), cell, face, layout.centreHeight, tagInRobot))
    return false;

  RotationMatrix rotation(tagInRobot.rotation);
  rotation.rotateX(noisy(0.f, tagRotationStdDev));
  rotation.rotateY(noisy(0.f, tagRotationStdDev));
  rotation.rotateZ(noisy(0.f, tagRotationStdDev));

  detection.id = cell;
  detection.corners.clear();
  detection.tagInRobot.rotation = rotation;
  detection.tagInRobot.translation = Vector3f(noisy(tagInRobot.translation.x(), tagPositionStdDev),
                                              noisy(tagInRobot.translation.y(), tagPositionStdDev),
                                              noisy(tagInRobot.translation.z(), tagPositionStdDev));
  detection.decisionMargin = perfectDecisionMargin;
  return true;
}

void OracledMazePerceptsProvider::update(AprilTagPercept& aprilTagPercept)
{
  aprilTagPercept.detections.clear();
  aprilTagPercept.timestamp = theFrameInfo.time;
  Maze::Oracle::View view;
  if(dropoutRate >= 1.f || !updateMaze() || !computeView(view))
    return;
  for(const Maze::Oracle::LookAheadCell& cell : view.cells)
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
  Maze::Oracle::View view;
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
  for(const Maze::Oracle::LookAheadCell& cell : view.cells)
  {
    const Vector2f centre = cellCentreOf(cell.row, cell.col);
    const bool last = &cell == &view.cells.back();

    // Distances to the nearest walls along and across the gaze.
    if(cell.ahead == 0)
    {
      if(cell.left == Maze::EdgeState::wall && !dropped())
        wallPercept.leftDistance = std::max(0.f, noisy((centre + directionOf(leftHeading) * halfCellMm - view.robot).dot(directionOf(leftHeading)), wallDistanceStdDev));
      if(cell.right == Maze::EdgeState::wall && !dropped())
        wallPercept.rightDistance = std::max(0.f, noisy((centre + directionOf(rightHeading) * halfCellMm - view.robot).dot(directionOf(rightHeading)), wallDistanceStdDev));
    }
    if(last && cell.front == Maze::EdgeState::wall && !dropped())
      wallPercept.frontDistance = std::max(0.f, noisy((centre + forward * halfCellMm - view.robot).dot(forward), wallDistanceStdDev));

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
      const Vector2f middle = centre + direction * halfCellMm;
      WallPercept::Segment segment;
      segment.start = toRobot(view, middle - along * halfCellMm);
      segment.end = toRobot(view, middle + along * halfCellMm);
      wallPercept.segments.push_back(segment);
    }
  }
}
