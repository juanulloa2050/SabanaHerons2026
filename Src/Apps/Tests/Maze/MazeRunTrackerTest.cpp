/**
 * @file MazeRunTrackerTest.cpp
 *
 * Tests of the measurement of a simulated maze run: tap timing, goal and timeout, wall contacts, stall and the two-attempt sequence.
 */

#include "Tools/Maze/MazeRunTracker.h"

#include <gtest/gtest.h>
#include <vector>

namespace
{
  using namespace Maze;

  /** A maze without inner walls (only the border is a wall) that starts in cell 0, the north-west corner. */
  MazeDescription openMaze()
  {
    MazeDescription maze;
    for(int row = 0; row < size; ++row)
      for(int col = 0; col < size; ++col)
        for(int h = 0; h < headingCount; ++h)
          maze.graph.set(row, col, static_cast<Heading>(h), EdgeState::open);
    maze.startCell = 0;
    return maze;
  }

  const Vector2f startCentre(250.f, 3750.f); // centre of cell 0
  const Vector2f goalCentre(2250.f, 1750.f); // centre of cell 36 (row 4, column 4)
  const Vector2f middleCentre(1250.f, 2750.f); // centre of cell 18, no goal, 750 mm from the border and no inner walls

  RunConfig config(int attempts)
  {
    RunConfig result;
    result.attempts = attempts;
    result.startTapAfterMs = 1000;
    result.attemptTimeoutMs = 20000;
    result.returnTimeoutMs = 30000;
    result.settleMs = 2000;
    return result;
  }

  /** Feeds frames and collects what the tracker asks for. */
  struct Harness
  {
    explicit Harness(const RunConfig& config) : tracker(openMaze(), config) {}

    RunUpdate feed(unsigned timeMs, const Vector2f& position, float rotation = 0.f, bool playing = true)
    {
      RunFrame frame;
      frame.timeMs = timeMs;
      frame.playing = playing;
      frame.believedAtStart = believedAtStart;
      frame.speedRunFound = speedRunFound;
      frame.pose = Pose2f(rotation, position);
      const RunUpdate update = tracker.update(frame);
      taps += update.tap ? 1 : 0;
      results.insert(results.end(), update.finished.begin(), update.finished.end());
      return update;
    }

    /** Feeds `position` every 100 ms from `from` (included) to `to` (included). */
    void hold(unsigned from, unsigned to, const Vector2f& position)
    {
      for(unsigned t = from; t <= to; t += 100)
        feed(t, position);
    }

    RunTracker tracker;
    bool believedAtStart = true; /**< Fed with every frame; the tests that need it false set it. */
    bool speedRunFound = false;
    int taps = 0;
    std::vector<AttemptResult> results;
  };
}

GTEST_TEST(MazeRunTracker, NothingHappensWithoutTapDelay)
{
  RunConfig disabled = config(1);
  disabled.startTapAfterMs = 0;
  Harness run(disabled);
  run.hold(0, 60000, goalCentre);
  EXPECT_EQ(run.taps, 0);
  EXPECT_TRUE(run.results.empty());
  EXPECT_EQ(run.tracker.phase(), RunPhase::waitingForStart);
}

GTEST_TEST(MazeRunTracker, FirstTapComesAfterThePlayingDelay)
{
  Harness run(config(1));
  run.feed(0, startCentre, 0.f, false);
  run.feed(5000, startCentre, 0.f, false); // not playing: no tap however late
  EXPECT_EQ(run.taps, 0);
  run.feed(6000, startCentre); // playing since now
  EXPECT_EQ(run.taps, 0);
  run.feed(6900, startCentre);
  EXPECT_EQ(run.taps, 0);
  EXPECT_TRUE(run.feed(7000, startCentre).tap);
  EXPECT_EQ(run.tracker.phase(), RunPhase::running);
  run.feed(7100, startCentre);
  EXPECT_EQ(run.taps, 1); // once
}

