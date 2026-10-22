/**
 * @file Tools/Maze/MazeDrive.cpp
 *
 * Implementation of the speech queue and the drive controller, see the header.
 */

#include "MazeDrive.h"

namespace Maze
{
  void SpeechQueue::push(const std::string& phrase)
  {
    while(phrases.size() >= maxQueuedPhrases)
    {
      // The phrase that is being said is not dropped, otherwise Say would be cut off in the middle of it.
      if(running && phrases.size() > 1)
        phrases.erase(phrases.begin() + 1);
      else
        phrases.pop_front();
      if(phrases.empty())
        running = false;
    }
    phrases.push_back(phrase);
  }

  std::optional<std::string> SpeechQueue::update(unsigned now)
  {
    if(phrases.empty())
      return std::nullopt;
    if(gap)
    {
      gap = false;
      return std::nullopt;
    }
    if(!running)
    {
      running = true;
      startTime = now;
    }
    std::string text = phrases.front();
    if(now - startTime >= announcementTimeMs)
    {
      phrases.pop_front();
      running = false;
      gap = true;
    }
    return text;
  }

  void DriveController::reset()
  {
    driveKind = DriveKind::idle;
    scannedCell = -1;
    noRouteReported = false;
    scanRequested = false;
    wide = false;
    blocked.reset();
  }

  bool DriveController::actionOver(bool skillDone, unsigned now) const
  {
    switch(driveKind)
    {
      case DriveKind::walk:
        return skillDone || now - startTime > walkTimeoutMs;
      case DriveKind::turn:
        return skillDone || now - startTime > turnTimeoutMs;
      case DriveKind::scan:
        return skillDone || now - startTime > scanTimeoutMs;
      case DriveKind::stepBack:
        return skillDone || now - startTime > stepBackTimeoutMs;
      default:
        return true;
    }
  }

  bool DriveController::mapBlocksAction(const MazeGraph& map) const
  {
    const std::size_t heading = static_cast<std::size_t>(lockedHeading);
    switch(driveKind)
    {
      case DriveKind::turn:
        return map.get(startRow, startCol, lockedHeading) == EdgeState::wall;
      case DriveKind::walk:
      {
        // The walk goes straight along its heading from the cell it started in; every edge up to the target has to be passable.
        int row = startRow, col = startCol;
        for(int step = 0; step < size && inside(row, col) && (row != lockedRow || col != lockedCol); ++step)
        {
          if(map.get(row, col, lockedHeading) == EdgeState::wall)
            return true;
          row += rowDelta[heading];
          col += colDelta[heading];
        }
        return false;
      }
      default:
        return false;
    }
  }

  bool DriveController::actionOver(bool skillDone, unsigned now, const MazeGraph& map) const
  {
    return actionOver(skillDone, now) || mapBlocksAction(map);
  }

  bool DriveController::reportBlocked(unsigned now)
  {
    if(driveKind != DriveKind::walk)
      return false;
    blocked = BlockedEdge{startRow, startCol, lockedHeading};
    driveKind = DriveKind::stepBack;
    startTime = now;
    lockedRow = startRow; // The step back ends in the cell where the walk began, not in the cell of the pose.
    lockedCol = startCol;
    return true;
  }

  DriveDecision DriveController::choose(const DriveInput& input, unsigned now)
  {
    DriveDecision decision;
    if(!input.planCurrent)
    {
      if(input.planValid)
      {
        decision.next = DriveNext::keep;
        return decision;
      }
      driveKind = DriveKind::idle;
      decision.next = DriveNext::idle;
      return decision;
    }
    if(!input.routeFound || !input.hasAction)
    {
      decision.reportNoRoute = !noRouteReported;
      noRouteReported = true;
      driveKind = DriveKind::idle;
      decision.next = DriveNext::idle;
      return decision;
    }
    noRouteReported = false;
    blocked.reset();
    startTime = now;
    startRow = cellRow(input.cell);
    startCol = cellCol(input.cell);
    if(!input.speedRun && scannedCell != input.cell && (input.unknownEdges || scanRequested))
    {
      wide = scanRequested;
      scanRequested = false;
      scannedCell = input.cell;
      driveKind = DriveKind::scan;
      decision.next = DriveNext::scan;
      return decision;
    }
    lockedRow = input.targetRow;
    lockedCol = input.targetCol;
    lockedHeading = input.targetHeading;
    driveKind = input.nextAction == Action::forward ? DriveKind::walk : DriveKind::turn;
    decision.next = input.nextAction == Action::forward ? DriveNext::walk : DriveNext::turn;
    return decision;
  }
}
