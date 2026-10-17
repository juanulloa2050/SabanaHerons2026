/**
 * @file MazeSessionTest.cpp
 *
 * Tests of the playing transitions (pose reset, attempt counter) and of the per-frame map actions.
 */

#include "Tools/Maze/MazeMapFusion.h"
#include "Tools/Maze/MazeSession.h"

#include <gtest/gtest.h>

namespace
{
  using namespace Maze;
}

GTEST_TEST(MazeSession, FirstFrameNotPlayingResetsOnceAndStartsNoAttempt)
{
  PlayingTracker tracker;
  PlayingTransition first = tracker.update(false);
  EXPECT_TRUE(first.firstFrame);
  EXPECT_TRUE(first.resetPose());
  EXPECT_FALSE(first.enteredPlaying);
  EXPECT_FALSE(first.newAttempt);
  const PlayingTransition second = tracker.update(false);
  EXPECT_FALSE(second.resetPose());
}

GTEST_TEST(MazeSession, FirstFrameAlreadyPlayingIsAttemptOne)
{
  PlayingTracker tracker;
  const PlayingTransition first = tracker.update(true);
  EXPECT_TRUE(first.resetPose());
  EXPECT_TRUE(first.enteredPlaying);
  EXPECT_FALSE(first.newAttempt); // The first attempt does not increment the counter.
  EXPECT_FALSE(tracker.update(true).resetPose());
}

GTEST_TEST(MazeSession, EveryTransitionIntoPlayingResetsAndLaterOnesStartAnAttempt)
{
  PlayingTracker tracker;
  (void)tracker.update(false);
  const PlayingTransition entered = tracker.update(true);
  EXPECT_TRUE(entered.resetPose());
  EXPECT_FALSE(entered.newAttempt);
  EXPECT_FALSE(tracker.update(true).resetPose());
  EXPECT_FALSE(tracker.update(false).resetPose()); // Pick-up, Standby: nothing happens yet ...
  EXPECT_FALSE(tracker.update(false).resetPose());
  const PlayingTransition again = tracker.update(true); // ... until playing is back.
  EXPECT_TRUE(again.resetPose());
  EXPECT_TRUE(again.newAttempt);
}

GTEST_TEST(MazeSession, AttemptCounterAndMapSurviveAPickUp)
{
  PlayingTracker tracker;
  MazeMapFusion fusion;
  const auto frame = [&](bool playing)
  {
    if(tracker.update(playing).newAttempt)
      fusion.newAttempt();
  };
  frame(true);
  for(int i = 0; i < 3; ++i)
    fusion.observe(2, 2, Heading::east, true, 1.f);
  fusion.markVisited(2, 2);
  const MazeMapFusion before = fusion;
  EXPECT_EQ(fusion.attempt, 1);
  frame(false);
  frame(true);
  EXPECT_EQ(fusion.attempt, 2);
  EXPECT_TRUE(fusion == before);
  frame(false);
  frame(true);
  EXPECT_EQ(fusion.attempt, 3);
}

GTEST_TEST(MazeSession, MapActionsApplyTheQualityGateAfterTheVisit)
{
  // Not active: nothing.
  EXPECT_FALSE(mapActions(false, true).markVisited);
  EXPECT_FALSE(mapActions(false, true).observe);
  // Active: the visit is recorded even when the quality is too poor to observe.
  const MapActions poor = mapActions(true, false);
  EXPECT_TRUE(poor.markVisited);
  EXPECT_FALSE(poor.observe);
  const MapActions good = mapActions(true, true);
  EXPECT_TRUE(good.markVisited);
  EXPECT_TRUE(good.observe);
}

GTEST_TEST(MazeSession, ActiveNeedsPlayingAndNotPickedUp)
{
  EXPECT_TRUE(isActive(true, false));
  EXPECT_FALSE(isActive(true, true)); // Picked up while the game still plays.
  EXPECT_FALSE(isActive(false, false)); // Standing on the ground outside of playing.
  EXPECT_FALSE(isActive(false, true));
}

GTEST_TEST(MazeSession, LiftingTheRobotStartsTheNextAttemptWhenItIsPutDown)
{
  PlayingTracker tracker;
  // The game keeps playing; the robot is on the ground, lifted, and put down again.
  EXPECT_TRUE(tracker.update(isActive(true, false)).resetPose());
  EXPECT_FALSE(tracker.update(isActive(true, false)).resetPose());
  EXPECT_FALSE(tracker.update(isActive(true, true)).resetPose());
  const PlayingTransition down = tracker.update(isActive(true, false));
  EXPECT_TRUE(down.resetPose());
  EXPECT_TRUE(down.newAttempt);
}

GTEST_TEST(MazeSession, QualityAcceptedComparesAgainstTheWorstAcceptedOne)
{
  EXPECT_TRUE(qualityAccepted(LocalizationQuality::superb, LocalizationQuality::okay));
  EXPECT_TRUE(qualityAccepted(LocalizationQuality::okay, LocalizationQuality::okay));
  EXPECT_FALSE(qualityAccepted(LocalizationQuality::poor, LocalizationQuality::okay));
  EXPECT_TRUE(qualityAccepted(LocalizationQuality::poor, LocalizationQuality::poor));
  EXPECT_FALSE(qualityAccepted(LocalizationQuality::okay, LocalizationQuality::superb));
}

GTEST_TEST(MazeSession, BootingPickedUpDelaysTheFirstAttemptWithoutCountingAnother)
{
  // FallDownState starts as pickedUp until the robot stands upright; the game may already be playing then.
  PlayingTracker tracker;
  const PlayingTransition boot = tracker.update(isActive(true, true));
  EXPECT_TRUE(boot.firstFrame);
  EXPECT_TRUE(boot.resetPose());
  EXPECT_FALSE(boot.enteredPlaying);
  EXPECT_FALSE(tracker.update(isActive(true, true)).resetPose());
  const PlayingTransition upright = tracker.update(isActive(true, false));
  EXPECT_TRUE(upright.enteredPlaying);
  EXPECT_FALSE(upright.newAttempt) << "the first attempt is attempt 1, not a second one";
}

GTEST_TEST(MazeSession, AFallIsNotAPickUp)
{
  // A fall is fallen/falling/staggering, never pickedUp, so the robot stays active and nothing resets.
  PlayingTracker tracker;
  EXPECT_TRUE(tracker.update(isActive(true, false)).resetPose());
  for(int i = 0; i < 10; ++i)
  {
    const PlayingTransition fall = tracker.update(isActive(true, false));
    EXPECT_FALSE(fall.resetPose());
    EXPECT_FALSE(fall.newAttempt);
  }
}
