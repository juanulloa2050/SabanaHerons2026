/**
 * @file MazeLocalizationTest.cpp
 *
 * Tests of the heading math and of the maze pose filter: reset, prediction, tag fixes and quality.
 */

#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeLocalization.h"
#include "Tools/Maze/MazeOracle.h"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

namespace
{
  using namespace Maze;

  constexpr float tolerance = 1e-4f;

  float deg(float degrees)
  {
    return Angle::fromDegrees(degrees);
  }

  /** A maze without inner walls, so that every tag is visible. */
  MazeGraph openMaze()
  {
    MazeGraph graph;
    for(int row = 0; row < size; ++row)
      for(int col = 0; col < size; ++col)
        for(int h = 0; h < headingCount; ++h)
          graph.set(row, col, static_cast<Heading>(h), EdgeState::open);
    return graph;
  }

  /**
   * The tag on `face` of `cell` as the oracle would show it to a robot standing at `truth`.
   * @return Whether the oracle sees it.
   */
  bool tagSeenFrom(const Pose2f& truth, int cell, Heading face, Pose3f& tagInRobot)
  {
    const Oracle::Camera camera{Vector2f::Zero(), 0.f, 1.2f};
    Oracle::View view;
    if(!Oracle::computeView(openMaze(), truth, camera, 2, view))
      return false;
    return Oracle::observeTag(view, camera, 2, deg(80.f), cell, face, 300.f, tagInRobot);
  }

  Pose2f truthIn(int row, int col, float dx, float dy, float rotation)
  {
    return Pose2f(rotation, Oracle::cellCentreOf(row, col) + Vector2f(dx, dy));
  }
}

GTEST_TEST(MazeHeading, Snap90RoundsToTheNearestQuarter)
{
  EXPECT_NEAR(snap90(deg(44.f)), 0.f, tolerance);
  EXPECT_NEAR(snap90(deg(-44.f)), 0.f, tolerance);
  EXPECT_NEAR(snap90(deg(46.f)), deg(90.f), tolerance);
  EXPECT_NEAR(snap90(deg(-46.f)), deg(-90.f), tolerance);
  EXPECT_NEAR(snap90(deg(134.f)), deg(90.f), tolerance);
  EXPECT_NEAR(std::abs(snap90(deg(136.f))), deg(180.f), tolerance);
  EXPECT_NEAR(std::abs(snap90(deg(179.f))), deg(180.f), tolerance);
  EXPECT_NEAR(std::abs(snap90(deg(-179.f))), deg(180.f), tolerance);
  EXPECT_NEAR(snap90(deg(-136.f)), deg(180.f) * (snap90(deg(-136.f)) < 0.f ? -1.f : 1.f), tolerance);
}

GTEST_TEST(MazeHeading, HeadingAndAngleRoundTrip)
{
  EXPECT_NEAR(angleOf(Heading::east), 0.f, tolerance);
  EXPECT_NEAR(angleOf(Heading::north), deg(90.f), tolerance);
  EXPECT_NEAR(angleOf(Heading::south), deg(-90.f), tolerance);
  EXPECT_NEAR(angleOf(Heading::west), deg(180.f), tolerance);
  for(int h = 0; h < headingCount; ++h)
  {
    const Heading heading = static_cast<Heading>(h);
    EXPECT_EQ(headingOf(angleOf(heading)), heading);
    EXPECT_EQ(headingOf(angleOf(heading) + deg(40.f)), heading);
    EXPECT_EQ(headingOf(angleOf(heading) - deg(40.f)), heading);
    EXPECT_EQ(headingOf(angleOf(heading) + deg(360.f)), heading);
    EXPECT_EQ(headingOf(angleOf(heading)), Oracle::snapToHeading(angleOf(heading)));
    EXPECT_NEAR(angleOf(heading), Oracle::angleOf(heading), tolerance);
  }
}

