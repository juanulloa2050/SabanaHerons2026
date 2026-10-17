/**
 * @file MazeCellPose.cpp
 *
 * Drawing of the cell pose.
 */

#include "MazeCellPose.h"
#include "Debugging/DebugDrawings.h"
#include "Tools/Maze/MazeGraph.h"

void MazeCellPose::draw() const
{
  DEBUG_DRAWING("representation:MazeCellPose", "drawingOnField")
  {
    // The drawing is in the maze frame, like the RobotPose.
    const Maze::CellCentre centre = Maze::cellCentre(row, col);
    const float half = Maze::cellSizeMm / 2.f - 20.f;
    const ColorRGBA color = insideMaze ? ColorRGBA::green : ColorRGBA::red;
    RECTANGLE("representation:MazeCellPose", centre.x - half, centre.y - half, centre.x + half, centre.y + half, 15, Drawings::solidPen, color);
    const float angle = std::array<float, 4>{{pi / 2.f, 0.f, -pi / 2.f, pi}}[static_cast<std::size_t>(heading)];
    ARROW("representation:MazeCellPose", centre.x, centre.y, centre.x + std::cos(angle) * half, centre.y + std::sin(angle) * half,
          15, Drawings::solidPen, color);
  }
}
