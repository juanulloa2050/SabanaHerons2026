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
    float tagLow = 0.f, tagHigh = 0.f; // a band of tag brightness on the wall between these heights, none if equal
    unsigned char tagBrightness = 20;
    int noise = 0;       // peak noise in grey levels
    float pitchDeg = 20; // camera pitch down
  };

  /** Pinhole camera, 480 mm high, pitched down, in the origin looking along +x. All maths in mm. */
  struct Pinhole
  {
    explicit Pinhole(float pitchDegrees)
    {
      const float pitch = pitchDegrees * 3.14159265f / 180.f;
      forward[0] = std::cos(pitch);
      forward[2] = -std::sin(pitch);
      up[0] = std::sin(pitch);
      up[2] = std::cos(pitch);
    }

    float forward[3] = {0.f, 0.f, 0.f};
    float up[3] = {0.f, 0.f, 0.f};

    /** Floor point of a pixel; false above the horizon. */
    bool floorPoint(float u, float v, float& x, float& y) const
    {
      const float dir[3] = {forward[0] * focal - up[0] * (v - cy), -(u - cx), forward[2] * focal - up[2] * (v - cy)};
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
    const Pinhole camera(scene.pitchDeg);
    std::vector<unsigned char> image(width * height);
    std::uint32_t state = 12345;
    for(int v = 0; v < height; ++v)
      for(int u = 0; u < width; ++u)
      {
        const float dir[3] = {camera.forward[0] * focal - camera.up[0] * (v - cy), -(u - cx), camera.forward[2] * focal - camera.up[2] * (v - cy)};
        unsigned char value = scene.aboveWallBrightness;
        const float t = dir[2] < 0.f ? cameraHeight / -dir[2] : 1e9f;
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
          if(z >= scene.tagLow && z <= scene.tagHigh)
            value = scene.tagBrightness;
        }
        state = state * 1664525u + 1013904223u;
        const int jitter = scene.noise ? static_cast<int>((state >> 16) % static_cast<unsigned>(2 * scene.noise + 1)) - scene.noise : 0;
        image[static_cast<std::size_t>(v * width + u)] = static_cast<unsigned char>(std::clamp(static_cast<int>(value) + jitter, 0, 255));
      }
    return image;
  }

  /** The point `heightMm` above a floor pixel. */
  HeightProjection projectionOf(float pitchDeg)
  {
    return [pitchDeg](int column, int row, float heightMm, int& rowAtHeight)
    {
      float x, y;
      const Pinhole camera(pitchDeg);
      if(!camera.floorPoint(static_cast<float>(column), static_cast<float>(row), x, y))
        return false;
      rowAtHeight = static_cast<int>(std::lround(camera.rowOf(x, y, heightMm)));
      return true;
    };
  }

  int horizon(float pitchDeg = 20.f)
  {
    return static_cast<int>(cy - focal * std::tan(pitchDeg * 3.14159265f / 180.f)) + 4;
  }

  std::vector<ColumnBoundary> scan(const Scene& scene, const Params& params = Params())
  {
    const std::vector<unsigned char> image = render(scene);
    return scanColumns({image.data(), width, height, width}, horizon(scene.pitchDeg), projectionOf(scene.pitchDeg), params);
  }

  /** Row of the last floor row of a wall at `wallX`, seen in `column`. */
  int expectedRow(float wallX, int column, float pitchDeg = 20.f)
  {
    // The boundary pixel is the last one whose floor point lies before the wall.
    int row = height - 1;
    float x, y;
    const Pinhole camera(pitchDeg);
    while(row > 0 && camera.floorPoint(static_cast<float>(column), static_cast<float>(row - 1), x, y) && x <= wallX)
      --row;
    return row;
  }

  /** The two polarities: a light wall on a darker floor, and the simulator's darker wall on a lighter floor. */
  Scene polarity(bool wallBrighter)
  {
    Scene scene;
    scene.floorBrightness = wallBrighter ? 100 : 200;
    scene.wallBrightness = scene.aboveWallBrightness = wallBrighter ? 200 : 100;
    return scene;
  }

  /** A camera pitch that sees the near edge of a patch at `distance` (mm) and its far edge 1000 mm behind. */
  float pitchFor(float distance)
  {
    return distance <= 300.f ? 40.f : distance <= 500.f ? 35.f : 20.f; // lowest floor row at 235, 290 and 500 mm
  }

  int countFound(const std::vector<ColumnBoundary>& boundaries)
  {
    return static_cast<int>(std::count_if(boundaries.begin(), boundaries.end(), [](const ColumnBoundary& boundary) { return boundary.found(); }));
  }
}

