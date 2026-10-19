/**
 * @file Tools/Maze/MazeOracle.cpp
 *
 * Implementation of the maze oracle geometry, see the header for the model.
 */

#include "MazeOracle.h"
#include <algorithm>
#include <cmath>

namespace Maze
{
  namespace Oracle
  {
    namespace
    {
      constexpr float quarterTurn = pi / 2.f;

      /** Cross product of two 2D vectors. */
      float cross(const Vector2f& a, const Vector2f& b)
      {
        return a.x() * b.y() - a.y() * b.x();
      }

      /** Whether the segments p to q and a to b share a point. Parallel segments never block. */
      bool segmentsIntersect(const Vector2f& p, const Vector2f& q, const Vector2f& a, const Vector2f& b)
      {
        const Vector2f r = q - p, s = b - a;
        const float denominator = cross(r, s);
        if(std::abs(denominator) < 1e-6f)
          return false;
        const float t = cross(a - p, s) / denominator;
        const float u = cross(a - p, r) / denominator;
        return t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f;
      }

      /** The edge of a cell as (row, col, heading) with a heading of north or west if the edge is shared with a neighbour. */
      struct EdgeKey
      {
        int row, col;
        Heading heading;
        bool operator==(const EdgeKey& other) const
        {
          return row == other.row && col == other.col && heading == other.heading;
        }
      };

      EdgeKey canonical(int row, int col, Heading heading)
      {
        if(heading == Heading::south || heading == Heading::east)
        {
          const int r = row + rowDelta[static_cast<std::size_t>(heading)], c = col + colDelta[static_cast<std::size_t>(heading)];
          if(inside(r, c))
            return {r, c, turned(heading, 2)};
        }
        return {row, col, heading};
      }
    }

    Vector2f directionOf(Heading heading)
    {
      switch(heading)
      {
        case Heading::north: return Vector2f(0.f, 1.f);
        case Heading::east: return Vector2f(1.f, 0.f);
        case Heading::south: return Vector2f(0.f, -1.f);
        default: return Vector2f(-1.f, 0.f);
      }
    }

    Heading snapToHeading(float angle)
    {
      const int quarters = ((static_cast<int>(std::lround(angle / quarterTurn)) % 4) + 4) % 4; // 0 east, 1 north, 2 west, 3 south
      constexpr Heading byQuarter[4] = {Heading::east, Heading::north, Heading::west, Heading::south};
      return byQuarter[quarters];
    }

    float angleOf(Heading heading)
    {
      const Vector2f direction = directionOf(heading);
      return std::atan2(direction.y(), direction.x());
    }

    Vector2f cellCentreOf(int row, int col)
    {
      const CellCentre centre = cellCentre(row, col);
      return Vector2f(centre.x, centre.y);
    }

    Vector2f rotated(const Vector2f& vector, float angle)
    {
      const float c = std::cos(angle), s = std::sin(angle);
      return Vector2f(c * vector.x() - s * vector.y(), s * vector.x() + c * vector.y());
    }

    bool cellContaining(const Vector2f& mazePoint, int& row, int& col)
    {
      if(mazePoint.x() < 0.f || mazePoint.y() < 0.f)
        return false;
      col = static_cast<int>(std::floor(mazePoint.x() / cellSizeMm));
      row = size - 1 - static_cast<int>(std::floor(mazePoint.y() / cellSizeMm));
      return inside(row, col);
    }

    Pose2f robotInMaze(const Pose2f& groundTruthRobotPose, const Pose2f& mazePoseOnField)
    {
      return mazePoseOnField.inverse() * groundTruthRobotPose;
    }

    bool computeView(const MazeGraph& maze, const Pose2f& robotInMaze, const Camera& camera, int maxRange, View& view)
    {
      view.robot = robotInMaze.translation;
      view.robotRotation = robotInMaze.rotation;
      if(!cellContaining(view.robot, view.row, view.col))
        return false;

      view.camera = view.robot + rotated(camera.positionInRobot, view.robotRotation);
      view.gaze = snapToHeading(Angle::normalize(view.robotRotation + camera.yawInRobot));

      // Walk along the gaze: stop after a front wall or after maxRange cells ahead.
      view.cells.clear();
      int r = view.row, c = view.col;
      for(int ahead = 0; ahead <= maxRange; ++ahead)
      {
        LookAheadCell cell;
        cell.ahead = ahead;
        cell.row = r;
        cell.col = c;
        cell.left = maze.get(r, c, turned(view.gaze, -1));
        cell.right = maze.get(r, c, turned(view.gaze, 1));
        cell.front = maze.get(r, c, view.gaze);
        view.cells.push_back(cell);
        if(cell.front != EdgeState::open)
          break;
        r += rowDelta[static_cast<std::size_t>(view.gaze)];
        c += colDelta[static_cast<std::size_t>(view.gaze)];
        if(!inside(r, c))
          break;
      }
      return true;
    }

