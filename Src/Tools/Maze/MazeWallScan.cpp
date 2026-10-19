/**
 * @file Tools/Maze/MazeWallScan.cpp
 *
 * Implementation of the floor-to-wall boundary scan, see the header for the rules.
 */

#include "MazeWallScan.h"
#include <algorithm>

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

    std::vector<ColumnBoundary> scanColumns(const GreyImage& image, int horizonRow, const HeightProjection& projection, const Params& params)
    {
      std::vector<ColumnBoundary> result;
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
      const float floorReference = median(bottoms);

      const int top = std::max(horizonRow, k);
      for(std::size_t i = 0; i < result.size(); ++i)
      {
        const std::vector<float>& profile = profiles[i];
        std::vector<float> sums(profile.size() + 1, 0.f); // sums[y] is the sum of the rows 0 .. y - 1
        for(std::size_t y = 0; y < profile.size(); ++y)
          sums[y + 1] = sums[y] + profile[y];
        const auto above = [&](int y) { return (sums[static_cast<std::size_t>(y)] - sums[static_cast<std::size_t>(y - k)]) / static_cast<float>(k); };
        const auto below = [&](int y) { return (sums[static_cast<std::size_t>(y + k + 1)] - sums[static_cast<std::size_t>(y + 1)]) / static_cast<float>(k); };
        const auto step = [&](int y) { return above(y) - below(y); };

        int y = image.height - 1 - k;
        while(y >= top)
        {
          if(step(y) < params.minGradient)
          {
            --y;
            continue;
          }
          // Strongest row of the run of candidates, the lowest one on a tie.
          int best = y;
          float bestStep = step(y);
          int next = y - 1;
          for(; next >= top && step(next) >= params.minGradient; --next)
            if(step(next) > bestStep)
            {
              best = next;
              bestStep = step(next);
            }
          y = next;

          // The floor above has to be a wall: contrast to the floor reference, and wall up to the check height.
          if(above(best) - floorReference < params.minWallContrast)
            continue;
          int rowAtHeight = 0;
          if(!projection || !projection(result[i].column, best, params.minWallCheckHeightMm, rowAtHeight))
            continue;
          bool wall = true;
          for(int row = best - 1; row >= std::max(0, rowAtHeight) && wall; --row)
            wall = profile[static_cast<std::size_t>(row)] - floorReference >= params.minWallContrast / 2.f;
          if(!wall)
            continue;
          result[i].row = best;
          result[i].strength = bestStep;
          break;
        }
      }
      return result;
    }
  }
}