GTEST_TEST(MazeLocalization, ResetPutsTheRobotAtTheCellCentre)
{
  MazeLocalization localization;
  localization.reset(0, Heading::east, 0.7f);
  EXPECT_NEAR(localization.pose.translation.x(), 250.f, tolerance);
  EXPECT_NEAR(localization.pose.translation.y(), 3750.f, tolerance);
  EXPECT_NEAR(localization.pose.rotation, 0.f, tolerance);
  int row = -1, col = -1;
  EXPECT_TRUE(localization.cell(row, col));
  EXPECT_EQ(row, 0);
  EXPECT_EQ(col, 0);

  localization.reset(63, Heading::north, 0.f);
  EXPECT_NEAR(localization.pose.translation.x(), 3750.f, tolerance);
  EXPECT_NEAR(localization.pose.translation.y(), 250.f, tolerance);
  EXPECT_NEAR(localization.pose.rotation, deg(90.f), tolerance);
  EXPECT_TRUE(localization.cell(row, col));
  EXPECT_EQ(row, 7);
  EXPECT_EQ(col, 7);
  EXPECT_EQ(localization.heading(), Heading::north);
}

GTEST_TEST(MazeLocalization, PredictFollowsTheGyroOrTheOdometry)
{
  MazeLocalization localization;
  localization.reset(cellId(4, 4), Heading::east, 1.f);
  // Walk 100 mm forward while the gyro turns 30 degrees and the odometry says 10 degrees.
  localization.predict(Pose2f(deg(10.f), Vector2f(100.f, 0.f)), 1.f + deg(30.f), true);
  EXPECT_NEAR(localization.pose.rotation, deg(30.f), tolerance);
  EXPECT_EQ(localization.headingSource, HeadingSource::gyro);
  EXPECT_NEAR(localization.pose.translation.x(), 2350.f, tolerance);

  localization.reset(cellId(4, 4), Heading::east, 1.f);
  localization.predict(Pose2f(deg(10.f), Vector2f(100.f, 0.f)), 1.f + deg(30.f), false);
  EXPECT_NEAR(localization.pose.rotation, deg(10.f), tolerance);
  EXPECT_EQ(localization.headingSource, HeadingSource::odometry);
  EXPECT_NEAR(localization.distanceSinceFix, 100.f, tolerance);

  // The next step moves along the rotated heading (north is +90 degrees).
  localization.reset(cellId(4, 4), Heading::north, 0.f);
  const Vector2f before = localization.pose.translation;
  localization.predict(Pose2f(0.f, Vector2f(200.f, 0.f)), 0.f, true);
  EXPECT_NEAR(localization.pose.translation.x(), before.x(), tolerance);
  EXPECT_NEAR(localization.pose.translation.y(), before.y() + 200.f, tolerance);
}

GTEST_TEST(MazeLocalization, GyroDriftShowsUntilASquareOnTagRealigns)
{
  MazeLocalization localization;
  const int cell = cellId(3, 2);
  localization.reset(cell, Heading::east, 0.f);
  // The robot does not turn, but the gyro has drifted by 8 degrees.
  const float drift = deg(8.f);
  localization.predict(Pose2f(0.f, Vector2f::Zero()), drift, true);
  EXPECT_NEAR(localization.pose.rotation, drift, tolerance);

  const Pose2f truth = truthIn(3, 2, 0.f, 0.f, 0.f);
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truth, cell, Heading::east, tag));
  for(int i = 0; i < 6; ++i)
  {
    const TagFix fix = localization.fixWithTag(cell, tag, 1000u + 100u * static_cast<unsigned>(i), MazeTagLayout());
    ASSERT_TRUE(fix.accepted);
    EXPECT_TRUE(fix.headingCorrected);
    localization.predict(Pose2f(0.f, Vector2f::Zero()), drift, true); // The gyro keeps reporting the drifted yaw.
  }
  EXPECT_NEAR(localization.pose.rotation, 0.f, deg(1.f));
  EXPECT_EQ(localization.headingSource, HeadingSource::gyro); // predict ran after the last fix
  EXPECT_NEAR(localization.headingDrift(), drift, deg(1.f));
}

