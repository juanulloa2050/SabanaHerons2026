/**
 * @file Tools/Maze/MazeStartDeduction.cpp
 *
 * Implementation of the start deduction, see the header for the model.
 */

#include "MazeStartDeduction.h"
#include "MazeHeading.h"
#include "MazeLocalization.h"
#include "MazeOracle.h"
#include <algorithm>
#include <cmath>

namespace Maze
{
  namespace
  {
    /** The step (row, column) turned clockwise by `quarterTurns` * 90 degrees, like `MazeGraph::rotated90`. */
    GridCell rotated(GridCell step, int quarterTurns)
    {
      for(int i = 0; i < (quarterTurns % headingCount + headingCount) % headingCount; ++i)
        step = {step.col, -step.row};
      return step;
    }

    /** The edge on side `side` of a cell, the same for both cells that share it: row, column, horizontal. */
    std::array<int, 3> canonicalEdge(GridCell cell, Heading side)
    {
      switch(side)
      {
        case Heading::north: return {cell.row, cell.col, 1};
        case Heading::south: return {cell.row + 1, cell.col, 1};
        case Heading::west: return {cell.row, cell.col, 0};
        default: return {cell.row, cell.col + 1, 0};
      }
    }

    GridCell neighbour(GridCell cell, Heading side)
    {
      return {cell.row + rowDelta[static_cast<std::size_t>(side)], cell.col + colDelta[static_cast<std::size_t>(side)]};
    }
  }

  const std::array<Hypothesis, StartDeduction::hypothesisCount>& StartDeduction::hypotheses()
  {
    static const std::array<Hypothesis, hypothesisCount> all = {{
      {0, Heading::east}, {0, Heading::south},
      {7, Heading::south}, {7, Heading::west},
      {56, Heading::north}, {56, Heading::east},
      {63, Heading::north}, {63, Heading::west},
    }};
    return all;
  }

  const char* evidenceName(Evidence kind)
  {
    switch(kind)
    {
      case Evidence::tag: return "tag";
      case Evidence::edge: return "boundary edge";
      case Evidence::position: return "position";
      default: return "unknown";
    }
  }

  int StartDeduction::indexOf(int cell, Heading heading)
  {
    const auto& all = hypotheses();
    const auto found = std::find(all.begin(), all.end(), Hypothesis{cell, heading});
    return found == all.end() ? -1 : static_cast<int>(found - all.begin());
  }

  bool StartDeduction::isValidStartCell(int cell)
  {
    return cell == -1 || cell == 0 || cell == size - 1 || cell == cellCount - size || cell == cellCount - 1;
  }

  Pose2f StartDeduction::startToMaze(const Hypothesis& hypothesis)
  {
    return Pose2f(angleOf(hypothesis.heading), Oracle::cellCentreOf(cellRow(hypothesis.cell), cellCol(hypothesis.cell)));
  }

  GridCell StartDeduction::toMaze(const Hypothesis& hypothesis, GridCell cell)
  {
    const GridCell step = rotated(cell, static_cast<int>(hypothesis.heading));
    return {cellRow(hypothesis.cell) + step.row, cellCol(hypothesis.cell) + step.col};
  }

  GridCell StartDeduction::toStart(const Hypothesis& hypothesis, GridCell cell)
  {
    return rotated({cell.row - cellRow(hypothesis.cell), cell.col - cellCol(hypothesis.cell)}, -static_cast<int>(hypothesis.heading));
  }

  Heading StartDeduction::toMaze(const Hypothesis& hypothesis, Heading heading)
  {
    return turned(heading, static_cast<int>(hypothesis.heading));
  }

  Heading StartDeduction::toStart(const Hypothesis& hypothesis, Heading heading)
  {
    return turned(heading, -static_cast<int>(hypothesis.heading));
  }

  GridCell StartDeduction::cellOf(const Vector2f& position)
  {
    // Forward (x) is north, which is up the rows; left (y) is west, which is down the columns.
    return {-static_cast<int>(std::floor(position.x() / cellSizeMm + 0.5f)), -static_cast<int>(std::floor(position.y() / cellSizeMm + 0.5f))};
  }