GTEST_TEST(MazeWallScan, FindsWallBoundaryInEveryColumnForBothPolarities)
{
  for(const bool wallBrighter : {true, false})
    for(const float wallX : {800.f, 1200.f, 1500.f, 2000.f})
    {
      Scene scene = polarity(wallBrighter);
      scene.wallX = wallX;
      const std::vector<ColumnBoundary> boundaries = scan(scene);
      ASSERT_FALSE(boundaries.empty());
      for(const ColumnBoundary& boundary : boundaries)
      {
        ASSERT_TRUE(boundary.found()) << "wall at " << wallX << ", column " << boundary.column << ", brighter " << wallBrighter;
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
  for(const bool wallBrighter : {true, false})
  {
    Scene scene = polarity(wallBrighter);
    scene.noise = 4;
    for(const ColumnBoundary& boundary : scan(scene))
    {
      ASSERT_TRUE(boundary.found());
      EXPECT_NEAR(boundary.row, expectedRow(scene.wallX, boundary.column), 2);
    }
  }
}

GTEST_TEST(MazeWallScan, FarEdgeOfADarkGoalPatchIsNotAWall)
{
  // A patch darker than the floor: its far edge is a dark-to-bright step, but floor follows, not a wall.
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

  // Without the floor reference and the extent check the near edge of the patch would be taken for a wall.
  Params lax;
  lax.minWallContrast = -255.f;
  lax.minWallRowFraction = 0.f;
  lax.minCheckRows = 0;
  for(const ColumnBoundary& boundary : scan(scene, lax))
  {
    ASSERT_TRUE(boundary.found()) << "column " << boundary.column;
    EXPECT_NEAR(boundary.row, expectedRow(700.f, boundary.column), 1);
  }
}

GTEST_TEST(MazeWallScan, GoalPatchWithTheGreyOfTheWallIsRejectedAtNearDistances)
{
  // The patch is 1000 mm deep and has the grey of the wall. The minimum distance is set by the camera: the near edge has to be
  // more than the floor reference rows (8) above the lowest visible floor row, and the far edge has to be in the picture.
  for(const bool wallBrighter : {true, false})
    for(const float distance : {300.f, 500.f, 1000.f})
    {
      Scene scene = polarity(wallBrighter);
      scene.pitchDeg = pitchFor(distance);
      scene.patches.push_back({distance, distance + 1000.f, scene.wallBrightness});
      scene.wallX = 1e7f; // no wall at all
      scene.aboveWallBrightness = scene.floorBrightness;
      const std::vector<ColumnBoundary> boundaries = scan(scene);
      ASSERT_FALSE(boundaries.empty());
      EXPECT_EQ(countFound(boundaries), 0) << "patch at " << distance << ", brighter " << wallBrighter;
    }
}

GTEST_TEST(MazeWallScan, WallBehindTheGoalPatchIsFoundAtTheWall)
{
  for(const bool wallBrighter : {true, false})
    for(const float distance : {300.f, 500.f, 1000.f})
    {
      Scene scene = polarity(wallBrighter);
      scene.pitchDeg = pitchFor(distance);
      scene.patches.push_back({distance, distance + 1000.f, scene.wallBrightness});
      scene.wallX = distance + 1300.f; // 300 mm of floor between the patch and the wall
      const std::vector<ColumnBoundary> boundaries = scan(scene);
      for(const ColumnBoundary& boundary : boundaries)
      {
        ASSERT_TRUE(boundary.found()) << "patch at " << distance << ", column " << boundary.column;
        EXPECT_NEAR(boundary.row, expectedRow(scene.wallX, boundary.column, scene.pitchDeg), 1) << "patch at " << distance << ", column " << boundary.column;
      }
    }
}

GTEST_TEST(MazeWallScan, WallWithATagIsAccepted)
{
  // A black band between 240 and 350 mm over the whole wall: the worst case of an AprilTag.
  for(const bool wallBrighter : {true, false})
    for(const float wallX : {600.f, 1000.f, 2000.f})
    {
      Scene scene = polarity(wallBrighter);
      scene.wallX = wallX;
      scene.tagLow = 240.f;
      scene.tagHigh = 350.f;
      scene.tagBrightness = wallBrighter ? 20 : 240;
      scene.pitchDeg = 30.f;
      for(const ColumnBoundary& boundary : scan(scene))
      {
        ASSERT_TRUE(boundary.found()) << "wall at " << wallX << ", column " << boundary.column << ", brighter " << wallBrighter;
        EXPECT_NEAR(boundary.row, expectedRow(wallX, boundary.column, 30.f), 1);
      }
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
  // Floor and wall painted the same, the wall only a bit different: detected from the minimum contrast on.
  for(const int sign : {1, -1})
  {
    Scene scene;
    scene.wallBrightness = scene.aboveWallBrightness = static_cast<unsigned char>(scene.floorBrightness + sign * 14);
    for(const ColumnBoundary& boundary : scan(scene))
    {
      ASSERT_TRUE(boundary.found()) << "column " << boundary.column;
      EXPECT_NEAR(boundary.row, expectedRow(scene.wallX, boundary.column), 1);
    }

    scene.wallBrightness = scene.aboveWallBrightness = static_cast<unsigned char>(scene.floorBrightness + sign * 6);
    for(const ColumnBoundary& boundary : scan(scene))
      EXPECT_FALSE(boundary.found());
  }
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
  for(const ColumnBoundary& boundary : scanColumns({image.data(), width, height, width}, height - 20, projectionOf(scene.pitchDeg), Params()))
    EXPECT_FALSE(boundary.found());
}

GTEST_TEST(MazeWallScan, ReportsTheFloorBrightnessAtTheBottom)
{
  Scene scene;
  const std::vector<unsigned char> image = render(scene);
  float floorReference = -1.f;
  for(const ColumnBoundary& boundary : scanColumns({image.data(), width, height, width}, horizon(), projectionOf(scene.pitchDeg), Params(), &floorReference))
    EXPECT_NEAR(boundary.bottom, 100.f, 0.5f);
  EXPECT_NEAR(floorReference, 100.f, 0.5f);

  // A wall that fills the bottom of the image: no boundary, and the bottom is as bright as the wall.
  scene.wallX = 100.f;
  const std::vector<unsigned char> close = render(scene);
  for(const ColumnBoundary& boundary : scanColumns({close.data(), width, height, width}, horizon(), projectionOf(scene.pitchDeg), Params(), &floorReference))
  {
    EXPECT_FALSE(boundary.found());
    EXPECT_NEAR(boundary.bottom, 200.f, 0.5f);
  }
}

GTEST_TEST(MazeWallScan, HorizonRowIsTheTopmostRowWithinTheDistance)
{
  // Row r sees the floor at 1000 mm + 10 mm * (height - r); rows above 40 see no floor.
  const auto floorDistance = [](int row, float& distance)
  {
    distance = 1000.f + 10.f * static_cast<float>(height - row);
    return row >= 40;
  };
  EXPECT_EQ(horizonRow(height, floorDistance, 1e9f), 40);
  EXPECT_EQ(horizonRow(height, floorDistance, 1500.f), height - 50);
  EXPECT_EQ(horizonRow(height, floorDistance, 500.f), -1);
}

namespace
{
  std::vector<ColumnBoundary> columns(int found, int total)
  {
    std::vector<ColumnBoundary> result(static_cast<std::size_t>(total));
    for(int i = 0; i < found; ++i)
      result[static_cast<std::size_t>(i)].row = 100;
    return result;
  }
}

GTEST_TEST(MazeWallScan, FloorModelColdStartHasNoFloor)
{
  FloorModel model;
  EXPECT_FALSE(model.known(0));
  EXPECT_FALSE(model.isFloor(100.f, 0));
  // Too few boundaries teach nothing.
  model.observe(columns(0, 10), 100.f, 10);
  model.observe(columns(4, 10), 100.f, 20);
  EXPECT_FALSE(model.known(20));
}

GTEST_TEST(MazeWallScan, FloorModelLearnsFromFramesWithEnoughBoundaries)
{
  FloorModel model;
  model.observe(columns(5, 10), 120.f, 10);
  EXPECT_TRUE(model.known(10));
  EXPECT_FLOAT_EQ(model.brightness(), 120.f);
  EXPECT_TRUE(model.isFloor(120.f, 10));
  EXPECT_TRUE(model.isFloor(120.f + model.params.tolerance, 10));
  EXPECT_FALSE(model.isFloor(120.f + model.params.tolerance + 1.f, 10));
  EXPECT_FALSE(model.isFloor(120.f - model.params.tolerance - 1.f, 10));
}

GTEST_TEST(MazeWallScan, FloorModelResetForgetsTheFloor)
{
  FloorModel model;
  model.observe(columns(10, 10), 120.f, 10);
  ASSERT_TRUE(model.known(10));
  model.reset();
  EXPECT_FALSE(model.known(10));
  EXPECT_FALSE(model.isFloor(120.f, 10));
  model.observe(columns(10, 10), 60.f, 20);
  EXPECT_FLOAT_EQ(model.brightness(), 60.f); // the new floor, not a mix with the old one
}

GTEST_TEST(MazeWallScan, FloorModelFollowsAChangeOfTheLighting)
{
  FloorModel model;
  model.observe(columns(10, 10), 100.f, 0);
  // The floor gets 15 levels brighter: not floor at first, followed after a few lessons.
  EXPECT_FALSE(model.isFloor(115.f, 100));
  unsigned time = 0;
  int lessons = 0;
  while(!model.isFloor(115.f, time) && lessons < 100)
  {
    time += 33;
    model.observe(columns(10, 10), 115.f, time);
    ++lessons;
  }
  EXPECT_LT(lessons, 100);
  EXPECT_GT(lessons, 1);
  for(int i = 0; i < 100; ++i)
    model.observe(columns(10, 10), 115.f, time += 33);
  EXPECT_NEAR(model.brightness(), 115.f, 0.5f);
}

GTEST_TEST(MazeWallScan, FloorModelIsForgottenWithoutLessons)
{
  FloorModel model;
  model.observe(columns(10, 10), 100.f, 1000);
  EXPECT_TRUE(model.isFloor(100.f, 1000 + model.params.maxAgeMs));
  EXPECT_FALSE(model.isFloor(100.f, 1001 + model.params.maxAgeMs));
  // A lesson after the forgetting starts from the new reference.
  model.observe(columns(10, 10), 140.f, 5000 + model.params.maxAgeMs);
  EXPECT_FLOAT_EQ(model.brightness(), 140.f);
}
