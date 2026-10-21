/**
 * @file MazeBlockedWalkTest.cpp
 *
 * Tests of the blocked walk detector (ticket MZ-046) on synthetic traces at 100 Hz, with the gyroscope levels that were measured
 * in SimRobot: a deviation of 0.06..0.19 rad/s walking free and 1..2 rad/s pushing against a panel.
 */

#include "Tools/Maze/MazeBlockedWalk.h"

#include <gtest/gtest.h>

namespace
{
  using namespace Maze;

  constexpr unsigned frameMs = 10;

  /** A trace generator: free gyro noise (uniform in +-0.3, deviation 0.17 rad/s) or a rocking robot (+-1.5, deviation 1.5 rad/s). */
  class Trace
  {
  public:
    float free()
    {
      state = state * 1664525u + 1013904223u;
      return (static_cast<float>(state >> 8) / static_cast<float>(1u << 24) - 0.5f) * 0.6f;
    }

    float rocking() { return (flip = !flip) ? 1.5f : -1.5f; }

  private:
    unsigned state = 12345;
    bool flip = false;
  };

  BlockedWalkInput walking(unsigned time, float gyro)
  {
    BlockedWalkInput input;
    input.timeMs = time;
    input.walkCommanded = true;
    input.forwardSpeed = 200.f;
    input.gyroZ = gyro;
    return input;
  }

  /** Feeds `duration` ms of `walking` frames from `time`; returns the time of the first blocked frame, or 0 if it never blocked. */
  template<typename Gyro>
  unsigned feed(BlockedWalkDetector& detector, unsigned& time, unsigned duration, Gyro gyro)
  {
    unsigned blockedAt = 0;
    for(unsigned end = time + duration; time < end; time += frameMs)
      if(detector.update(walking(time, gyro())) && !blockedAt)
        blockedAt = time;
    return blockedAt;
  }
}

GTEST_TEST(MazeBlockedWalk, AFreeWalkIsNeverBlocked)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  EXPECT_EQ(feed(detector, time, 120000, [&] { return trace.free(); }), 0u);
  EXPECT_FALSE(detector.blocked());
  EXPECT_LT(detector.gyroDeviation(), detector.parameters.gyroStdThreshold);
}

GTEST_TEST(MazeBlockedWalk, ABlockedWalkIsBlockedAfterTheTimeoutAndNotBefore)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  EXPECT_EQ(feed(detector, time, 500, [&] { return trace.free(); }), 0u);

  const unsigned contact = time;
  const unsigned blockedAt = feed(detector, time, 3000, [&] { return trace.rocking(); });
  ASSERT_NE(blockedAt, 0u);
  EXPECT_EQ(detector.blockedBy(), BlockedCause::gyro);
  // The window has to fill with the oscillation first (a few frames); the alarm then holds for the whole timeout.
  EXPECT_GE(blockedAt - contact, detector.parameters.blockedTimeoutMs);
  EXPECT_LE(blockedAt - contact, detector.parameters.blockedTimeoutMs + detector.parameters.gyroWindowMs);
  EXPECT_TRUE(detector.update(walking(time, 0.f))) << "latched: a quiet frame afterwards does not clear it";
}

GTEST_TEST(MazeBlockedWalk, ShortStartAndStopSpikesAreIgnored)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  for(int walk = 0; walk < 20; ++walk)
  {
    EXPECT_EQ(feed(detector, time, 150, [&] { return trace.rocking(); }), 0u) << "the jolt of the start";
    EXPECT_EQ(feed(detector, time, 2000, [&] { return trace.free(); }), 0u);
    EXPECT_EQ(feed(detector, time, 150, [&] { return trace.rocking(); }), 0u) << "the jolt of the stop";
    EXPECT_EQ(feed(detector, time, 800, [&] { return trace.free(); }), 0u);
  }
  EXPECT_FALSE(detector.blocked());
}

GTEST_TEST(MazeBlockedWalk, AGapInTheAlarmRestartsTheTimeout)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  // The alarm outlasts the burst by the window (the deviation falls once the window is quiet), so 600 ms of rocking hold for ~850 ms.
  EXPECT_EQ(feed(detector, time, 600, [&] { return trace.rocking(); }), 0u);
  EXPECT_EQ(feed(detector, time, 400, [&] { return trace.free(); }), 0u);
  EXPECT_EQ(feed(detector, time, 600, [&] { return trace.rocking(); }), 0u) << "two bursts are not one alarm of 1000 ms";
  EXPECT_NE(feed(detector, time, 600, [&] { return trace.rocking(); }), 0u);
}

