/**
 * @file MazeMap.cpp
 *
 * Access to the flat edge arrays (the indexing is in Tools/Maze/MazeEdgeIndex.h), conversion to `Maze::MazeGraph` and drawing of the map.
 */

#include "MazeMap.h"
#include "Debugging/DebugDrawings.h"

MazeMap::Edge MazeMap::get(int row, int col, Maze::Heading heading) const
{
  return Maze::EdgeIndex::get(horizontal, vertical, row, col, heading);
}

void MazeMap::set(int row, int col, Maze::Heading heading, Edge edge)
{
  Maze::EdgeIndex::set(horizontal, vertical, row, col, heading, edge);
}

void MazeMap::setEdges(const Maze::MazeGraph& graph)
{
  Maze::EdgeIndex::setEdges(horizontal, vertical, graph);
}

Maze::MazeGraph MazeMap::toGraph() const
{
  return Maze::EdgeIndex::toGraph(horizontal, vertical);
}

int MazeMap::unknownEdges() const
{
  return Maze::EdgeIndex::unknownCount(horizontal, vertical);
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