GTEST_TEST(MazeLocalization, HeadingDriftIsTheNetChangeOfTheGyroOffsetSinceReset)
{
  MazeLocalization localization;
  const int cell = cellId(3, 2);
  const float reference = deg(170.f); // The gyro reference is arbitrary: a reset near +-pi must not show as drift.
  localization.reset(cell, Heading::east, reference);
  EXPECT_EQ(localization.headingDrift(), 0.f);
  localization.predict(Pose2f(0.f, Vector2f::Zero()), reference, true);
  EXPECT_NEAR(localization.headingDrift(), 0.f, 1e-4f);

  // The gyro yaw wraps over +-pi while it drifts by 15 degrees; the offset does the same, the net drift stays 15 degrees.
  localization.gyroOffset = Angle::normalize(localization.gyroOffset + deg(15.f));
  EXPECT_NEAR(localization.headingDrift(), deg(15.f), 1e-4f);
  localization.gyroOffset = Angle::normalize(localization.gyroOffset - deg(40.f));
  EXPECT_NEAR(localization.headingDrift(), deg(-25.f), 1e-4f);
}

GTEST_TEST(MazeLocalization, TagFixesConvergeToTheTruth)
{
  MazeLocalization localization;
  const int cell = cellId(3, 2);
  const Pose2f truth = truthIn(3, 2, -120.f, 60.f, deg(8.f));
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truth, cell, Heading::east, tag));

  localization.reset(cell, Heading::east, 0.f);
  localization.pose.translation += Vector2f(90.f, -70.f); // A perturbed estimate.
  localization.pose.rotation = deg(-5.f);
  const float errorBefore = (localization.pose.translation - truth.translation).norm();
  for(int i = 0; i < 8; ++i)
  {
    const TagFix fix = localization.fixWithTag(cell, tag, 100u * static_cast<unsigned>(i), MazeTagLayout());
    ASSERT_TRUE(fix.accepted);
    EXPECT_EQ(fix.face, Heading::east);
  }
  EXPECT_LT((localization.pose.translation - truth.translation).norm(), errorBefore / 10.f);
  EXPECT_NEAR(localization.pose.rotation, truth.rotation, deg(1.f));
  EXPECT_EQ(localization.headingSource, HeadingSource::tag);
}

GTEST_TEST(MazeLocalization, TagOnAnotherFaceIsFoundThroughTheNormal)
{
  MazeLocalization localization;
  const int cell = cellId(5, 5);
  const Pose2f truth = truthIn(5, 5, 20.f, 0.f, deg(90.f));
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truth, cell, Heading::north, tag));
  localization.reset(cell, Heading::north, 0.f);
  const TagFix fix = localization.fixWithTag(cell, tag, 10u, MazeTagLayout());
  ASSERT_TRUE(fix.accepted);
  EXPECT_EQ(fix.face, Heading::north);
}

GTEST_TEST(MazeLocalization, WrongFaceAndWrongIdAreRejected)
{
  MazeLocalization localization;
  const int cell = cellId(3, 2);
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truthIn(3, 2, 0.f, 0.f, 0.f), cell, Heading::east, tag));

  // The localizer believes it looks south: the measured normal then points east, which no face of the layout explains.
  MazeTagLayout northOnly;
  northOnly.mode = TagLayoutMode::faces;
  northOnly.faces = {TagFace::north};
  localization.reset(cell, Heading::east, 0.f);
  const Pose2f poseBefore = localization.pose;
  const TagFix wrongFace = localization.fixWithTag(cell, tag, 10u, northOnly);
  EXPECT_FALSE(wrongFace.accepted);
  EXPECT_GT(wrongFace.faceAngleError, deg(45.f));
  EXPECT_EQ(localization.pose.translation, poseBefore.translation);
  EXPECT_EQ(localization.lastFixTime, 0u);

  EXPECT_FALSE(localization.fixWithTag(-1, tag, 10u, MazeTagLayout()).accepted);
  EXPECT_FALSE(localization.fixWithTag(cellCount, tag, 10u, MazeTagLayout()).accepted);
  EXPECT_TRUE(localization.fixWithTag(cell, tag, 10u, MazeTagLayout()).accepted);
}

