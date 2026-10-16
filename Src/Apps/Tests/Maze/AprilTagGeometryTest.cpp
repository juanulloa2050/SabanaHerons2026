#include "Tools/Maze/AprilTagGeometry.h"
#include "Math/Angle.h"
#include "Math/Eigen.h"
#include "Math/Pose3f.h"

#include <gtest/gtest.h>
#include <cmath>

namespace
{
  using namespace Maze::AprilTagGeometry;

  constexpr float eps = 1e-3f;

  /** The camera frame is the robot frame. */
  Pose3f identityCamera()
  {
    return Pose3f();
  }

  void expectNear(const Vector3f& actual, const Vector3f& expected)
  {
    EXPECT_NEAR(actual.x(), expected.x(), eps);
    EXPECT_NEAR(actual.y(), expected.y(), eps);
    EXPECT_NEAR(actual.z(), expected.z(), eps);
  }

  /** The filter limits of Config/Scenarios/Maze/aprilTagDetector.cfg. */
  Filter defaultFilter()
  {
    return Filter();
  }

  // A tag of 150 mm seen by a camera with a focal length of 400 px: a frontal tag at 1000 mm spans 60 px.
  constexpr float tagSize = 150.f;
  constexpr float focalLength = 400.f;
  constexpr float frontalEdge = 60.f;
}

GTEST_TEST(AprilTagGeometry, FrontalTagFacesTheCamera)
{
  // apriltag: the tag frame has z into the wall, so the identity rotation is a tag that faces the camera.
  const Pose3f tag = toRobotFrame(Matrix3f::Identity(), Vector3f(0.f, 0.f, 1.f), identityCamera());
  expectNear(tag.translation, Vector3f(1000.f, 0.f, 0.f));
  // The normal points out of the wall, back towards the robot.
  expectNear(tag.rotation * Vector3f::UnitZ(), Vector3f(-1.f, 0.f, 0.f));
  // Looking at the tag from the robot, right is the robot's -y and up is the robot's z.
  expectNear(tag.rotation * Vector3f::UnitX(), Vector3f(0.f, -1.f, 0.f));
  expectNear(tag.rotation * Vector3f::UnitY(), Vector3f(0.f, 0.f, 1.f));
}

GTEST_TEST(AprilTagGeometry, OffCentreTagsMoveSidewaysAndVertically)
{
  // Optical x is right, which is the robot's -y.
  const Pose3f right = toRobotFrame(Matrix3f::Identity(), Vector3f(0.2f, 0.f, 1.f), identityCamera());
  expectNear(right.translation, Vector3f(1000.f, -200.f, 0.f));
  // Optical y is down, which is the robot's -z.
  const Pose3f below = toRobotFrame(Matrix3f::Identity(), Vector3f(0.f, 0.1f, 1.f), identityCamera());
  expectNear(below.translation, Vector3f(1000.f, 0.f, -100.f));
}

GTEST_TEST(AprilTagGeometry, CameraMatrixIsComposed)
{
  const float pitch = Angle::fromDegrees(30.f);
  const Pose3f camera(RotationMatrix(AngleAxisf(pitch, Vector3f::UnitY())), Vector3f(0.f, 0.f, 500.f));
  const Pose3f tag = toRobotFrame(Matrix3f::Identity(), Vector3f(0.f, 0.f, 1.f), camera);
  // A positive rotation around y lowers the x-axis: the tag 1 m in front of the camera is below the camera.
  expectNear(tag.translation, Vector3f(1000.f * std::cos(pitch), 0.f, 500.f - 1000.f * std::sin(pitch)));
  expectNear(tag.rotation * Vector3f::UnitZ(), Vector3f(-std::cos(pitch), 0.f, std::sin(pitch)));
  expectNear(tag.rotation * Vector3f::UnitX(), Vector3f(0.f, -1.f, 0.f));
}

GTEST_TEST(AprilTagGeometry, RotationStaysOrthonormal)
{
  // A tag slanted around the optical y-axis and the optical x-axis, seen by a pitched camera.
  const Matrix3f slanted = (AngleAxisf(Angle::fromDegrees(35.f), Vector3f::UnitY()) *
                            AngleAxisf(Angle::fromDegrees(-20.f), Vector3f::UnitX())).toRotationMatrix();
  const Pose3f camera(RotationMatrix(AngleAxisf(Angle::fromDegrees(20.f), Vector3f::UnitY())), Vector3f(0.f, 0.f, 500.f));
  const Pose3f tag = toRobotFrame(slanted, Vector3f(-0.1f, 0.05f, 0.8f), camera);
  const Matrix3f& r = tag.rotation;
  EXPECT_NEAR(r.determinant(), 1.f, eps);
  EXPECT_NEAR((r.transpose() * r - Matrix3f::Identity()).norm(), 0.f, eps);
}

GTEST_TEST(AprilTagGeometry, DecodingFilterRejectsEachReason)
{
  const Filter filter = defaultFilter();
  EXPECT_TRUE(acceptsDecoding(12, 0, 80.f, filter));
  EXPECT_TRUE(acceptsDecoding(0, 0, filter.minDecisionMargin, filter));   // the limits are inclusive
  EXPECT_TRUE(acceptsDecoding(filter.maxId, 0, 80.f, filter));
  EXPECT_FALSE(acceptsDecoding(filter.maxId + 1, 0, 80.f, filter));       // id of no maze cell
  EXPECT_FALSE(acceptsDecoding(-1, 0, 80.f, filter));
  EXPECT_FALSE(acceptsDecoding(12, filter.maxHamming + 1, 80.f, filter)); // too many corrected bits
  EXPECT_FALSE(acceptsDecoding(12, 0, filter.minDecisionMargin - 1.f, filter));
}

GTEST_TEST(AprilTagGeometry, DistanceFilterRejectsEachReason)
{
  const Filter filter = defaultFilter();
  // Frontal tag at 1000 mm: the pose and the apparent size agree.
  EXPECT_TRUE(acceptsDistance(1000.f, frontalEdge, focalLength, tagSize, filter));
  // Too far away, even though the apparent size agrees (tag at 3500 mm spans about 17 px).
  EXPECT_FALSE(acceptsDistance(3500.f, 400.f * 150.f / 3500.f, focalLength, tagSize, filter));
  // The pose says 1000 mm but the tag spans only 20 px (expected 3000 mm): a wrong pose.
  EXPECT_FALSE(acceptsDistance(1000.f, 20.f, focalLength, tagSize, filter));
  // The pose says 1000 mm but the tag spans 200 px (expected 300 mm).
  EXPECT_FALSE(acceptsDistance(1000.f, 200.f, focalLength, tagSize, filter));
}

GTEST_TEST(AprilTagGeometry, SlantedTagIsAcceptedWithinTolerance)
{
  const Filter filter = defaultFilter();
  // A tag slanted by 60 degrees has an apparent size of cos(60) of the frontal one at best, but its longest
  // edge is the tilted one, which still spans a large part of the frontal size.
  EXPECT_TRUE(acceptsDistance(1000.f, frontalEdge * 0.75f, focalLength, tagSize, filter));
  // The tolerance is relative: expected 1500 mm against a pose of 1000 mm differs by exactly one third.
  EXPECT_TRUE(acceptsDistance(1000.f, frontalEdge * (2.f / 3.f), focalLength, tagSize, filter));
  // A degenerate edge does not divide by zero.
  EXPECT_FALSE(acceptsDistance(1000.f, 0.f, focalLength, tagSize, filter));
}
