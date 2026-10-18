/**
 * @file MazeDriveTest.cpp
 *
 * Tests of the decisions that HandleMaze takes besides the phases (ticket MZ-024): the speech queue (phrase separation,
 * bound) and the drive controller (which action to run, locking while the plan is stale, one scan per cell, time limits).
 */

#include "Tools/Maze/MazeDrive.h"

#include <gtest/gtest.h>

namespace
{
  using namespace Maze;

  /** Runs the queue for `frames` frames of 10 ms and returns what Say would have been called with, "" for a frame without a call. */
  std::vector<std::string> run(SpeechQueue& queue, unsigned& now, int frames)
  {
    std::vector<std::string> said;
    for(int i = 0; i < frames; ++i, now += 10)
    {
      const std::optional<std::string> text = queue.update(now);
      said.push_back(text ? *text : "");
    }
    return said;
  }

  /** The input of a robot at cell 9 whose plan describes its pose and whose exploration route walks forward to (1, 2). */
  DriveInput walkInput()
  {
    DriveInput input;
    input.planValid = true;
    input.planCurrent = true;
    input.routeFound = true;
    input.hasAction = true;
    input.nextAction = Action::forward;
    input.targetRow = 1;
    input.targetCol = 2;
    input.targetHeading = Heading::east;
    input.cell = 9;
    return input;
  }
}

GTEST_TEST(MazeSpeechQueue, SaysNothingWhileEmpty)
{
  SpeechQueue queue;
  unsigned now = 0;
  for(const std::string& text : run(queue, now, 5))
    EXPECT_TRUE(text.empty());
}

GTEST_TEST(MazeSpeechQueue, ASinglePhraseIsHeldForTheAnnouncementTime)
{
  SpeechQueue queue;
  queue.push("ready");
  unsigned now = 0;
  const std::vector<std::string> said = run(queue, now, 200); // 2 s
  int calls = 0;
  for(const std::string& text : said)
    if(!text.empty())
    {
      EXPECT_EQ(text, "ready");
      ++calls;
    }
  EXPECT_EQ(calls, static_cast<int>(announcementTimeMs / 10) + 1) << "every frame from 0 ms to 1500 ms inclusive";
  EXPECT_TRUE(queue.empty());
}

GTEST_TEST(MazeSpeechQueue, TheSamePhraseTwiceIsSeparatedByAFrameWithoutACall)
{
  SpeechQueue queue;
  queue.push("tag 11");
  queue.push("tag 11");
  unsigned now = 0;
  const std::vector<std::string> said = run(queue, now, 400);
  int gaps = 0, runs = 0;
  for(std::size_t i = 0; i < said.size(); ++i)
  {
    if(said[i].empty() && i > 0 && !said[i - 1].empty() && i + 1 < said.size() && !said[i + 1].empty())
      ++gaps;
    if(!said[i].empty() && (i == 0 || said[i - 1].empty()))
      ++runs;
  }
  EXPECT_EQ(gaps, 1) << "Say forgets the first phrase during the gap and says the second one";
  EXPECT_EQ(runs, 2);
  EXPECT_TRUE(queue.empty());
}

GTEST_TEST(MazeSpeechQueue, DifferentPhrasesAreSaidInOrder)
{
  SpeechQueue queue;
  queue.push("goal");
  queue.push("finished");
  unsigned now = 0;
  std::vector<std::string> order;
  for(const std::string& text : run(queue, now, 400))
    if(!text.empty() && (order.empty() || order.back() != text))
      order.push_back(text);
  EXPECT_EQ(order, (std::vector<std::string>{"goal", "finished"}));
}

GTEST_TEST(MazeSpeechQueue, TappingWhileSpeakingDoesNotGrowTheQueueWithoutBound)
{
  SpeechQueue queue;
  queue.push("first");
  unsigned now = 0;
  (void) run(queue, now, 10); // "first" is being said.
  for(int i = 0; i < 100; ++i)
    queue.push("tag " + std::to_string(i));
  EXPECT_EQ(queue.size(), maxQueuedPhrases);

  // The phrase that is being said is not cut off; the oldest waiting ones were dropped, the newest are kept.
  const std::vector<std::string> said = run(queue, now, 200);
  EXPECT_EQ(said.front(), "first");
  std::vector<std::string> order;
  for(const std::string& text : run(queue, now, static_cast<int>(maxQueuedPhrases) * 200))
    if(!text.empty() && (order.empty() || order.back() != text))
      order.push_back(text);
  ASSERT_FALSE(order.empty());
  EXPECT_EQ(order.back(), "tag 99");
  EXPECT_TRUE(queue.empty());
}

GTEST_TEST(MazeSpeechQueue, TheBoundAlsoHoldsWhileNothingIsBeingSaid)
{
  SpeechQueue queue;
  for(int i = 0; i < 50; ++i)
    queue.push(std::to_string(i));
  EXPECT_EQ(queue.size(), maxQueuedPhrases);
  unsigned now = 0;
  EXPECT_EQ(*queue.update(now), std::to_string(50 - static_cast<int>(maxQueuedPhrases))) << "the oldest were dropped";
}

GTEST_TEST(MazeDrive, ForwardActionWalksAndLocksTheTarget)
{
  DriveController drive;
  const DriveDecision decision = drive.choose(walkInput(), 1000);
  EXPECT_EQ(decision.next, DriveNext::walk);
  EXPECT_EQ(drive.kind(), DriveKind::walk);
  EXPECT_EQ(drive.targetRow(), 1);
  EXPECT_EQ(drive.targetCol(), 2);
  EXPECT_EQ(drive.targetHeading(), Heading::east);
}