GTEST_TEST(MazeRunTracker, GoalEntryEndsTheAttemptAndGivesTheTime)
{
  Harness run(config(1));
  run.feed(0, startCentre);
  run.feed(1000, startCentre); // tap
  run.hold(1100, 5000, middleCentre);
  EXPECT_TRUE(run.results.empty());
  run.feed(5100, goalCentre);
  ASSERT_EQ(run.results.size(), 1u);
  EXPECT_EQ(run.results[0].attempt, 1);
  EXPECT_TRUE(run.results[0].reachedGoal);
  EXPECT_EQ(run.results[0].status, RunStatus::goal);
  EXPECT_EQ(run.results[0].timeMs, 4100u);
  EXPECT_EQ(run.tracker.phase(), RunPhase::done);
}

GTEST_TEST(MazeRunTracker, OnlyTheCentreCountsForTheGoal)
{
  Harness run(config(1));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  run.feed(1100, Vector2f(1490.f, 2000.f)); // ten millimetres west of the goal block (x 1500..2500, y 1500..2500)
  EXPECT_TRUE(run.results.empty());
  run.feed(1200, Vector2f(1510.f, 2010.f)); // goal cell 35
  EXPECT_EQ(run.results.size(), 1u);
}

GTEST_TEST(MazeRunTracker, TimeoutEndsTheAttemptWithoutTheGoal)
{
  Harness run(config(1));
  run.feed(0, startCentre);
  run.feed(1000, startCentre); // tap
  run.hold(1100, 20900, middleCentre);
  EXPECT_TRUE(run.results.empty());
  run.feed(21000, middleCentre);
  ASSERT_EQ(run.results.size(), 1u);
  EXPECT_FALSE(run.results[0].reachedGoal);
  EXPECT_EQ(run.results[0].status, RunStatus::timeout);
  EXPECT_EQ(run.results[0].timeMs, 20000u);
  EXPECT_EQ(run.tracker.phase(), RunPhase::done);
}

GTEST_TEST(MazeRunTracker, WallContactCountsEntriesWithHysteresis)
{
  Harness run(config(1));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  const float y = 2750.f;
  run.feed(1100, Vector2f(400.f, y)); // free
  run.feed(1200, Vector2f(140.f, y)); // enters contact (west border at x = 0, radius 150)
  run.feed(1300, Vector2f(160.f, y)); // above the radius, below radius + hysteresis: still in contact
  run.feed(1400, Vector2f(145.f, y)); // flicker: no new entry
  run.feed(1500, Vector2f(165.f, y));
  run.feed(1600, Vector2f(140.f, y));
  EXPECT_EQ(run.tracker.currentResult().wallContacts, 1);
  run.feed(1700, Vector2f(175.f, y)); // above radius + hysteresis: free
  run.feed(1800, Vector2f(140.f, y)); // second entry
  EXPECT_EQ(run.tracker.currentResult().wallContacts, 2);
}

GTEST_TEST(MazeRunTracker, ContactAtTheTapIsNotAnEntry)
{
  Harness run(config(1));
  run.feed(0, Vector2f(100.f, 2750.f));
  run.feed(1000, Vector2f(100.f, 2750.f)); // tap while touching the west border
  run.feed(1100, Vector2f(100.f, 2750.f));
  EXPECT_EQ(run.tracker.currentResult().wallContacts, 0);
}

