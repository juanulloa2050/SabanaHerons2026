/**
 * @file Tools/Maze/MazeWallScan.cpp
 *
 * Implementation of the floor-to-wall boundary scan, see the header for the rules.
 */

#include "MazeWallScan.h"
#include <algorithm>
#include <cmath>

namespace Maze
{
  namespace WallScan
  {
    namespace
    {
      /** Mean brightness of one image row around `column`. */
      float rowBrightness(const GreyImage& image, int column, int row, int halfWidth)
      {
        const int first = std::max(0, column - halfWidth), last = std::min(image.width - 1, column + halfWidth);
        float sum = 0.f;
        const unsigned char* line = image.data + static_cast<std::size_t>(row) * static_cast<std::size_t>(image.stride);
        for(int x = first; x <= last; ++x)
          sum += static_cast<float>(line[x]);
        return sum / static_cast<float>(last - first + 1);
      }

      std::vector<float> columnProfile(const GreyImage& image, int column, int halfWidth)
      {
        std::vector<float> profile(static_cast<std::size_t>(image.height));
        for(int y = 0; y < image.height; ++y)
          profile[static_cast<std::size_t>(y)] = rowBrightness(image, column, y, halfWidth);
        return profile;
      }

      float median(std::vector<float> values)
      {
        if(values.empty())
          return 0.f;
        const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
        std::nth_element(values.begin(), middle, values.end());
        return *middle;
      }
    }