GTEST_TEST(MazeDrive, ATurnActionTurnsToTheTargetHeading)
{
  DriveController drive;
  DriveInput input = walkInput();
  input.nextAction = Action::turnLeft;
  input.targetHeading = Heading::north;
  EXPECT_EQ(drive.choose(input, 0).next, DriveNext::turn);
  EXPECT_EQ(drive.kind(), DriveKind::turn);
  EXPECT_EQ(drive.targetHeading(), Heading::north);
}

GTEST_TEST(MazeDrive, AStalePlanKeepsTheLockedActionAndItsTarget)
{
  DriveController drive;
  ASSERT_EQ(drive.choose(walkInput(), 0).next, DriveNext::walk);
  DriveInput stale = walkInput();
  stale.planCurrent = false; // The pose moved on, the plan is for the old one.
  stale.targetRow = 5;
  EXPECT_EQ(drive.choose(stale, 100).next, DriveNext::keep);
  EXPECT_EQ(drive.kind(), DriveKind::walk);
  EXPECT_EQ(drive.targetRow(), 1) << "the lock is not overwritten by a stale plan";
}

GTEST_TEST(MazeDrive, AnInvalidPlanStandsStill)
{
  DriveController drive;
  ASSERT_EQ(drive.choose(walkInput(), 0).next, DriveNext::walk);
  DriveInput outside = walkInput();
  outside.planValid = false;
  outside.planCurrent = false;
  EXPECT_EQ(drive.choose(outside, 10).next, DriveNext::idle);
  EXPECT_EQ(drive.kind(), DriveKind::idle);
  EXPECT_TRUE(drive.actionOver(false, 10)) << "nothing runs, so the plan is read again in the next frame";
}

GTEST_TEST(MazeDrive, NoRouteStandsAndIsReportedOnce)
{
  DriveController drive;
  DriveInput input = walkInput();
  input.routeFound = false;
  DriveDecision decision = drive.choose(input, 0);
  EXPECT_EQ(decision.next, DriveNext::idle);
  EXPECT_TRUE(decision.reportNoRoute);
  decision = drive.choose(input, 10);
  EXPECT_EQ(decision.next, DriveNext::idle);
  EXPECT_FALSE(decision.reportNoRoute);
  input.routeFound = true; // A route again, and then none again: reported again.
  EXPECT_EQ(drive.choose(input, 20).next, DriveNext::walk);
  input.routeFound = false;
  EXPECT_TRUE(drive.choose(input, 30).reportNoRoute);

  input = walkInput();
  input.hasAction = false; // Already at the goal: a found route without action.
  EXPECT_EQ(drive.choose(input, 40).next, DriveNext::idle);
}

GTEST_TEST(MazeDrive, ACellWithUnknownEdgesIsScannedOnceAndOnlyWhenExploring)
{
  DriveController drive;
  DriveInput input = walkInput();
  input.unknownEdges = true;
  EXPECT_EQ(drive.choose(input, 0).next, DriveNext::scan);
  EXPECT_EQ(drive.kind(), DriveKind::scan);
  EXPECT_EQ(drive.choose(input, 3000).next, DriveNext::walk) << "the same cell is not scanned a second time";

  input.cell = 10; // Another cell with unknown edges is scanned.
  EXPECT_EQ(drive.choose(input, 6000).next, DriveNext::scan);
  input.cell = 11;
  input.unknownEdges = false; // No unknown edge: nothing to scan.
  EXPECT_EQ(drive.choose(input, 9000).next, DriveNext::walk);

  DriveController speedRun;
  input.speedRun = true;
  input.cell = 12;
  input.unknownEdges = true;
  EXPECT_EQ(speedRun.choose(input, 0).next, DriveNext::walk) << "a speed run never scans";
}

GTEST_TEST(MazeDrive, ANewPhaseMayScanTheCellAgain)
{
  DriveController drive;
  DriveInput input = walkInput();
  input.unknownEdges = true;
  ASSERT_EQ(drive.choose(input, 0).next, DriveNext::scan);
  ASSERT_EQ(drive.choose(input, 10).next, DriveNext::walk);
  drive.reset();
  EXPECT_EQ(drive.kind(), DriveKind::idle);
  EXPECT_EQ(drive.choose(input, 20).next, DriveNext::scan);
}

GTEST_TEST(MazeDrive, ActionsEndWhenTheSkillIsDoneOrTheTimeoutPasses)
{
  DriveController drive;
  EXPECT_TRUE(drive.actionOver(false, 0)) << "idle";

  ASSERT_EQ(drive.choose(walkInput(), 1000).next, DriveNext::walk);
  EXPECT_FALSE(drive.actionOver(false, 1000 + walkTimeoutMs));
  EXPECT_TRUE(drive.actionOver(false, 1000 + walkTimeoutMs + 1));
  EXPECT_TRUE(drive.actionOver(true, 1001));

  DriveInput turn = walkInput();
  turn.nextAction = Action::turnRight;
  ASSERT_EQ(drive.choose(turn, 5000).next, DriveNext::turn);
  EXPECT_FALSE(drive.actionOver(false, 5000 + turnTimeoutMs));
  EXPECT_TRUE(drive.actionOver(false, 5000 + turnTimeoutMs + 1));
  EXPECT_TRUE(drive.actionOver(true, 5001));

  DriveInput scan = walkInput();
  scan.unknownEdges = true;
  scan.cell = 33;
  ASSERT_EQ(drive.choose(scan, 9000).next, DriveNext::scan);
  EXPECT_FALSE(drive.actionOver(false, 9000 + scanTimeoutMs));
  EXPECT_TRUE(drive.actionOver(false, 9000 + scanTimeoutMs + 1));
  EXPECT_TRUE(drive.actionOver(true, 9001));
}
