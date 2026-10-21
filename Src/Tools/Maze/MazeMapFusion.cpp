/**
 * @file Tools/Maze/MazeMapFusion.cpp
 *
 * Implementation of the evidence based wall map, see the header for the model.
 */

#include "MazeMapFusion.h"
#include <algorithm>

namespace Maze
{
  float MazeMapFusion::wallWeight(float distanceMm) const
  {
    if(distanceMm <= parameters.fullWeightDistance)
      return 1.f;
    if(distanceMm >= parameters.zeroWeightDistance)
      return 0.f;
    return (parameters.zeroWeightDistance - distanceMm) / (parameters.zeroWeightDistance - parameters.fullWeightDistance);
  }

  std::size_t MazeMapFusion::index(int row, int col, Heading heading)
  {
    // The south and east side of a cell are the north and west side of the neighbour, if there is one.
    if(heading == Heading::south || heading == Heading::east)
    {
      const int r = row + rowDelta[static_cast<std::size_t>(heading)], c = col + colDelta[static_cast<std::size_t>(heading)];
      if(inside(r, c))
        return index(r, c, turned(heading, 2));
    }
    return static_cast<std::size_t>(cellId(row, col) * headingCount + static_cast<int>(heading));
  }

  bool MazeMapFusion::observe(int row, int col, Heading heading, bool isWall, float weight, unsigned time)
  {
    if(!inside(row, col) || weight <= 0.f)
      return false;
    if(!inside(row + rowDelta[static_cast<std::size_t>(heading)], col + colDelta[static_cast<std::size_t>(heading)]))
      return false; // The border is a wall and stays one.

    const std::size_t edge = index(row, col, heading);
    if(!isWall && blocked[edge])
      return false; // The robot was stopped by this edge: nothing opens it, whatever the source.
    if(time != untimed)
    {
      // An out-of-order observation (a tag percept older than the last wall percept, both on the Upper clock) is dropped and
      // must not move the edge's clock back, or it would shorten the interval of the next observation. A jump back larger than
      // `clockRestartThreshold` is a new session (the clock restarted) and is accepted.
      const bool restarted = hasObservation[edge] && time < lastObservation[edge] && static_cast<float>(lastObservation[edge] - time) > parameters.clockRestartThreshold;
      const bool outOfOrder = hasObservation[edge] && time < lastObservation[edge] && !restarted;
      const bool tooSoon = hasObservation[edge] && time >= lastObservation[edge] && static_cast<float>(time - lastObservation[edge]) < parameters.minObservationInterval;
      if(tooSoon || outOfOrder)
        return false;
      hasObservation[edge] = true;
      lastObservation[edge] = time;
    }

    float& score = evidence[edge];
    const EdgeState state = graph.get(row, col, heading);
    score += isWall ? weight : -weight;
    switch(state)
    {
      case EdgeState::unknown:
        break;
      case EdgeState::wall:
        score = std::min(score, 0.f); // Only evidence against the wall counts.
        break;
      default:
        score = std::max(score, 0.f);
        break;
    }

    EdgeState decided = state;
    if(score >= parameters.threshold && state != EdgeState::wall)
      decided = EdgeState::wall;
    else if(score <= -parameters.threshold && state != EdgeState::open)
      decided = EdgeState::open;
    if(decided == state)
      return false;
    graph.set(row, col, heading, decided);
    score = 0.f;
    return true;
  }

  int MazeMapFusion::observeCellsAhead(int robotRow, int robotCol, Heading gaze, const std::vector<CellObservation>& cells, unsigned time)
  {
    int changes = 0;
    const std::size_t g = static_cast<std::size_t>(gaze);
    for(const CellObservation& cell : cells)
    {
      const int row = robotRow + rowDelta[g] * cell.ahead, col = robotCol + colDelta[g] * cell.ahead;
      if(!inside(row, col))
        continue;
      const float weight = wallWeight(cell.distance);
      const std::array<std::pair<Heading, EdgeState>, 3> edges = {{{turned(gaze, -1), cell.left}, {turned(gaze, 1), cell.right}, {gaze, cell.front}}};
      for(const auto& [heading, state] : edges)
      {
        if(state == EdgeState::unknown)
          continue;
        if(state == EdgeState::open && (tagged[index(row, col, heading)] || blocked[index(row, col, heading)]))
          continue; // A tag was seen on this face or the robot was blocked by it: a wall percept that sees through it is wrong.
        if(observe(row, col, heading, state == EdgeState::wall, weight, time))
          ++changes;
      }
    }
    return changes;
  }

  bool MazeMapFusion::observeTag(int cell, Heading face, unsigned time)
  {
    if(cell < 0 || cell >= cellCount)
      return false;
    tagged[index(cellRow(cell), cellCol(cell), face)] = true; // Even if the observation is rate limited: the tag was seen.
    return observe(cellRow(cell), cellCol(cell), face, true, parameters.tagWeight, time);
  }

  bool MazeMapFusion::observeBlocked(int row, int col, Heading heading)
  {
    if(!inside(row, col) || !inside(row + rowDelta[static_cast<std::size_t>(heading)], col + colDelta[static_cast<std::size_t>(heading)]))
      return false;
    const std::size_t edge = index(row, col, heading);
    blocked[edge] = true;
    evidence[edge] = 0.f; // Nothing can speak against the wall any more.
    if(graph.get(row, col, heading) == EdgeState::wall)
      return false;
    graph.set(row, col, heading, EdgeState::wall);
    return true;
  }

  void MazeMapFusion::markVisited(int row, int col)
  {
    if(inside(row, col))
      visitedCells[static_cast<std::size_t>(cellId(row, col))] = true;
  }

  bool MazeMapFusion::visited(int row, int col) const
  {
    return inside(row, col) && visitedCells[static_cast<std::size_t>(cellId(row, col))];
  }

  bool MazeMapFusion::operator==(const MazeMapFusion& other) const
  {
    return graph == other.graph && evidence == other.evidence && visitedCells == other.visitedCells && startCell == other.startCell;
  }
}