    std::vector<ColumnBoundary> scanColumns(const GreyImage& image, int horizonRow, const HeightProjection& projection, const Params& params, float* floorReference)
    {
      std::vector<ColumnBoundary> result;
      if(floorReference)
        *floorReference = 0.f;
      const int k = std::max(1, params.smoothRows);
      if(!image.data || image.width <= 0 || image.height < 2 * k + 2 || params.columnStep <= 0)
        return result;

      std::vector<std::vector<float>> profiles;
      for(int column = params.columnStep / 2; column < image.width; column += params.columnStep)
      {
        result.push_back({column, -1, 0.f});
        profiles.push_back(columnProfile(image, column, params.columnHalfWidth));
      }

      // Floor reference: the median over the columns of the mean of the bottom rows.
      std::vector<float> bottoms;
      const int referenceRows = std::clamp(params.floorReferenceRows, 1, image.height);
      for(const std::vector<float>& profile : profiles)
      {
        float sum = 0.f;
        for(int y = image.height - referenceRows; y < image.height; ++y)
          sum += profile[static_cast<std::size_t>(y)];
        bottoms.push_back(sum / static_cast<float>(referenceRows));
      }
      const float floorBrightness = median(bottoms);
      if(floorReference)
        *floorReference = floorBrightness;
      for(std::size_t i = 0; i < result.size(); ++i)
        result[i].bottom = bottoms[i];

      const int top = std::max(horizonRow, k);
      for(std::size_t i = 0; i < result.size(); ++i)
      {
        // The floor is visible at the bottom of the column; otherwise there is no boundary (the floor is below the image, and
        // the steps in the column are those of a wall, such as the edges of an AprilTag).
        if(std::abs(result[i].bottom - floorBrightness) > params.maxFloorDeviation)
          continue;
        const std::vector<float>& profile = profiles[i];
        std::vector<float> sums(profile.size() + 1, 0.f); // sums[y] is the sum of the rows 0 .. y - 1
        for(std::size_t y = 0; y < profile.size(); ++y)
          sums[y + 1] = sums[y] + profile[y];
        const auto above = [&](int y) { return (sums[static_cast<std::size_t>(y)] - sums[static_cast<std::size_t>(y - k)]) / static_cast<float>(k); };
        const auto below = [&](int y) { return (sums[static_cast<std::size_t>(y + k + 1)] - sums[static_cast<std::size_t>(y + 1)]) / static_cast<float>(k); };
        const auto step = [&](int y) { return above(y) - below(y); };
        // The floor under a boundary, `k` rows farther down than `below`: an edge is a ramp of several rows in the image, and its
        // lower rows are not floor yet.
        const auto floorBelow = [&](int y)
        {
          const int first = std::min(y + 1 + k, image.height - k);
          return (sums[static_cast<std::size_t>(first + k)] - sums[static_cast<std::size_t>(first)]) / static_cast<float>(k);
        };

        int y = image.height - 1 - k;
        while(y >= top)
        {
          if(std::abs(step(y)) < params.minGradient)
          {
            --y;
            continue;
          }
          // Strongest row of the run of candidates with the same sign, the lowest one on a tie.
          const float sign = step(y) > 0.f ? 1.f : -1.f; // the direction in which the wall differs from the floor
          int best = y;
          float bestStep = sign * step(y);
          int next = y - 1;
          for(; next >= top && sign * step(next) >= params.minGradient; --next)
            if(sign * step(next) > bestStep)
            {
              best = next;
              bestStep = sign * step(next);
            }
          y = next;

          // From here on the column holds something that is no free floor, whether or not it turns out to be a wall boundary.
          result[i].rejected = true;
          // The floor above has to be a wall: contrast to the floor reference in the direction of the step ...
          if(sign * (above(best) - floorBrightness) < params.minWallContrast)
            continue;
          // The floor lies below the boundary: a step between two parts of a wall (the edge of an AprilTag) is none, and in
          // a column whose floor is below the bottom of the image there is no boundary at all.
          if(std::abs(floorBelow(best) - floorBrightness) > params.maxFloorDeviation)
            continue;
          // ... and the column stays wall up to the check height: enough of the rows keep that contrast. Rows beyond the
          // image are not checked, but a minimum number has to remain.
          int rowAtHeight = 0;
          if(!projection || !projection(result[i].column, best, params.minWallCheckHeightMm, rowAtHeight))
            continue;
          const int topRow = std::clamp(rowAtHeight, 0, best);
          const int rows = best - topRow;
          if(rows < params.minCheckRows)
            continue;
          // Rows that do not look like floor count as wall, whatever their direction: a black tag on a light wall is no floor.
          int wallRows = 0, floorRun = 0, longestFloorRun = 0;
          for(int row = best - 1; row >= topRow; --row)
          {
            const bool floorLike = std::abs(profile[static_cast<std::size_t>(row)] - floorBrightness) < params.minWallContrast / 2.f;
            wallRows += !floorLike;
            floorRun = floorLike ? floorRun + 1 : 0;
            longestFloorRun = std::max(longestFloorRun, floorRun);
          }
          if(static_cast<float>(wallRows) < params.minWallRowFraction * static_cast<float>(rows) || longestFloorRun > params.maxFloorRunRows)
            continue;
          result[i].row = best;
          result[i].strength = bestStep;
          result[i].rejected = false;
          break;
        }
      }
      return result;
    }

    int horizonRow(int imageHeight, const std::function<bool(int row, float& distanceMm)>& floorDistance, float maxDistanceMm)
    {
      int top = -1;
      for(int row = imageHeight - 1; row >= 0; --row)
      {
        float distance = 0.f;
        if(!floorDistance(row, distance) || distance > maxDistanceMm)
          break;
        top = row;
      }
      return top;
    }

    void FloorModel::reset()
    {
      learned = false;
    }

    void FloorModel::observe(const std::vector<ColumnBoundary>& columns, float floorReference, unsigned timeMs)
    {
      const std::size_t found = static_cast<std::size_t>(std::count_if(columns.begin(), columns.end(), [](const ColumnBoundary& column) { return column.found(); }));
      if(found == 0 || found * 2 < columns.size())
        return;
      floor = known(timeMs) ? floor + params.trackingRate * (floorReference - floor) : floorReference;
      learned = true;
      lastLessonMs = timeMs;
    }

    bool FloorModel::known(unsigned timeMs) const
    {
      return learned && timeMs - lastLessonMs <= params.maxAgeMs;
    }

    bool FloorModel::isFloor(float bottomBrightness, unsigned timeMs) const
    {
      return known(timeMs) && std::abs(bottomBrightness - floor) <= params.tolerance;
    }
  }
}
