/**
 * @file MazePlan.cpp
 *
 * Drawing of the maze plan.
 */

#include "MazePlan.h"
#include "Debugging/DebugDrawings.h"
#include "Tools/Maze/MazeGraph.h"

void MazePlan::draw() const
{
  DEBUG_DRAWING("representation:MazePlan", "drawingOnField")
  {
    // Lines through the cell centres of the maze frame; the routes are shifted against each other so that they stay visible where they coincide.
    struct Line
    {
      const MazeRoute& route;
      ColorRGBA color;
      Drawings::PenStyle style;
      float offset;
    };
    for(const Line& line : {Line{exploration, ColorRGBA(240, 140, 0), Drawings::dashedPen, 25.f}, Line{speedRun, ColorRGBA::blue, Drawings::solidPen, -25.f}, Line{returnToStart, ColorRGBA(0, 160, 0), Drawings::dottedPen, 0.f}})
      for(std::size_t i = 1; i < line.route.cells.size(); ++i)
      {
        const Maze::CellCentre from = Maze::cellCentre(Maze::cellRow(line.route.cells[i - 1]), Maze::cellCol(line.route.cells[i - 1]));
        const Maze::CellCentre to = Maze::cellCentre(Maze::cellRow(line.route.cells[i]), Maze::cellCol(line.route.cells[i]));
        LINE("representation:MazePlan", from.x + line.offset, from.y + line.offset, to.x + line.offset, to.y + line.offset, 20, line.style, line.color);
      }
  }
}