GTEST_TEST(MazeRunTracker, InnerWallsAreContactsAndTheirEndsToo)
{
  MazeDescription maze = openMaze();
  maze.graph.set(2, 2, Heading::east, EdgeState::wall); // x = 1500, y 2500..3000
  RunTracker tracker(maze, config(1));
  const auto feed = [&](unsigned t, const Vector2f& position)
  {
    RunFrame frame;
    frame.timeMs = t;
    frame.playing = true;
    frame.pose = Pose2f(0.f, position);
    return tracker.update(frame);
  };
  feed(0, startCentre);
  feed(1000, startCentre);
  feed(1100, Vector2f(1250.f, 2750.f)); // 250 mm from the wall
  EXPECT_EQ(tracker.currentResult().wallContacts, 0);
  feed(1200, Vector2f(1400.f, 2750.f)); // 100 mm from the wall
  EXPECT_EQ(tracker.currentResult().wallContacts, 1);
  feed(1300, Vector2f(1400.f, 3250.f)); // 250 mm beyond its end: free
  feed(1400, Vector2f(1400.f, 3100.f)); // 141 mm from the end point of the wall: contact again
  EXPECT_EQ(tracker.currentResult().wallContacts, 2);
  feed(1500, Vector2f(1250.f, 3250.f));
  feed(1600, Vector2f(1600.f, 2250.f)); // 269 mm from the other end of the wall: free
  EXPECT_EQ(tracker.currentResult().wallContacts, 2);
}

GTEST_TEST(MazeRunTracker, MaxStallIsTheLongestIntervalWithoutProgress)
{
  Harness run(config(1));
  run.feed(0, startCentre);
  run.feed(1000, startCentre); // tap
  run.hold(1100, 4000, Vector2f(1000.f, 2750.f)); // moved from the start at 1100: stall restarts there
  run.hold(4100, 5000, Vector2f(1060.f, 2750.f)); // 60 mm: progress, the anchor moves to 4100
  run.hold(5100, 6000, Vector2f(1080.f, 2750.f)); // 20 mm from the anchor: still stalled
  run.feed(6100, Vector2f(1300.f, 2750.f));
  // The longest interval: anchor 4100 up to 6000 is 1900 ms, the one from 1100 to 4000 is 2900 ms.
  EXPECT_EQ(run.tracker.currentResult().maxStallMs, 2900u);
}

GTEST_TEST(MazeRunTracker, TurningOnTheSpotIsProgressUntilItIsSmall)
{
  Harness run(config(1));
  run.feed(0, middleCentre);
  run.feed(1000, middleCentre);
  run.feed(2000, middleCentre, 0.1f); // 5.7 degrees: still stalled since the tap
  EXPECT_EQ(run.tracker.currentResult().maxStallMs, 1000u);
  run.feed(3000, middleCentre, 0.5f); // 28 degrees: progress, the anchor moves
  run.feed(3500, middleCentre, 0.5f);
  EXPECT_EQ(run.tracker.currentResult().maxStallMs, 1000u);
}

GTEST_TEST(MazeRunTracker, SecondAttemptStartsAfterTheReturnAndTheSettleTime)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre); // tap 1
  run.feed(3000, goalCentre);
  ASSERT_EQ(run.results.size(), 1u);
  EXPECT_TRUE(run.results[0].reachedGoal);
  EXPECT_EQ(run.tracker.phase(), RunPhase::returning);
  run.hold(3100, 10000, goalCentre); // standing at the goal: no tap
  EXPECT_EQ(run.taps, 1);
  run.hold(10100, 12000, Vector2f(250.f, 3000.f)); // walking in cell 8, outside the start cell
  run.feed(12100, Vector2f(250.f, 3100.f));
  run.feed(12200, Vector2f(300.f, 3300.f));
  EXPECT_EQ(run.taps, 1);
  run.feed(13000, startCentre); // the first frame in cell 0 (y 3500..4000)
  run.hold(13100, 14900, startCentre);
  EXPECT_EQ(run.taps, 1); // 1900 ms still
  run.feed(15000, startCentre);
  EXPECT_EQ(run.taps, 2);
  EXPECT_EQ(run.tracker.phase(), RunPhase::running);
  EXPECT_EQ(run.tracker.attempt(), 2);
  EXPECT_EQ(run.tracker.currentResult().wallContacts, 0); // the counters start from zero again
  run.feed(16000, goalCentre);
  ASSERT_EQ(run.results.size(), 2u);
  EXPECT_EQ(run.results[1].attempt, 2);
  EXPECT_TRUE(run.results[1].reachedGoal);
  EXPECT_EQ(run.results[1].timeMs, 1000u);
  EXPECT_EQ(run.tracker.phase(), RunPhase::done);
}

