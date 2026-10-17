/**
 * @file Tools/Maze/MazeHeading.h
 *
 * Conversions between maze angles and `Maze::Heading`. A maze angle is measured in the maze frame
 * (x east, y north) counterclockwise from east, like the rotation of a `RobotPose`, so north is +90 degrees.
 * `Maze::Heading` itself is clockwise from north. Pure and header-only.
 */

#pragma once

#include "MazeGraph.h"
#include "Math/Angle.h"
#include <cmath>

namespace Maze
{
  /** The nearest multiple of 90 degrees to `angle` (rad), normalized to (-pi, pi]. */
  [[nodiscard]] inline float snap90(float angle)
  {
    return Angle::normalize(static_cast<float>(std::lround(angle / (pi / 2.f))) * (pi / 2.f));
  }

  /** The heading closest to a maze angle. */
  [[nodiscard]] inline Heading headingOf(float angle)
  {
    const int quarters = ((static_cast<int>(std::lround(angle / (pi / 2.f))) % 4) + 4) % 4; // 0 east, 1 north, 2 west, 3 south
    constexpr Heading byQuarter[4] = {Heading::east, Heading::north, Heading::west, Heading::south};
    return byQuarter[quarters];
  }

  /** The maze angle of a heading: east 0, north +90 degrees, west 180 degrees, south -90 degrees. */
  [[nodiscard]] inline float angleOf(Heading heading)
  {
    switch(heading)
    {
      case Heading::north: return pi / 2.f;
      case Heading::east: return 0.f;
      case Heading::south: return -pi / 2.f;
      default: return pi;
    }
  }

  /**
   * The maze heading of a gaze. `quarterTurnsLeft` counts quarter turns counterclockwise from the robot's
   * x-axis (`WallPercept::lookAheadTurns`), while `Heading` turns clockwise, so the turn is subtracted.
   */
  [[nodiscard]] constexpr Heading gazeHeading(Heading robotHeading, int quarterTurnsLeft)
  {
    return turned(robotHeading, -quarterTurnsLeft);
  }
}
