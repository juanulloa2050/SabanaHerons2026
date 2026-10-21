/**
 * @file Tools/Maze/MazeStartAnchoredMap.cpp
 *
 * Implementation of the start-anchored map, see the header.
 */

#include "MazeStartAnchoredMap.h"
#include "MazeOracle.h"
#include <algorithm>
#include <cmath>

namespace Maze
{
  StartAnchoredMap::StartAnchoredMap(const AnchoredMapParameters& parameters) : fusion(parameters.fusion), anchored(startDeduction.working())
  {
    setParameters(parameters);
  }

  void StartAnchoredMap::setParameters(const AnchoredMapParameters& newParameters)
  {
    parameters = newParameters;
    fusion.parameters = parameters.fusion;
    startDeduction.parameters.fusion = parameters.fusion;
    startDeduction.parameters.maxFaceAngleError = parameters.maxFaceAngleError;
  }

  bool StartAnchoredMap::setGiven(int cell, Heading heading)
  {
    if(!startDeduction.setGiven(cell, heading))
      return false;
    given = true;
    rebuild();
    return true;
  }

  void StartAnchoredMap::rebuild()
  {
    MazeMapFusion fresh(fusion.parameters);
    fresh.attempt = fusion.attempt;
    startDeduction.replay(startDeduction.working(), fresh, layout);
    fusion = fresh;
    anchored = startDeduction.working();
  }

  GridCell StartAnchoredMap::startCellOf(const Pose2f& pose) const
  {
    return StartDeduction::cellOf((StartDeduction::startToMaze(anchored).inverse() * pose.translation));
  }

  void StartAnchoredMap::markVisited(const Pose2f& pose)
  {
    const GridCell cell = startCellOf(pose);
    startDeduction.markVisited(cell);
    const GridCell inMaze = StartDeduction::toMaze(anchored, cell);
    fusion.markVisited(inMaze.row, inMaze.col); // Ignores a cell outside the maze.
  }

  void StartAnchoredMap::observePosition(const Pose2f& pose)
  {
    if(lookAheadUsable(pose))
      startDeduction.observePosition(startCellOf(pose));
  }

  bool StartAnchoredMap::lookAheadUsable(const Pose2f& pose) const
  {
    // The position in the start-relative frame; the offset to the cell centre is the same in every frame.
    const Vector2f inStart = StartDeduction::startToMaze(anchored).inverse() * pose.translation;
    if(!inStart.allFinite())
      return false;
    const GridCell cell = StartDeduction::cellOf(inStart);
    // Start-relative cell centre: x forward is up the rows, y left is down the columns.
    const Vector2f centre(-static_cast<float>(cell.row) * cellSizeMm, -static_cast<float>(cell.col) * cellSizeMm);
    return std::abs(inStart.x() - centre.x()) <= parameters.maxOffsetFromCentre && std::abs(inStart.y() - centre.y()) <= parameters.maxOffsetFromCentre;
  }

  int StartAnchoredMap::observeCellsAhead(const Pose2f& pose, Heading gaze, const std::vector<CellObservation>& cells, unsigned time)
  {
    const GridCell robot = startCellOf(pose);
    startDeduction.observeCellsAhead(robot, StartDeduction::toStart(anchored, gaze), cells, fusion, time);
    const GridCell inMaze = StartDeduction::toMaze(anchored, robot);
    if(!inside(inMaze.row, inMaze.col))
      return 0;
    return fusion.observeCellsAhead(inMaze.row, inMaze.col, gaze, cells, time);
  }

  bool StartAnchoredMap::observeBlocked(int row, int col, Heading heading)
  {
    if(!inside(row, col))
      return false;
    startDeduction.observeBlocked(StartDeduction::toStart(anchored, GridCell{row, col}), StartDeduction::toStart(anchored, heading));
    return fusion.observeBlocked(row, col, heading);
  }

  int StartAnchoredMap::observeTag(const Pose2f& pose, int id, const Pose3f& tagInRobot, unsigned time)
  {
    if(id < 0 || id >= cellCount || !tagInRobot.translation.allFinite() || !tagInRobot.rotation.allFinite() || !pose.translation.allFinite() ||
       !std::isfinite(pose.rotation))
      return 0;
    const Pose2f toStart = StartDeduction::startToMaze(anchored).inverse();
    const Vector2f positionInMaze = pose.translation + Oracle::rotated(tagInRobot.translation.head<2>(), pose.rotation);
    const Vector2f normalInMaze = Oracle::rotated(tagInRobot.rotation.col(2).head<2>(), pose.rotation);
    startDeduction.observeTag(id, toStart * positionInMaze, Oracle::rotated(normalInMaze, toStart.rotation), time, layout);

    const GridCell robot = StartDeduction::toMaze(anchored, startCellOf(pose));
    if(!inside(robot.row, robot.col))
      return 0;
    // The face that the working hypothesis explains, like the localizer picks it; a detection it would reject says nothing about the map.
    const FaceChoice choice = chooseFace(id, pose.rotation, tagInRobot, layout);
    return choice.found && choice.angleError <= parameters.maxFaceAngleError && fusion.observeTag(id, choice.face, time) ? 1 : 0;
  }

  bool StartAnchoredMap::endFrame()
  {
    bool rebuilt = false;
    if(!(startDeduction.working() == anchored))
    {
      rebuild();
      rebuilt = true;
    }
    if(startDeduction.known() && startDeduction.logging())
      startDeduction.freezeLog();
    return rebuilt;
  }

  bool StartAnchoredMap::takeConflictWarning(unsigned time)
  {
    if(startDeduction.conflicts <= reportedConflicts)
      return false;
    if(conflictWarned && time >= lastConflictWarning && time - lastConflictWarning < conflictWarningInterval)
      return false;
    reportedConflicts = startDeduction.conflicts;
    lastConflictWarning = time;
    conflictWarned = true;
    return true;
  }

  bool StartAnchoredMap::takeLogOverflowWarning()
  {
    if(!startDeduction.logOverflowed() || overflowReported)
      return false;
    overflowReported = true;
    return true;
  }

  StartStatus StartAnchoredMap::status() const
  {
    StartStatus status;
    status.given = given;
    status.known = startDeduction.known();
    status.working = startDeduction.working();
    status.aliveCount = startDeduction.aliveCount();
    status.aliveMask = 0;
    for(int i = 0; i < StartDeduction::hypothesisCount; ++i)
      status.aliveMask |= startDeduction.alive(i) ? 1u << i : 0u;
    status.revision = startDeduction.revision;
    status.conflicts = startDeduction.conflicts;
    status.lastConflict = startDeduction.lastConflict;
    status.logOverflowed = startDeduction.logOverflowed();
    return status;
  }
}
