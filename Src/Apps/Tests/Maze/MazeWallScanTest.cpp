#include "Tools/Maze/MazeWallScan.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
  using namespace Maze::WallScan;

  constexpr int width = 320, height = 240;
  constexpr float focal = 270.f, cx = width / 2.f, cy = height / 2.f;
  constexpr float cameraHeight = 480.f;      // mm
  constexpr float wallHeight = 600.f;        // mm
  constexpr float pitch = 20.f * 3.14159265f / 180.f; // looking down

  /** A floor patch (x in mm ahead of the camera) of its own brightness. */
  struct Patch
  {
    float from, to;
    unsigned char brightness;
  };

  struct Scene
  {
    float wallX = 1500.f;
    unsigned char floorBrightness = 100, wallBrightness = 200, aboveWallBrightness = 200;
    std::vector<Patch> patches;
    int noise = 0;   // peak noise in grey levels
  };

  /** Pinhole camera, 480 mm high, pitched down, in the origin looking along +x. All maths in mm. */
  struct Pinhole
  {
    float forward[3] = {std::cos(pitch), 0.f, -std::sin(pitch)};
    float up[3] = {std::sin(pitch), 0.f, std::cos(pitch)};

    /** Floor point of a pixel; false above the horizon. */
    bool floorPoint(float u, float v, float& x, float& y) const
    {
      const float dir[3] = {forward[0] * focal - 0.f - up[0] * (v - cy), -(u - cx), forward[2] * focal - up[2] * (v - cy)};
      if(dir[2] >= 0.f)
        return false;
      const float t = cameraHeight / -dir[2];
      x = dir[0] * t;
      y = dir[1] * t;
      return true;
    }

    /** Pixel row of the world point (x, y, z). */
    float rowOf(float x, float y, float z) const
    {
      const float r[3] = {x, y, z - cameraHeight};
      const float depth = r[0] * forward[0] + r[2] * forward[2];
      return cy - (r[0] * up[0] + r[2] * up[2]) * focal / depth;
    }
  };

  std::vector<unsigned char> render(const Scene& scene)
  {
    const Pinhole camera;
    std::vector<unsigned char> image(width * height);
    std::uint32_t state = 12345;
    for(int v = 0; v < height; ++v)
      for(int u = 0; u < width; ++u)
      {
        const float dir[3] = {camera.forward[0] * focal - camera.up[0] * (v - cy), -(u - cx), camera.forward[2] * focal - camera.up[2] * (v - cy)};
        unsigned char value = scene.aboveWallBrightness;
        float t = dir[2] < 0.f ? cameraHeight / -dir[2] : 1e9f;
        const float floorX = dir[0] * t;
        if(floorX <= scene.wallX && dir[2] < 0.f)
        {
          value = scene.floorBrightness;
          for(const Patch& patch : scene.patches)
            if(floorX >= patch.from && floorX <= patch.to)
              value = patch.brightness;
        }
        else if(dir[0] > 0.f)
        {
          const float z = cameraHeight + dir[2] * scene.wallX / dir[0];
          value = z <= wallHeight ? scene.wallBrightness : scene.aboveWallBrightness;
        }
        state = state * 1664525u + 1013904223u;
        const int jitter = scene.noise ? static_cast<int>((state >> 16) % static_cast<unsigned>(2 * scene.noise + 1)) - scene.noise : 0;
        image[static_cast<std::size_t>(v * width + u)] = static_cast<unsigned char>(std::clamp(static_cast<int>(value) + jitter, 0, 255));
      }
    return image;
  }

  /** The point `heightMm` above a floor pixel. */
  HeightProjection projectionOf()
  {
    return [](int column, int row, float heightMm, int& rowAtHeight)
    {
      float x, y;
      if(!Pinhole().floorPoint(static_cast<float>(column), static_cast<float>(row), x, y))
        return false;
      rowAtHeight = static_cast<int>(std::lround(Pinhole().rowOf(x, y, heightMm)));
      return true;
    };
  }

  int horizon()
  {
    return static_cast<int>(cy - focal * std::tan(pitch)) + 4;
  }

  std::vector<ColumnBoundary> scan(const Scene& scene, const Params& params = Params())
  {
    const std::vector<unsigned char> image = render(scene);
    return scanColumns({image.data(), width, height, width}, horizon(), projectionOf(), params);
  }

  /** Row of the last floor row of a wall at `wallX`, seen in `column`. */
  int expectedRow(float wallX, int column)
  {
    // The boundary pixel is the last one whose floor point lies before the wall.
    int row = height - 1;
    float x, y;
    while(row > 0 && Pinhole().floorPoint(static_cast<float>(column), static_cast<float>(row - 1), x, y) && x <= wallX)
      --row;
    return row;
  }
}

GTEST_TEST(MazeWallScan, FindsWallBoundaryInEveryColumn)
{
  for(const float wallX : {800.f, 1200.f, 1500.f, 2000.f})
  {
    Scene scene;
    scene.wallX = wallX;
    const std::vector<ColumnBoundary> boundaries = scan(scene);
    ASSERT_FALSE(boundaries.empty());
    for(const ColumnBoundary& boundary : boundaries)
    {
      ASSERT_TRUE(boundary.found()) << "wall at " << wallX << ", column " << boundary.column;
      EXPECT_NEAR(boundary.row, expectedRow(wallX, boundary.column), 1) << "wall at " << wallX << ", column " << boundary.column;
    }
  }
}