GTEST_TEST(MazeLocalization, ObliqueTagCorrectsOnlyThePosition)
{
  MazeLocalization localization;
  const int cell = cellId(3, 2);
  const Pose2f truth = truthIn(3, 2, -200.f, 150.f, deg(0.f));
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truth, cell, Heading::east, tag));
  localization.reset(cell, Heading::east, 0.f);
  localization.parameters.squareOnAngle = deg(5.f);
  const TagFix fix = localization.fixWithTag(cell, tag, 10u, MazeTagLayout());
  ASSERT_TRUE(fix.accepted);
  EXPECT_FALSE(fix.headingCorrected);
  EXPECT_EQ(localization.headingDrift(), 0.f);
  EXPECT_NE(localization.headingSource, HeadingSource::tag);
}

GTEST_TEST(MazeLocalization, QualityDecaysWithoutFixes)
{
  MazeLocalization localization;
  localization.reset(cellId(3, 2), Heading::east, 0.f, 1000u);
  EXPECT_EQ(localization.quality(1000u), LocalizationQuality::superb);
  EXPECT_EQ(localization.quality(5000u), LocalizationQuality::superb);
  EXPECT_EQ(localization.quality(20000u), LocalizationQuality::okay);
  EXPECT_EQ(localization.quality(60000u), LocalizationQuality::poor);

  // Distance alone also degrades it.
  localization.reset(cellId(3, 2), Heading::east, 0.f, 1000u);
  for(int i = 0; i < 4; ++i)
    localization.predict(Pose2f(0.f, Vector2f(500.f, 0.f)), 0.f, true);
  EXPECT_EQ(localization.quality(1100u), LocalizationQuality::okay);
  for(int i = 0; i < 8; ++i)
    localization.predict(Pose2f(0.f, Vector2f(-500.f, 0.f)), 0.f, true);
  EXPECT_EQ(localization.quality(1100u), LocalizationQuality::poor);

  // A fix brings it back.
  Pose3f tag;
  const int cell = cellId(3, 2);
  localization.pose = Pose2f(0.f, Oracle::cellCentreOf(3, 2));
  ASSERT_TRUE(tagSeenFrom(localization.pose, cell, Heading::east, tag));
  ASSERT_TRUE(localization.fixWithTag(cell, tag, 70000u, MazeTagLayout()).accepted);
  EXPECT_EQ(localization.quality(70000u), LocalizationQuality::superb);
  EXPECT_EQ(localization.quality(100000u), LocalizationQuality::okay);
}

GTEST_TEST(MazeLocalization, PoseOutsideTheMazeIsFlaggedAndClamped)
{
  MazeLocalization localization;
  localization.reset(0, Heading::east, 0.f);
  localization.pose.translation = Vector2f(-300.f, 5000.f);
  int row = -1, col = -1;
  EXPECT_FALSE(localization.cell(row, col));
  EXPECT_EQ(row, 0);
  EXPECT_EQ(col, 0);
}

