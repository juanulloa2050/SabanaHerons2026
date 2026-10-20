/**
 * @file Tools/Maze/MazeWallView.cpp
 *
 * Implementation of the wall look-ahead from boundary points, see the header for the rules.
 */

#include "MazeWallView.h"
#include "MazeEdgeIndex.h"
#include "MazeOracle.h"
#include "Math/Angle.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Maze
{
  namespace WallView
  {
    namespace
    {
      using namespace Oracle;

      constexpr float quarterTurn = pi / 2.f;
      constexpr float extentMm = size * cellSizeMm;

      /** Number of snapped points per edge, in the layout of `EdgeIndex`. */
      struct Support
      {
        std::array<int, EdgeIndex::horizontalCount> horizontal{};
        std::array<int, EdgeIndex::verticalCount> vertical{};
      };

      /**
       * Snaps a point of the maze frame to the nearest grid line and counts it there, or drops it. A point closer
       * than `vertexMarginMm` to a grid vertex is dropped too: there the line it belongs to is ambiguous.
       */
      void addPoint(Support& support, const Vector2f& point, float toleranceMm, float vertexMarginMm)
      {
        if(point.x() < -toleranceMm || point.x() > extentMm + toleranceMm || point.y() < -toleranceMm || point.y() > extentMm + toleranceMm)
          return;
        const float column = point.x() / cellSizeMm, row = (extentMm - point.y()) / cellSizeMm; // row 0 is the top border
        const int verticalLine = std::clamp(static_cast<int>(std::lround(column)), 0, size);
        const int horizontalLine = std::clamp(static_cast<int>(std::lround(row)), 0, size);
        const float verticalDistance = std::abs(column - static_cast<float>(verticalLine)) * cellSizeMm;
        const float horizontalDistance = std::abs(row - static_cast<float>(horizontalLine)) * cellSizeMm;
        if(std::min(verticalDistance, horizontalDistance) > toleranceMm)
          return;
        const float alongVertical = std::abs(row - std::round(row)) * cellSizeMm;       // distance to the next vertex along a vertical line
        const float alongHorizontal = std::abs(column - std::round(column)) * cellSizeMm;
        if(verticalDistance <= horizontalDistance)
        {
          if(alongVertical >= vertexMarginMm)
            ++support.vertical[static_cast<std::size_t>(std::clamp(static_cast<int>(std::floor(row)), 0, size - 1) * (size + 1) + verticalLine)];
        }
        else if(alongHorizontal >= vertexMarginMm)
          ++support.horizontal[static_cast<std::size_t>(horizontalLine * size + std::clamp(static_cast<int>(std::floor(column)), 0, size - 1))];
      }

      int supportOf(const Support& support, int row, int col, Heading heading)
      {
        const EdgeIndex::Slot slot = EdgeIndex::slotOf(row, col, heading);
        if(!slot.valid)
          return 0;
        return slot.horizontal ? support.horizontal[slot.index] : support.vertical[slot.index];
      }

      float cross(const Vector2f& a, const Vector2f& b)
      {
        return a.x() * b.y() - a.y() * b.x();
      }

      /**
       * Whether the line from `camera` to `point` crosses the segment a to b and the point is at least `clearanceMm`
       * farther than the crossing.
       */
      bool seesPast(const Vector2f& camera, const Vector2f& point, const Vector2f& a, const Vector2f& b, float clearanceMm)
      {
        const Vector2f r = point - camera, s = b - a;
        const float denominator = cross(r, s);
        if(std::abs(denominator) < 1e-6f)
          return false;
        const float t = cross(a - camera, s) / denominator;
        const float u = cross(a - camera, r) / denominator;
        return t > 0.f && t < 1.f && u >= 0.f && u <= 1.f && (1.f - t) * r.norm() >= clearanceMm;
      }

      /** Whether the edge lies between two cells of the goal block. */
      bool insideGoalBlock(int row, int col, Heading heading)
      {
        return isGoal(row, col) && isGoal(row + rowDelta[static_cast<std::size_t>(heading)], col + colDelta[static_cast<std::size_t>(heading)]);
      }

      EdgeState classify(const Support& support, const std::vector<Vector2f>& points, const Vector2f& camera,
                         int row, int col, Heading heading, const Params& params)
      {
        // The goal cells have no walls between them, whatever the points say (the colour of the goal patch gives edges).
        const bool noWall = params.noGoalBlockWalls && insideGoalBlock(row, col, heading);
        if(!noWall && supportOf(support, row, col, heading) >= params.minSupportPoints)
          return EdgeState::wall;
        const Vector2f direction = directionOf(heading);
        const Vector2f along(-direction.y(), direction.x());
        const Vector2f middle = cellCentreOf(row, col) + direction * halfCellMm;
        const float reach = halfCellMm - params.vertexMarginMm;
        for(const Vector2f& point : points)
          if(seesPast(camera, point, middle - along * reach, middle + along * reach, params.openClearanceMm))
            return EdgeState::open;
        return EdgeState::unknown;
      }
    }

    bool computeWallView(const std::vector<Vector2f>& boundaryPointsInRobot, const std::vector<Vector2f>& clearPointsInRobot,
                         const Pose2f& robotInMaze, const Vector2f& cameraInRobot, float gazeYawInRobot, int range, const Params& params, Result& result)
    {
      result = Result();
      const Vector2f robot = robotInMaze.translation;
      const float rotation = robotInMaze.rotation;
      int row = 0, col = 0;
      if(!cellContaining(robot, row, col))
        return false;

      const Heading gaze = snapToHeading(Angle::normalize(rotation + gazeYawInRobot));
      int turns = static_cast<int>(std::lround(Angle::normalize(angleOf(gaze) - rotation) / quarterTurn));
      if(turns < -1)
        turns += 4;
      result.lookAheadTurns = turns;

      Support support;
      std::vector<Vector2f> points; // every point that can show open edges
      for(const Vector2f& pointInRobot : boundaryPointsInRobot)
      {
        points.push_back(robot + rotated(pointInRobot, rotation));
        addPoint(support, points.back(), params.snapToleranceMm, params.vertexMarginMm);
      }
      for(const Vector2f& pointInRobot : clearPointsInRobot)
        points.push_back(robot + rotated(pointInRobot, rotation));
      const Vector2f camera = robot + rotated(cameraInRobot, rotation);

      const Vector2f forward = directionOf(gaze);
      const Heading leftHeading = turned(gaze, -1), rightHeading = turned(gaze, 1);
      for(int ahead = 0; ahead <= range; ++ahead)
      {
        Cell cell;
        cell.ahead = ahead;
        cell.left = classify(support, points, camera, row, col, leftHeading, params);
        cell.right = classify(support, points, camera, row, col, rightHeading, params);
        cell.front = classify(support, points, camera, row, col, gaze, params);
        const Vector2f centre = cellCentreOf(row, col);
        cell.distance = std::max(0.f, (centre - robot).norm());
        result.cellsAhead.push_back(cell);

        if(ahead == 0)
        {
          if(cell.left == EdgeState::wall)
            result.leftDistance = std::max(0.f, (centre + directionOf(leftHeading) * halfCellMm - robot).dot(directionOf(leftHeading)));
          if(cell.right == EdgeState::wall)
            result.rightDistance = std::max(0.f, (centre + directionOf(rightHeading) * halfCellMm - robot).dot(directionOf(rightHeading)));
        }
        const bool last = cell.front != EdgeState::open || ahead == range || !inside(row + rowDelta[static_cast<std::size_t>(gaze)], col + colDelta[static_cast<std::size_t>(gaze)]);
        if(last && cell.front == EdgeState::wall)
          result.frontDistance = std::max(0.f, (centre + forward * halfCellMm - robot).dot(forward));

        // The 500 mm edge of every wall, in the robot frame.
        const std::pair<Heading, EdgeState> edges[] = {{leftHeading, cell.left}, {rightHeading, cell.right}, {gaze, cell.front}};
        for(const auto& [heading, state] : edges)
        {
          if(state != EdgeState::wall)
            continue;
          const Vector2f direction = directionOf(heading);
          const Vector2f along(-direction.y(), direction.x());
          const Vector2f middle = centre + direction * halfCellMm;
          result.segments.push_back({rotated(middle - along * halfCellMm - robot, -rotation), rotated(middle + along * halfCellMm - robot, -rotation)});
        }

        if(last)
          break;
        row += rowDelta[static_cast<std::size_t>(gaze)];
        col += colDelta[static_cast<std::size_t>(gaze)];
      }
      return true;
    }
  }
}