  bool StartDeduction::setGiven(int cell, Heading heading)
  {
    const int index = indexOf(cell, heading);
    if(index < 0)
      return false;
    const int before = workingIndex();
    aliveMask.fill(false);
    aliveMask[static_cast<std::size_t>(index)] = true;
    // Restart from the given start: what was collected for the other hypotheses says nothing about it.
    freezeLog();
    edges.clear();
    conflicts = 0;
    lastConflict = Evidence::none;
    if(index != before)
      ++revision;
    return true;
  }

  void StartDeduction::freezeLog()
  {
    recording = false;
    std::vector<Entry>().swap(log);
    visitedCells.clear();
  }

  void StartDeduction::record(const Entry& entry)
  {
    if(!recording)
      return;
    if(log.size() >= maxLogEntries)
      overflowed = true;
    else
      log.push_back(entry);
  }

  int StartDeduction::aliveCount() const
  {
    return static_cast<int>(std::count(aliveMask.begin(), aliveMask.end(), true));
  }

  int StartDeduction::workingIndex() const
  {
    return static_cast<int>(std::find(aliveMask.begin(), aliveMask.end(), true) - aliveMask.begin());
  }

  int StartDeduction::remove(const std::array<bool, hypothesisCount>& doomed, Evidence kind)
  {
    int removed = 0, left = aliveCount();
    for(int i = 0; i < hypothesisCount; ++i)
      removed += doomed[static_cast<std::size_t>(i)] && alive(i) ? 1 : 0;
    if(removed == 0)
      return 0;
    if(removed >= left)
    {
      lastConflict = kind;
      ++conflicts; // The evidence contradicts every hypothesis that is left: believe none of it.
      return 0;
    }
    const int before = workingIndex();
    for(int i = 0; i < hypothesisCount; ++i)
      if(doomed[static_cast<std::size_t>(i)])
        aliveMask[static_cast<std::size_t>(i)] = false;
    if(workingIndex() != before)
      ++revision;
    return removed;
  }

  int StartDeduction::observeTag(int id, const Vector2f& position, const Vector2f& normal, unsigned time, const MazeTagLayout& layout)
  {
    if(id < 0 || id >= cellCount || !position.allFinite() || !normal.allFinite() || normal.norm() < 1e-3f)
      return 0;
    const float normalAngle = std::atan2(normal.y(), normal.x());
    Entry entry;
    entry.isTag = true;
    entry.id = id;
    entry.normalAngle = normalAngle;
    entry.time = time;
    record(entry);

    const Vector2f centre = Oracle::cellCentreOf(cellRow(id), cellCol(id));
    std::array<bool, hypothesisCount> doomed = {};
    for(int i = 0; i < hypothesisCount; ++i)
    {
      if(!alive(i))
        continue;
      const Hypothesis& hypothesis = hypotheses()[static_cast<std::size_t>(i)];
      const Vector2f tagInMaze = startToMaze(hypothesis) * position;
      const float angleInMaze = normalAngle + angleOf(hypothesis.heading);
      bool explained = false;
      for(const Heading face : layout.candidateFaces(id))
      {
        const Vector2f faceCentre = centre + Oracle::directionOf(face) * Oracle::halfCellMm;
        explained = explained || ((tagInMaze - faceCentre).norm() <= parameters.tagPositionTolerance &&
                                  std::abs(Angle::normalize(angleInMaze - angleOf(turned(face, 2)))) <= parameters.maxFaceAngleError);
      }
      doomed[static_cast<std::size_t>(i)] = !explained;
    }
    return remove(doomed, Evidence::tag);
  }

  bool StartDeduction::acceptedByClock(EdgeEvidence& evidence, unsigned time) const
  {
    if(time == untimed)
      return true;
    // The same rule as the fusion: at most one observation per interval, none from the past unless the clock restarted.
    const bool older = evidence.hasLast && time < evidence.last;
    const bool restarted = older && static_cast<float>(evidence.last - time) > parameters.fusion.clockRestartThreshold;
    const bool tooSoon = evidence.hasLast && !older && static_cast<float>(time - evidence.last) < parameters.fusion.minObservationInterval;
    if((older && !restarted) || tooSoon)
      return false;
    evidence.hasLast = true;
    evidence.last = time;
    return true;
  }

  bool StartDeduction::edgeOnBoundary(const Hypothesis& hypothesis, GridCell cell, Heading side) const
  {
    const GridCell a = toMaze(hypothesis, cell), b = toMaze(hypothesis, neighbour(cell, side));
    return !inside(a.row, a.col) || !inside(b.row, b.col);
  }