GTEST_TEST(MazeBlockedWalk, AStandingRobotIsNeverBlockedWhateverTheGyroSays)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  for(; time < 5000; time += frameMs)
  {
    BlockedWalkInput input = walking(time, trace.rocking());
    input.walkCommanded = false; // Standing, turning on the spot or kicking.
    EXPECT_FALSE(detector.update(input));
  }
  for(; time < 10000; time += frameMs)
  {
    BlockedWalkInput input = walking(time, trace.rocking());
    input.forwardSpeed = 0.f; // Commanded walk without a forward part.
    input.bumperContact = true;
    EXPECT_FALSE(detector.update(input));
  }
  for(; time < 15000; time += frameMs)
  {
    BlockedWalkInput input = walking(time, trace.rocking());
    input.forwardSpeed = detector.parameters.minWalkSpeed - 1.f; // Too slow to be blocked.
    EXPECT_FALSE(detector.update(input));
  }
  EXPECT_FALSE(detector.blocked());
}

GTEST_TEST(MazeBlockedWalk, TheMinimumSpeedItselfCountsAsWalking)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned blockedAt = 0;
  for(unsigned time = 0; time < 3000 && !blockedAt; time += frameMs)
  {
    BlockedWalkInput input = walking(time, trace.rocking());
    input.forwardSpeed = detector.parameters.minWalkSpeed;
    if(detector.update(input))
      blockedAt = time;
  }
  EXPECT_NE(blockedAt, 0u);
}

GTEST_TEST(MazeBlockedWalk, AFootBumperContactDuringAWalkBlocksAfterTheHold)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  EXPECT_EQ(feed(detector, time, 1000, [&] { return trace.free(); }), 0u);

  const unsigned contact = time;
  unsigned blockedAt = 0;
  for(; time < contact + 1000 && !blockedAt; time += frameMs)
  {
    BlockedWalkInput input = walking(time, trace.free());
    input.bumperContact = true;
    if(detector.update(input))
      blockedAt = time;
  }
  ASSERT_NE(blockedAt, 0u);
  EXPECT_EQ(detector.blockedBy(), BlockedCause::bumper);
  EXPECT_EQ(blockedAt - contact, detector.parameters.bumperHoldMs) << "far sooner than the gyroscope, which stayed quiet";
}

GTEST_TEST(MazeBlockedWalk, ABrieflyBouncingBumperDoesNotBlock)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  for(int bounce = 0; bounce < 30; ++bounce)
    for(int frame = 0; frame < 12; ++frame, time += frameMs)
    {
      BlockedWalkInput input = walking(time, trace.free());
      input.bumperContact = frame < 6; // 60 ms of contact, then released.
      EXPECT_FALSE(detector.update(input));
    }
}

GTEST_TEST(MazeBlockedWalk, ResetClearsTheResultTheHoldsAndTheSamples)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 0;
  ASSERT_NE(feed(detector, time, 3000, [&] { return trace.rocking(); }), 0u);
  detector.reset();
  EXPECT_FALSE(detector.blocked());
  EXPECT_EQ(detector.blockedBy(), BlockedCause::none);
  EXPECT_EQ(detector.gyroDeviation(), 0.f);

  // A new action needs the whole timeout again.
  const unsigned start = time;
  const unsigned blockedAt = feed(detector, time, 3000, [&] { return trace.rocking(); });
  ASSERT_NE(blockedAt, 0u);
  EXPECT_GE(blockedAt - start, detector.parameters.blockedTimeoutMs);
}

GTEST_TEST(MazeBlockedWalk, AClockThatWentBackIsAReset)
{
  BlockedWalkDetector detector;
  Trace trace;
  unsigned time = 100000;
  EXPECT_EQ(feed(detector, time, 800, [&] { return trace.rocking(); }), 0u);
  time = 500; // A new session.
  EXPECT_EQ(feed(detector, time, 800, [&] { return trace.rocking(); }), 0u) << "the 800 ms of the old clock do not count";
  EXPECT_NE(feed(detector, time, 800, [&] { return trace.rocking(); }), 0u);
}
