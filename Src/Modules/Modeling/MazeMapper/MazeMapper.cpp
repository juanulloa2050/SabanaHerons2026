/**
 * @file MazeMapper.cpp
 *
 * Implementation of the maze mapper, see the header.
 */

#include "MazeMapper.h"
#include "Debugging/DebugDrawings.h"
#include "Debugging/Debugging.h"
#include "Tools/Maze/MazeHeading.h"
#include "Streaming/TypeRegistry.h"
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
  if(!Maze::MazeTagLayout::load("mazeTagLayout.cfg", anchored.layout))
    OUTPUT_WARNING("MazeMapper: mazeTagLayout.cfg not found, using a tag on every wall face.");
  loadStart();
}

void MazeMapper::loadStart()
{
  Maze::StartFile file;
  if(!Maze::StartFile::load("handleMaze.cfg", file))
    OUTPUT_WARNING("MazeMapper: handleMaze.cfg not found, deducing the start (start.cell = -1).");
  std::string error;
  const Maze::StartChoice choice = Maze::resolveStart(file.start, error);
  if(!error.empty())
    OUTPUT_ERROR("MazeMapper: handleMaze.cfg: " << error);
  if(choice.given && anchored.setGiven(choice.cell, choice.heading))
    OUTPUT_TEXT("MazeMapper: start given, cell " << choice.cell << " facing " << TypeRegistry::getEnumName(static_cast<MazeCellPose::Heading>(static_cast<int>(choice.heading))) << ".");
}

int MazeMapper::observeWalls()
{
  if(!wallFilter.accept(theWallPercept.timestamp))
    return 0;
  const Pose2f& pose = theRobotPose;
  lookAheadSkipped = !anchored.lookAheadUsable(pose);
  if(lookAheadSkipped)
    return 0; // The oracle shows the true cell, which may be the neighbour of the estimated one.
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
  return anchored.observeCellsAhead(pose, gaze, cells, theWallPercept.timestamp);
}

int MazeMapper::observeTags()
{
  if(!tagFilter.accept(theAprilTagPercept.timestamp))
    return 0;
  int changes = 0;
  for(const AprilTagPercept::Detection& detection : theAprilTagPercept.detections)
    changes += anchored.observeTag(theRobotPose, detection.id, detection.tagInRobot, theAprilTagPercept.timestamp);
  return changes;
}

void MazeMapper::step()
{
  if(stepped && lastStepTime == theFrameInfo.time)
    return;
  stepped = true;
  lastStepTime = theFrameInfo.time;

  Maze::AnchoredMapParameters parameters;
  parameters.fusion.threshold = threshold;
  parameters.fusion.fullWeightDistance = fullWeightDistance;
  parameters.fusion.zeroWeightDistance = zeroWeightDistance;
  parameters.fusion.tagWeight = tagWeight;
  parameters.fusion.minObservationInterval = minObservationInterval;
  parameters.maxFaceAngleError = maxFaceAngleError;
  parameters.maxOffsetFromCentre = maxOffsetFromCentre;
  anchored.setParameters(parameters);

  // A picked-up robot is as inactive as a paused game: the pose would not match what the camera sees.
  const bool active = Maze::isActive(theGameState.isPlaying(), theFallDownState.state);
  if(session.update(active).newAttempt)
    anchored.newAttempt();

  lastObservedCells.clear();
  lookAheadSkipped = false;
  const Maze::MapActions actions = Maze::mapActions(active, Maze::qualityAccepted(static_cast<Maze::LocalizationQuality>(static_cast<int>(theMazeCellPose.quality)),
                                                                                     static_cast<Maze::LocalizationQuality>(static_cast<int>(worstAcceptedQuality))));
  int changes = 0;
  if(actions.markVisited)
    anchored.markVisited(theRobotPose);
  if(actions.observe)
  {
    anchored.observePosition(theRobotPose);
    changes = observeWalls() + observeTags();
  }
  // The start may have become known in this frame: the map is then rebuilt in the new frame, which the planner has to notice.
  if(anchored.endFrame())
  {
    ++changes;
    const Maze::StartStatus status = anchored.status();
    OUTPUT_TEXT("MazeMapper: start hypothesis " << status.working.cell << " facing " << TypeRegistry::getEnumName(static_cast<MazeCellPose::Heading>(static_cast<int>(status.working.heading)))
                << " (revision " << status.revision << ", " << status.aliveCount << " left" << (status.known ? ", known" : "") << ").");
  }
  if(changes > 0)
  {
    ++changeCounter;
    lastChangeTime = theFrameInfo.time;
  }

  if(anchored.takeConflictWarning(theFrameInfo.time))
  {
    const Maze::StartStatus status = anchored.status();
    OUTPUT_WARNING("MazeMapper: an observation contradicts the " << (status.given ? "given" : "only remaining") << " start (cell " << status.working.cell
                   << "), it is not followed. Latest: " << Maze::evidenceName(status.lastConflict) << ". Conflicts so far: " << status.conflicts << ".");
  }
  if(anchored.takeLogOverflowWarning())
    OUTPUT_WARNING("MazeMapper: the start is still unknown after " << static_cast<unsigned>(Maze::StartDeduction::maxLogEntries) << " observations, the map can no longer be re-anchored completely.");
}

void MazeMapper::update(MazeStart& mazeStart)
{
  step();
  const Maze::StartStatus status = anchored.status();
  mazeStart.given = status.given;
  mazeStart.known = status.known;
  mazeStart.cell = status.working.cell;
  mazeStart.heading = static_cast<MazeCellPose::Heading>(static_cast<int>(status.working.heading));
  mazeStart.aliveCount = status.aliveCount;
  mazeStart.aliveMask = status.aliveMask;
  mazeStart.revision = status.revision;
  mazeStart.conflicts = status.conflicts;
  mazeStart.logOverflowed = status.logOverflowed;
}

void MazeMapper::update(MazeMap& mazeMap)
{
  step();
  const Maze::MazeMapFusion& fusion = anchored.map();
  const Maze::StartStatus status = anchored.status();
  mazeMap.setEdges(fusion.graph);
  for(int cell = 0; cell < Maze::cellCount; ++cell)
    mazeMap.visited[static_cast<std::size_t>(cell)] = fusion.visited(Maze::cellRow(cell), Maze::cellCol(cell));
  mazeMap.startCell = fusion.startCell;
  mazeMap.startHeading = static_cast<MazeCellPose::Heading>(static_cast<int>(status.working.heading));
  mazeMap.startKnown = status.known;
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
    if(lookAheadSkipped)
    {
      // The estimate is too close to a cell boundary: the look-ahead is not used.
      const Maze::CellCentre centre = Maze::cellCentre(theMazeCellPose.row, theMazeCellPose.col);
      const float half = Maze::cellSizeMm / 2.f - 60.f;
      RECTANGLE("module:MazeMapper:lookAhead", centre.x - half, centre.y - half, centre.x + half, centre.y + half, 10, Drawings::dashedPen, ColorRGBA::red);
    }
  }
}
