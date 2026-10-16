/**
 * @file MazeMapper.cpp
 *
 * Implementation of the maze mapper, see the header.
 */

#include "MazeMapper.h"
#include "Debugging/DebugDrawings.h"
#include "Debugging/Debugging.h"
#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeSession.h"
#include <algorithm>

// The quality gate casts RobotPose::LocalizationQuality to Maze::LocalizationQuality, so both must share the order.
static_assert(static_cast<int>(RobotPose::superb) == static_cast<int>(Maze::LocalizationQuality::superb) &&
              static_cast<int>(RobotPose::okay) == static_cast<int>(Maze::LocalizationQuality::okay) &&
              static_cast<int>(RobotPose::poor) == static_cast<int>(Maze::LocalizationQuality::poor),
              "RobotPose::LocalizationQuality and Maze::LocalizationQuality must have the same order");

MAKE_MODULE(MazeMapper);

namespace
{
  Maze::EdgeState edgeStateOf(WallPercept::Edge edge)
  {
    switch(edge)
    {
      case WallPercept::wall: return Maze::EdgeState::wall;
      case WallPercept::open: return Maze::EdgeState::open;
      default: return Maze::EdgeState::unknown;
    }
  }
}

MazeMapper::MazeMapper()
{
  if(!Maze::MazeTagLayout::load("mazeTagLayout.cfg", layout))
    OUTPUT_WARNING("MazeMapper: mazeTagLayout.cfg not found, using a tag on every wall face.");
}

int MazeMapper::observeWalls()
{
  if(!wallFilter.accept(theWallPercept.timestamp))
    return 0;
  lastObservedCells.clear();
  const Maze::Heading robotHeading = static_cast<Maze::Heading>(static_cast<int>(theMazeCellPose.heading));
  const Maze::Heading gaze = Maze::gazeHeading(robotHeading, theWallPercept.lookAheadTurns);

  std::vector<Maze::CellObservation> cells;
  cells.reserve(theWallPercept.cellsAhead.size());
  for(const WallPercept::CellAhead& ahead : theWallPercept.cellsAhead)
  {
    Maze::CellObservation observation;
    observation.ahead = ahead.ahead;
    observation.left = edgeStateOf(ahead.left);
    observation.right = edgeStateOf(ahead.right);
    observation.front = edgeStateOf(ahead.front);
    observation.distance = ahead.distance;
    cells.push_back(observation);
    const int row = theMazeCellPose.row + Maze::rowDelta[static_cast<std::size_t>(gaze)] * ahead.ahead;
    const int col = theMazeCellPose.col + Maze::colDelta[static_cast<std::size_t>(gaze)] * ahead.ahead;
    if(Maze::inside(row, col))
      lastObservedCells.push_back(Maze::cellId(row, col));
  }
  return fusion.observeCellsAhead(theMazeCellPose.row, theMazeCellPose.col, gaze, cells);
}

int MazeMapper::observeTags()
{
  if(!tagFilter.accept(theAprilTagPercept.timestamp))
    return 0;
  int changes = 0;
  for(const AprilTagPercept::Detection& detection : theAprilTagPercept.detections)
  {
    // The same face the localizer picks; a detection it would reject says nothing about the map.
    const Maze::FaceChoice choice = Maze::chooseFace(detection.id, theRobotPose.rotation, detection.tagInRobot, layout);
    if(choice.found && choice.angleError <= static_cast<float>(maxFaceAngleError) && fusion.observeTag(detection.id, choice.face))
      ++changes;
  }
  return changes;
}

void MazeMapper::update(MazeMap& mazeMap)
{
  fusion.parameters.threshold = threshold;
  fusion.parameters.fullWeightDistance = fullWeightDistance;
  fusion.parameters.zeroWeightDistance = zeroWeightDistance;
  fusion.parameters.tagWeight = tagWeight;
  fusion.startCell = std::clamp(startCell, 0, Maze::cellCount - 1);

  const bool playing = theGameState.isPlaying();
  if(session.update(playing).newAttempt)
    fusion.newAttempt();

  const Maze::MapActions actions = Maze::mapActions(playing, theMazeCellPose.insideMaze,
                                                    Maze::qualityAccepted(static_cast<Maze::LocalizationQuality>(static_cast<int>(theMazeCellPose.quality)),
                                                                          static_cast<Maze::LocalizationQuality>(static_cast<int>(worstAcceptedQuality))));
  if(actions.markVisited)
    fusion.markVisited(theMazeCellPose.row, theMazeCellPose.col);
  if(actions.observe)
  {
    const int changes = observeWalls() + observeTags();
    if(changes > 0)
    {
      ++changeCounter;
      lastChangeTime = theFrameInfo.time;
    }
  }

  mazeMap.setEdges(fusion.graph);
  for(int cell = 0; cell < Maze::cellCount; ++cell)
    mazeMap.visited[static_cast<std::size_t>(cell)] = fusion.visited(Maze::cellRow(cell), Maze::cellCol(cell));
  mazeMap.startCell = fusion.startCell;
  mazeMap.attempt = fusion.attempt;
  mazeMap.changeCounter = changeCounter;
  mazeMap.lastChangeTime = lastChangeTime;

  DEBUG_DRAWING("module:MazeMapper:lookAhead", "drawingOnField")
  {
    // The cells of the last wall look-ahead in the maze frame.
    for(const int cell : lastObservedCells)
    {
      const Maze::CellCentre centre = Maze::cellCentre(Maze::cellRow(cell), Maze::cellCol(cell));
      const float half = Maze::cellSizeMm / 2.f - 60.f;
      FILLED_RECTANGLE("module:MazeMapper:lookAhead", centre.x - half, centre.y - half, centre.x + half, centre.y + half,
                       10, Drawings::solidPen, ColorRGBA::orange, Drawings::solidBrush, ColorRGBA(255, 165, 0, 60));
    }
  }
}