GTEST_TEST(MazeLocalization, NonFiniteTagPoseIsRejectedAndKeepsThePoseFinite)
{
  MazeLocalization localization;
  const int cell = cellId(3, 2);
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truthIn(3, 2, 0.f, 0.f, 0.f), cell, Heading::east, tag));
  localization.reset(cell, Heading::east, 0.f);
  const Pose2f poseBefore = localization.pose;

  Pose3f nanTranslation = tag;
  nanTranslation.translation.x() = std::numeric_limits<float>::quiet_NaN();
  Pose3f infTranslation = tag;
  infTranslation.translation.y() = std::numeric_limits<float>::infinity();
  Pose3f nanRotation = tag;
  nanRotation.rotation(0, 2) = std::numeric_limits<float>::quiet_NaN();
  for(const Pose3f& bad : {nanTranslation, infTranslation, nanRotation})
    EXPECT_FALSE(localization.fixWithTag(cell, bad, 10u, MazeTagLayout()).accepted);

  EXPECT_EQ(localization.pose.translation, poseBefore.translation);
  EXPECT_EQ(localization.pose.rotation, poseBefore.rotation);
  EXPECT_EQ(localization.lastFixTime, 0u);
  EXPECT_TRUE(localization.fixWithTag(cell, tag, 10u, MazeTagLayout()).accepted);
  EXPECT_TRUE(localization.pose.translation.allFinite());
}

GTEST_TEST(MazeLocalization, QualityIgnoresAClockThatWentBackwards)
{
  MazeLocalization localization;
  localization.reset(cellId(3, 2), Heading::east, 0.f, 50000u);
  // now < lastFixTime must not wrap around to a huge elapsed time.
  EXPECT_EQ(localization.quality(1000u), LocalizationQuality::superb);
  EXPECT_EQ(localization.quality(0u), LocalizationQuality::superb);
}

GTEST_TEST(MazeLocalization, ChooseFacePicksTheCandidateClosestToTheNormal)
{
  const int cell = cellId(3, 2);
  Pose3f tag;
  ASSERT_TRUE(tagSeenFrom(truthIn(3, 2, 0.f, 0.f, 0.f), cell, Heading::east, tag));

  FaceChoice choice = chooseFace(cell, 0.f, tag, MazeTagLayout());
  ASSERT_TRUE(choice.found);
  EXPECT_EQ(choice.face, Heading::east);
  EXPECT_NEAR(choice.angleError, 0.f, deg(1.f));

  // A robot that thinks it points elsewhere gets an error, not a different face when the layout has no other.
  MazeTagLayout northOnly;
  northOnly.mode = TagLayoutMode::faces;
  northOnly.faces = {TagFace::north};
  choice = chooseFace(cell, 0.f, tag, northOnly);
  ASSERT_TRUE(choice.found);
  EXPECT_EQ(choice.face, Heading::north);
  EXPECT_GT(choice.angleError, deg(45.f));

  EXPECT_FALSE(chooseFace(-1, 0.f, tag, MazeTagLayout()).found);
  Pose3f degenerate = tag;
  degenerate.rotation = RotationMatrix(Matrix3f::Zero());
  EXPECT_FALSE(chooseFace(cell, 0.f, degenerate, MazeTagLayout()).found);
}

GTEST_TEST(MazeHeading, GazeTurnsCounterclockwiseWhileHeadingTurnsClockwise)
{
  // Facing north, one quarter turn counterclockwise (left) looks west; three look east.
  EXPECT_EQ(gazeHeading(Heading::north, 0), Heading::north);
  EXPECT_EQ(gazeHeading(Heading::north, 1), Heading::west);
  EXPECT_EQ(gazeHeading(Heading::north, 2), Heading::south);
  EXPECT_EQ(gazeHeading(Heading::north, 3), Heading::east);
  EXPECT_EQ(gazeHeading(Heading::east, 1), Heading::north);
  EXPECT_EQ(gazeHeading(Heading::south, -1), Heading::west);
  EXPECT_EQ(gazeHeading(Heading::west, 5), Heading::south);
  // Consistent with the angles: a counterclockwise quarter turn adds pi/2 to the maze angle.
  for(int h = 0; h < headingCount; ++h)
    for(int turns = -5; turns <= 5; ++turns)
    {
      const Heading heading = static_cast<Heading>(h);
      EXPECT_EQ(headingOf(angleOf(heading) + static_cast<float>(turns) * deg(90.f)), gazeHeading(heading, turns));
    }
}