  int StartDeduction::observeEdge(GridCell cell, Heading side, bool isWall, float weight, unsigned time)
  {
    if(!std::isfinite(weight) || weight <= 0.f)
      return 0; // A NaN weight would skip the threshold test and could prune on garbage.
    Entry entry;
    entry.cell = cell;
    entry.side = side;
    entry.isWall = isWall;
    entry.weight = weight;
    entry.time = time;
    record(entry);

    EdgeEvidence& evidence = edges[canonicalEdge(cell, side)];
    if(!acceptedByClock(evidence, time))
      return 0;
    // Wall evidence takes back open evidence, so that a boundary seen as a wall most of the time does not condemn a hypothesis.
    evidence.open = isWall ? std::max(0.f, evidence.open - weight) : evidence.open + weight;
    if(isWall || evidence.open < parameters.fusion.threshold)
      return 0;

    std::array<bool, hypothesisCount> doomed = {};
    for(int i = 0; i < hypothesisCount; ++i)
      doomed[static_cast<std::size_t>(i)] = alive(i) && edgeOnBoundary(hypotheses()[static_cast<std::size_t>(i)], cell, side);
    return remove(doomed, Evidence::edge);
  }

  int StartDeduction::observeCellsAhead(GridCell robot, Heading gaze, const std::vector<CellObservation>& cells, const MazeMapFusion& weights, unsigned time)
  {
    int removed = 0;
    for(const CellObservation& observation : cells)
    {
      const GridCell cell = {robot.row + rowDelta[static_cast<std::size_t>(gaze)] * observation.ahead, robot.col + colDelta[static_cast<std::size_t>(gaze)] * observation.ahead};
      const float weight = weights.wallWeight(observation.distance);
      const std::array<std::pair<Heading, EdgeState>, 3> sides = {{{turned(gaze, -1), observation.left}, {turned(gaze, 1), observation.right}, {gaze, observation.front}}};
      for(const auto& [side, state] : sides)
        if(state != EdgeState::unknown)
          removed += observeEdge(cell, side, state == EdgeState::wall, weight, time);
    }
    return removed;
  }

  void StartDeduction::observeBlocked(GridCell cell, Heading side)
  {
    Entry entry;
    entry.isBlocked = true;
    entry.cell = cell;
    entry.side = side;
    entry.isWall = true;
    record(entry);
  }

  void StartDeduction::markVisited(GridCell cell)
  {
    if(recording)
      visitedCells.insert(cell);
  }

  int StartDeduction::observePosition(GridCell cell)
  {
    markVisited(cell);
    std::array<bool, hypothesisCount> doomed = {};
    for(int i = 0; i < hypothesisCount; ++i)
    {
      const GridCell inMaze = toMaze(hypotheses()[static_cast<std::size_t>(i)], cell);
      doomed[static_cast<std::size_t>(i)] = alive(i) && !inside(inMaze.row, inMaze.col);
    }
    return remove(doomed, Evidence::position);
  }

  void StartDeduction::replay(const Hypothesis& hypothesis, MazeMapFusion& fusion, const MazeTagLayout& layout) const
  {
    fusion.startCell = hypothesis.cell;
    for(const GridCell& cell : visitedCells)
    {
      const GridCell inMaze = toMaze(hypothesis, cell);
      if(inside(inMaze.row, inMaze.col))
        fusion.markVisited(inMaze.row, inMaze.col);
    }
    for(const Entry& entry : log)
    {
      if(entry.isTag)
      {
        const FaceChoice choice = chooseFaceByNormal(entry.id, entry.normalAngle + angleOf(hypothesis.heading), layout);
        if(choice.found && choice.angleError <= parameters.maxFaceAngleError)
          fusion.observeTag(entry.id, choice.face, entry.time);
      }
      else if(entry.isBlocked)
      {
        const GridCell inMaze = toMaze(hypothesis, entry.cell);
        fusion.observeBlocked(inMaze.row, inMaze.col, toMaze(hypothesis, entry.side));
      }
      else
      {
        const GridCell inMaze = toMaze(hypothesis, entry.cell);
        fusion.observe(inMaze.row, inMaze.col, toMaze(hypothesis, entry.side), entry.isWall, entry.weight, entry.time);
      }
    }
  }
}