    Vector2f toRobot(const View& view, const Vector2f& mazePoint)
    {
      return rotated(mazePoint - view.robot, -view.robotRotation);
    }

    bool observeTag(const View& view, const Camera& camera, float maxTagDistance, float maxTagViewAngle,
                    int cell, Heading face, float centreHeight, Pose3f& tagInRobot, float tagSize)
    {
      const Vector2f centre = cellCentreOf(cellRow(cell), cellCol(cell));
      const Vector2f normal = -directionOf(face); // points into the cell, out of the wall
      const Vector2f tag = centre + directionOf(face) * halfCellMm;

      // Close enough, inside the horizontal field of view of the camera.
      const Vector2f positionInRobot = toRobot(view, tag);
      const Vector2f toTag = positionInRobot - camera.positionInRobot;
      if(toTag.norm() > maxTagDistance)
        return false;
      // The whole tag, not only its centre, has to be in view: both side edges of the black square.
      const Vector2f along(-normal.y(), normal.x());
      const Vector2f edgeA = toRobot(view, tag + along * (tagSize / 2.f)) - camera.positionInRobot;
      const Vector2f edgeB = toRobot(view, tag - along * (tagSize / 2.f)) - camera.positionInRobot;
      for(const Vector2f& direction : {toTag, edgeA, edgeB})
      {
        const float bearing = Angle::normalize(std::atan2(direction.y(), direction.x()) - camera.yawInRobot);
        if(std::abs(bearing) > camera.openingAngleWidth / 2.f)
          return false;
      }

      // The tag faces the camera.
      const Vector2f toCamera = view.camera - tag;
      if(toCamera.norm() < 1.f || std::acos(std::clamp(normal.dot(toCamera.normalized()), -1.f, 1.f)) > maxTagViewAngle)
        return false;

      // Pose: x to the right and y up when looking at the tag, z along the normal.
      const Vector2f normal2D = rotated(normal, -view.robotRotation);
      const Vector3f normalInRobot(normal2D.x(), normal2D.y(), 0.f);
      Matrix3f axes;
      axes.col(0) = Vector3f(-normalInRobot.y(), normalInRobot.x(), 0.f);
      axes.col(1) = Vector3f(0.f, 0.f, 1.f);
      axes.col(2) = normalInRobot;
      tagInRobot.rotation = RotationMatrix(axes);
      tagInRobot.translation = Vector3f(positionInRobot.x(), positionInRobot.y(), centreHeight);
      return true;
    }

    bool lineOfSightClear(const MazeGraph& maze, const Vector2f& from, const Vector2f& to, int cell, Heading face)
    {
      const EdgeKey own = canonical(cellRow(cell), cellCol(cell), face);
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
          for(int h = 0; h < headingCount; ++h)
          {
            const Heading heading = static_cast<Heading>(h);
            const EdgeKey key = canonical(row, col, heading);
            // Every edge once: the shared ones from the north/west side, the south/east border from its cell.
            if(!(key == EdgeKey{row, col, heading}) || maze.get(row, col, heading) != EdgeState::wall || key == own)
              continue;
            const Vector2f direction = directionOf(heading);
            const Vector2f along(-direction.y(), direction.x());
            const Vector2f middle = cellCentreOf(row, col) + direction * halfCellMm;
            const float reach = halfCellMm + wallThicknessMm / 2.f;
            if(segmentsIntersect(from, to, middle - along * reach, middle + along * reach))
              return false;
          }
      return true;
    }

    std::vector<VisibleTag> visibleTags(const MazeGraph& maze, const MazeTagLayout& layout, const View& view,
                                        const Camera& camera, float maxTagDistance, float maxTagViewAngle)
    {
      std::vector<VisibleTag> tags;
      for(int cell = 0; cell < cellCount; ++cell)
        for(int h = 0; h < headingCount; ++h)
        {
          const Heading face = static_cast<Heading>(h);
          VisibleTag tag{cell, face, Pose3f()};
          if(!layout.hasTag(cell, face, maze) ||
             !observeTag(view, camera, maxTagDistance, maxTagViewAngle, cell, face, layout.centreHeight, tag.tagInRobot, layout.size))
            continue;
          const Vector2f centre = cellCentreOf(cellRow(cell), cellCol(cell)) + directionOf(face) * halfCellMm;
          const Vector2f along(-directionOf(face).y(), directionOf(face).x());
          const Vector2f halfTag = along * (layout.size / 2.f);
          if(lineOfSightClear(maze, view.camera, centre, cell, face) &&
             lineOfSightClear(maze, view.camera, centre + halfTag, cell, face) &&
             lineOfSightClear(maze, view.camera, centre - halfTag, cell, face))
            tags.push_back(tag);
        }
      return tags;
    }
  }
}
