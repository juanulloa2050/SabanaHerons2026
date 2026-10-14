/**
 * @file WallPercept.cpp
 *
 * Drawing and verification of the wall percept.
 */

#include "WallPercept.h"
#include "Debugging/DebugDrawings.h"
#include "Platform/BHAssert.h"
#include <cmath>

void WallPercept::draw() const
{
  DEBUG_DRAWING("representation:WallPercept:field", "drawingOnField")
  {
    for(const Segment& segment : segments)
      LINE("representation:WallPercept:field", segment.start.x(), segment.start.y(), segment.end.x(), segment.end.y(),
           30, Drawings::solidPen, ColorRGBA::orange);
    for(const CellAhead& cell : cellsAhead)
    {
      // Cell centres along the gaze, marked by the cell index and the number of walls known.
      const float angle = static_cast<float>(lookAheadTurns) * 1.5707963f;
      const Vector2f centre = Vector2f(std::cos(angle), std::sin(angle)) * cell.distance;
      CROSS("representation:WallPercept:field", centre.x(), centre.y(), 30, 10, Drawings::solidPen, ColorRGBA::orange);
      DRAW_TEXT("representation:WallPercept:field", centre.x() + 40.f, centre.y() + 40.f, 40, ColorRGBA::orange, cell.ahead);
    }
  }
}

void WallPercept::verify() const
{
  ASSERT(std::isfinite(frontDistance));
  ASSERT(std::isfinite(leftDistance));
  ASSERT(std::isfinite(rightDistance));
  for(const Segment& segment : segments)
    ASSERT(segment.start.allFinite() && segment.end.allFinite());
  for(const CellAhead& cell : cellsAhead)
    ASSERT(cell.ahead >= 0 && std::isfinite(cell.distance));
}
