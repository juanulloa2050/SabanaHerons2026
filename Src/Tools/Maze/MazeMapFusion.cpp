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
    if(time != untimed)
    {
      // A clock that went backwards (new session) does not block the edge.
      const bool tooSoon = hasObservation[edge] && time >= lastObservation[edge] && static_cast<float>(time - lastObservation[edge]) < parameters.minObservationInterval;
      if(tooSoon)
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
        if(state != EdgeState::unknown && observe(row, col, heading, state == EdgeState::wall, weight, time))
          ++changes;
    }
    return changes;
  }

  bool MazeMapFusion::observeTag(int cell, Heading face, unsigned time)
  {
    if(cell < 0 || cell >= cellCount)
      return false;
    return observe(cellRow(cell), cellCol(cell), face, true, parameters.tagWeight, time);
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