GTEST_TEST(MazeWallScan, NoWallNoBoundary)
{
  Scene scene;
  scene.wallX = 1e7f; // the floor reaches the horizon
  scene.aboveWallBrightness = scene.floorBrightness;
  for(const ColumnBoundary& boundary : scan(scene))
    EXPECT_FALSE(boundary.found());
}

GTEST_TEST(MazeWallScan, ToleratesImageNoise)
{
  Scene scene;
  scene.noise = 4;
  for(const ColumnBoundary& boundary : scan(scene))
  {
    ASSERT_TRUE(boundary.found());
    EXPECT_NEAR(boundary.row, expectedRow(scene.wallX, boundary.column), 2);
  }
}

GTEST_TEST(MazeWallScan, DarkGoalPatchIsNotAWall)
{
  // The patch is darker than the floor: its far edge is a dark-to-bright step, but floor follows, not a wall.
  Scene scene;
  scene.patches.push_back({700.f, 1100.f, 50});
  scene.wallX = 2500.f;
  for(const ColumnBoundary& boundary : scan(scene))
  {
    ASSERT_TRUE(boundary.found()) << "column " << boundary.column;
    EXPECT_NEAR(boundary.row, expectedRow(2500.f, boundary.column), 1) << "column " << boundary.column;
  }

  scene.wallX = 1e7f;
  scene.aboveWallBrightness = scene.floorBrightness;
  for(const ColumnBoundary& boundary : scan(scene))
    EXPECT_FALSE(boundary.found()) << "column " << boundary.column;

  // Without the floor reference and the extent check the far edge of the patch would be taken for a wall.
  Params lax;
  lax.minWallContrast = -255.f;
  lax.minWallCheckHeightMm = 0.f;
  for(const ColumnBoundary& boundary : scan(scene, lax))
  {
    ASSERT_TRUE(boundary.found()) << "column " << boundary.column;
    EXPECT_NEAR(boundary.row, expectedRow(1100.f, boundary.column), 1);
  }
}

GTEST_TEST(MazeWallScan, NarrowBrightMarkingIsNotAWall)
{
  Scene scene;
  scene.patches.push_back({900.f, 930.f, 200});
  scene.wallX = 1e7f;
  scene.aboveWallBrightness = scene.floorBrightness;
  for(const ColumnBoundary& boundary : scan(scene))
    EXPECT_FALSE(boundary.found()) << "column " << boundary.column;
}

GTEST_TEST(MazeWallScan, SameColourWallWithShadingContrast)
{
  // Floor and wall painted the same, the wall only a bit brighter: detected from the minimum contrast on.
  Scene scene;
  scene.wallBrightness = scene.aboveWallBrightness = scene.floorBrightness + 14;
  for(const ColumnBoundary& boundary : scan(scene))
  {
    ASSERT_TRUE(boundary.found()) << "column " << boundary.column;
    EXPECT_NEAR(boundary.row, expectedRow(scene.wallX, boundary.column), 1);
  }

  scene.wallBrightness = scene.aboveWallBrightness = scene.floorBrightness + 6;
  for(const ColumnBoundary& boundary : scan(scene))
    EXPECT_FALSE(boundary.found());
}

GTEST_TEST(MazeWallScan, StepOfColumnsAndHorizonRow)
{
  Scene scene;
  Params params;
  params.columnStep = 40;
  const std::vector<ColumnBoundary> boundaries = scan(scene, params);
  ASSERT_EQ(boundaries.size(), static_cast<std::size_t>(width / 40));
  EXPECT_EQ(boundaries.front().column, 20);
  EXPECT_EQ(boundaries[1].column, 60);

  // A boundary above the horizon row is not searched.
  const std::vector<unsigned char> image = render(scene);
  for(const ColumnBoundary& boundary : scanColumns({image.data(), width, height, width}, height - 20, projectionOf(), Params()))
    EXPECT_FALSE(boundary.found());
}

GTEST_TEST(MazeWallScan, ReportsTheFloorBrightnessAtTheBottom)
{
  Scene scene;
  const std::vector<unsigned char> image = render(scene);
  float floorReference = -1.f;
  for(const ColumnBoundary& boundary : scanColumns({image.data(), width, height, width}, horizon(), projectionOf(), Params(), &floorReference))
    EXPECT_NEAR(boundary.bottom, 100.f, 0.5f);
  EXPECT_NEAR(floorReference, 100.f, 0.5f);

  // A wall that fills the bottom of the image: no boundary, and the bottom is as bright as the wall.
  scene.wallX = 100.f;
  const std::vector<unsigned char> close = render(scene);
  for(const ColumnBoundary& boundary : scanColumns({close.data(), width, height, width}, horizon(), projectionOf(), Params(), &floorReference))
  {
    EXPECT_FALSE(boundary.found());
    EXPECT_NEAR(boundary.bottom, 200.f, 0.5f);
  }
}

GTEST_TEST(MazeWallScan, WallDarkerThanFloorNeedsThePolarity)
{
  // The simulator lights the horizontal floor more than the vertical walls.
  Scene scene;
  scene.floorBrightness = 200;
  scene.wallBrightness = scene.aboveWallBrightness = 100;
  for(const ColumnBoundary& boundary : scan(scene))
    EXPECT_FALSE(boundary.found()) << "column " << boundary.column;

  Params darker;
  darker.wallBrighterThanFloor = false;
  for(const ColumnBoundary& boundary : scan(scene, darker))
  {
    ASSERT_TRUE(boundary.found()) << "column " << boundary.column;
    EXPECT_NEAR(boundary.row, expectedRow(scene.wallX, boundary.column), 1);
  }
}