GTEST_TEST(MazeRunTracker, AMovingRobotInTheStartCellDoesNotSettle)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  run.feed(2000, goalCentre);
  for(unsigned t = 2100; t <= 12000; t += 100)
    run.feed(t, Vector2f(250.f, t % 200 == 0 ? 3600.f : 3800.f)); // 200 mm back and forth in cell 0
  EXPECT_EQ(run.taps, 1);
}

GTEST_TEST(MazeRunTracker, ReturnTimeoutReportsTheSecondAttemptAsNotStarted)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  run.feed(2000, goalCentre); // return starts at 2000
  run.hold(2100, 31900, goalCentre);
  EXPECT_EQ(run.results.size(), 1u);
  run.feed(32000, goalCentre);
  ASSERT_EQ(run.results.size(), 2u);
  EXPECT_EQ(run.results[1].attempt, 2);
  EXPECT_EQ(run.results[1].status, RunStatus::notStarted);
  EXPECT_FALSE(run.results[1].reachedGoal);
  EXPECT_EQ(run.taps, 1);
  EXPECT_EQ(run.tracker.phase(), RunPhase::done);
}

GTEST_TEST(MazeRunTracker, FailedFirstAttemptLeavesTheSecondNotStarted)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  const RunUpdate update = run.feed(21000, middleCentre);
  ASSERT_EQ(update.finished.size(), 2u);
  EXPECT_EQ(update.finished[0].status, RunStatus::timeout);
  EXPECT_EQ(update.finished[1].attempt, 2);
  EXPECT_EQ(update.finished[1].status, RunStatus::notStarted);
  EXPECT_EQ(run.tracker.phase(), RunPhase::done);
  run.hold(21100, 30000, startCentre);
  EXPECT_EQ(run.taps, 1);
  EXPECT_EQ(run.results.size(), 2u);
}

GTEST_TEST(MazeRunTracker, SecondTapWaitsForTheRobotToBelieveItIsAtTheStart)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  run.feed(2000, goalCentre);
  run.believedAtStart = false; // HandleMaze is still in returnToStart
  run.hold(2100, 9000, startCentre); // in the start cell and still for far longer than settleMs
  EXPECT_EQ(run.taps, 1);
  EXPECT_EQ(run.tracker.phase(), RunPhase::returning);
  run.believedAtStart = true;
  run.feed(9100, startCentre); // settled, now believed
  EXPECT_EQ(run.taps, 2);
  EXPECT_EQ(run.tracker.phase(), RunPhase::running);
}

GTEST_TEST(MazeRunTracker, SpeedRunIsRecordedAtTheTap)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre); // tap 1: nothing known yet
  run.speedRunFound = true; // found while exploring; not what attempt 1 recorded
  run.feed(2000, goalCentre);
  ASSERT_EQ(run.results.size(), 1u);
  EXPECT_FALSE(run.results[0].speedRun);
  run.hold(2100, 4100, startCentre);
  ASSERT_EQ(run.taps, 2);
  run.speedRunFound = false; // after the tap: does not change the attempt
  run.feed(4500, goalCentre);
  ASSERT_EQ(run.results.size(), 2u);
  EXPECT_TRUE(run.results[1].speedRun);
}

GTEST_TEST(MazeRunTracker, ReturnTimeoutWithoutBeliefStillGivesNotStarted)
{
  Harness run(config(2));
  run.feed(0, startCentre);
  run.feed(1000, startCentre);
  run.feed(2000, goalCentre); // return starts at 2000
  run.believedAtStart = false;
  run.hold(2100, 31900, startCentre);
  EXPECT_EQ(run.taps, 1);
  run.feed(32000, startCentre);
  ASSERT_EQ(run.results.size(), 2u);
  EXPECT_EQ(run.results[1].status, RunStatus::notStarted);
  EXPECT_EQ(run.tracker.phase(), RunPhase::done);
}
