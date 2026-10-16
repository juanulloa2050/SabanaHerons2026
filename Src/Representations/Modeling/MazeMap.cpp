/**
 * @file MazeMap.cpp
 *
 * Access to the flat edge arrays, conversion to `Maze::MazeGraph` and drawing of the map.
 */

#include "MazeMap.h"
#include "Debugging/DebugDrawings.h"

namespace
{
  /** Index into the flat edge arrays of the edge on side `heading` of (row, col). */
  struct EdgeSlot
  {
    bool horizontal;
    std::size_t index;
  };

  EdgeSlot slotOf(int row, int col, Maze::Heading heading)
  {
    switch(heading)
    {
      case Maze::Heading::north: return {true, static_cast<std::size_t>(row * Maze::size + col)};
      case Maze::Heading::south: return {true, static_cast<std::size_t>((row + 1) * Maze::size + col)};
      case Maze::Heading::west: return {false, static_cast<std::size_t>(row * (Maze::size + 1) + col)};
      default: return {false, static_cast<std::size_t>(row * (Maze::size + 1) + col + 1)};
    }
  }
}

MazeMap::Edge MazeMap::get(int row, int col, Maze::Heading heading) const
{
  const EdgeSlot slot = slotOf(row, col, heading);
  return slot.horizontal ? horizontal[slot.index] : vertical[slot.index];
}

void MazeMap::set(int row, int col, Maze::Heading heading, Edge edge)
{
  const EdgeSlot slot = slotOf(row, col, heading);
  (slot.horizontal ? horizontal[slot.index] : vertical[slot.index]) = edge;
}

void MazeMap::setEdges(const Maze::MazeGraph& graph)
{
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
      for(int h = 0; h < Maze::headingCount; ++h)
      {
        const Maze::Heading heading = static_cast<Maze::Heading>(h);
        set(row, col, heading, static_cast<Edge>(static_cast<int>(graph.get(row, col, heading))));
      }
}

Maze::MazeGraph MazeMap::toGraph() const
{
  Maze::MazeGraph graph;
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
      for(int h = 0; h < Maze::headingCount; ++h)
      {
        const Maze::Heading heading = static_cast<Maze::Heading>(h);
        graph.set(row, col, heading, static_cast<Maze::EdgeState>(static_cast<int>(get(row, col, heading))));
      }
  return graph;
}

int MazeMap::unknownEdges() const
{
  int count = 0;
  for(int row = 0; row < Maze::size; ++row)
    for(int col = 0; col < Maze::size; ++col)
    {
      // Count each shared edge once: the north and west side of every cell, plus the south and east border (always walls).
      count += get(row, col, Maze::Heading::north) == Edge::unknown ? 1 : 0;
      count += get(row, col, Maze::Heading::west) == Edge::unknown ? 1 : 0;
    }
  return count;
}

void MazeMap::draw() const
{
  DEBUG_DRAWING("representation:MazeMap", "drawingOnField")
  {
    // The drawing is in the maze frame: x east, y north, origin at the outer bottom-left corner (mm).
    for(int row = 0; row < Maze::size; ++row)
      for(int col = 0; col < Maze::size; ++col)
      {
        const Maze::CellCentre centre = Maze::cellCentre(row, col);
        const float half = Maze::cellSizeMm / 2.f;
        const int cell = Maze::cellId(row, col);
        if(visited[static_cast<std::size_t>(cell)])
          FILLED_RECTANGLE("representation:MazeMap", centre.x - half + 40.f, centre.y - half + 40.f, centre.x + half - 40.f, centre.y + half - 40.f,
                           0, Drawings::noPen, ColorRGBA::green, Drawings::solidBrush, ColorRGBA(120, 200, 120, 90));
        else if(Maze::isGoal(row, col))
          FILLED_RECTANGLE("representation:MazeMap", centre.x - half + 40.f, centre.y - half + 40.f, centre.x + half - 40.f, centre.y + half - 40.f,
                           0, Drawings::noPen, ColorRGBA::yellow, Drawings::solidBrush, ColorRGBA(240, 220, 60, 90));
        if(cell == startCell)
          DRAW_TEXT("representation:MazeMap", centre.x - 30.f, centre.y - 30.f, 80, ColorRGBA::blue, "S");

        // Every edge is drawn from the cell on its north and west side; the south and east borders close the maze.
        for(const Maze::Heading side : {Maze::Heading::north, Maze::Heading::west, Maze::Heading::south, Maze::Heading::east})
        {
          const bool shared = side == Maze::Heading::south || side == Maze::Heading::east;
          if(shared && (side == Maze::Heading::south ? row != Maze::size - 1 : col != Maze::size - 1))
            continue;
          float x1, y1, x2, y2;
          switch(side)
          {
            case Maze::Heading::north: x1 = centre.x - half; y1 = centre.y + half; x2 = centre.x + half; y2 = y1; break;
            case Maze::Heading::south: x1 = centre.x - half; y1 = centre.y - half; x2 = centre.x + half; y2 = y1; break;
            case Maze::Heading::west: x1 = centre.x - half; y1 = centre.y - half; x2 = x1; y2 = centre.y + half; break;
            default: x1 = centre.x + half; y1 = centre.y - half; x2 = x1; y2 = centre.y + half; break;
          }
          switch(get(row, col, side))
          {
            case Edge::wall: LINE("representation:MazeMap", x1, y1, x2, y2, 30, Drawings::solidPen, ColorRGBA::black); break;
            case Edge::open: LINE("representation:MazeMap", x1, y1, x2, y2, 8, Drawings::dottedPen, ColorRGBA(60, 170, 60)); break;
            default: LINE("representation:MazeMap", x1, y1, x2, y2, 10, Drawings::dashedPen, ColorRGBA(150, 150, 150)); break;
          }
        }
      }
  }
}
