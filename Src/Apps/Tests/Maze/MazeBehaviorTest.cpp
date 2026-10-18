/**
 * @file MazeBehaviorTest.cpp
 *
 * Tests of the phase machine of HandleMaze (ticket MZ-024): every transition, the phrase of each, the second run, and
 * the announce switch. The CABSL options and the skills are not involved.
 */

#include "Platform/File.h"
#include "Streaming/FunctionList.h"
#include "Tools/Maze/MazeBehavior.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace
{
  using namespace Maze;
  using Said = std::vector<std::string>;

  /** The input of a robot that is playing and standing on the ground. */
  BehaviorInput playingInput()
  {
    BehaviorInput input;
    input.playing = true;
    input.active = true;
    return input;
  }

  BehaviorInput tap(BehaviorInput input = playingInput())
  {
    input.headFront = true;
    return input;
  }

  BehaviorInput pickedUp()
  {
    BehaviorInput input = playingInput();
    input.active = false;
    return input;
  }

  /** The chest button: the game state still says playing, `Maze::isActive` is false. */
  BehaviorInput penalized()
  {
    BehaviorInput input = playingInput();
    input.penalized = true;
    input.active = false;
    return input;
  }

  BehaviorInput atGoal()
  {
    BehaviorInput input = playingInput();
    input.atGoal = true;
    return input;
  }

  /** A machine that is waiting for the tap. */
  MazeBehavior waiting()
  {
    MazeBehavior behavior;
    (void) behavior.update(playingInput());
    return behavior;
  }

  /** A machine that is exploring. */
  MazeBehavior exploring()
  {
    MazeBehavior behavior = waiting();
    (void) behavior.update(tap());
    return behavior;
  }

  /** A machine that is in standby after an exploration. */
  MazeBehavior standingBy()
  {
    MazeBehavior behavior = exploring();
    (void) behavior.update(pickedUp());
    return behavior;
  }

  BehaviorInput withRoute(BehaviorInput input)
  {
    input.speedRunFound = true;
    return input;
  }
}

GTEST_TEST(MazeBehavior, StartsNotPlayingAndSaysNothing)
{
  MazeBehavior behavior;
  EXPECT_EQ(behavior.phase(), Phase::notPlaying);
  BehaviorInput input;
  input.headFront = true; // Not playing: the tap means nothing.
  const BehaviorStep step = behavior.update(input);
  EXPECT_EQ(step.phase, Phase::notPlaying);
  EXPECT_FALSE(step.changed);
  EXPECT_TRUE(step.phrases.empty());
}

GTEST_TEST(MazeBehavior, PlayingEntersWaitingForStartAndSaysReady)
{
  MazeBehavior behavior;
  const BehaviorStep step = behavior.update(playingInput());
  EXPECT_EQ(step.phase, Phase::waitingForStart);
  EXPECT_TRUE(step.changed);
  EXPECT_EQ(step.phrases, Said({"ready"}));
  const BehaviorStep next = behavior.update(playingInput());
  EXPECT_FALSE(next.changed);
  EXPECT_TRUE(next.phrases.empty());
}

GTEST_TEST(MazeBehavior, PlayingWhilePickedUpStillWaitsForStart)
{
  MazeBehavior behavior;
  EXPECT_EQ(behavior.update(pickedUp()).phase, Phase::waitingForStart); // The robot boots while it is carried.
  EXPECT_EQ(behavior.update(tap(pickedUp())).phase, Phase::waitingForStart) << "a tap only counts while the robot is active";
  EXPECT_EQ(behavior.update(tap()).phase, Phase::exploration);
}

GTEST_TEST(MazeBehavior, WaitingForStartLeavesWhenTheGameStopsPlaying)
{
  MazeBehavior behavior = waiting();
  const BehaviorStep step = behavior.update(BehaviorInput());
  EXPECT_EQ(step.phase, Phase::notPlaying);
  EXPECT_TRUE(step.phrases.empty());
  EXPECT_EQ(behavior.update(playingInput()).phrases, Said({"ready"})) << "ready again when playing resumes";
}

GTEST_TEST(MazeBehavior, TheTapStartsTheExplorationWithoutAKnownRoute)
{
  MazeBehavior behavior = waiting();
  EXPECT_EQ(behavior.update(playingInput()).phase, Phase::waitingForStart) << "no tap, no start";
  const BehaviorStep step = behavior.update(tap());
  EXPECT_EQ(step.phase, Phase::exploration);
  EXPECT_TRUE(step.changed);
  EXPECT_EQ(step.phrases, Said({"exploring"}));
}

GTEST_TEST(MazeBehavior, TheTapStartsTheSpeedRunWhenARouteIsKnown)
{
  MazeBehavior behavior = waiting();
  const BehaviorStep step = behavior.update(withRoute(tap()));
  EXPECT_EQ(step.phase, Phase::speedRun);
  EXPECT_EQ(step.phrases, Said({"speed run"}));
}

GTEST_TEST(MazeBehavior, ExplorationEndsAtTheGoalWithGoalAndFinished)
{
  MazeBehavior behavior = exploring();
  EXPECT_EQ(behavior.update(playingInput()).phase, Phase::exploration);
  const BehaviorStep step = behavior.update(atGoal());
  EXPECT_EQ(step.phase, Phase::finished);
  EXPECT_TRUE(step.changed);
  EXPECT_EQ(step.phrases, Said({"goal", "finished"}));
  EXPECT_EQ(behavior.update(atGoal()).phase, Phase::finished);
}

GTEST_TEST(MazeBehavior, SpeedRunEndsAtTheGoalWithGoalAndFinished)
{
  MazeBehavior behavior = waiting();
  (void) behavior.update(withRoute(tap()));
  const BehaviorStep step = behavior.update(atGoal());
  EXPECT_EQ(step.phase, Phase::finished);
  EXPECT_EQ(step.phrases, Said({"goal", "finished"}));
}

GTEST_TEST(MazeBehavior, PickingTheRobotUpEntersStandbyFromEveryAttemptPhase)
{
  {
    MazeBehavior behavior = exploring();
    const BehaviorStep step = behavior.update(pickedUp());
    EXPECT_EQ(step.phase, Phase::standby);
    EXPECT_EQ(step.phrases, Said({"standby"}));
  }
  {
    MazeBehavior behavior = waiting();
    (void) behavior.update(withRoute(tap()));
    EXPECT_EQ(behavior.update(pickedUp()).phrases, Said({"standby"}));
    EXPECT_EQ(behavior.phase(), Phase::standby);
  }
  {
    MazeBehavior behavior = exploring();
    (void) behavior.update(atGoal());
    EXPECT_EQ(behavior.phase(), Phase::finished);
    EXPECT_EQ(behavior.update(pickedUp()).phrases, Said({"standby"}));
    EXPECT_EQ(behavior.phase(), Phase::standby);
  }
}

GTEST_TEST(MazeBehavior, LeavingPlayingInSimulationEntersStandbyToo)
{
  MazeBehavior behavior = exploring();
  const BehaviorStep step = behavior.update(BehaviorInput()); // gc initial: neither playing nor active.
  EXPECT_EQ(step.phase, Phase::standby);
  EXPECT_EQ(step.phrases, Said({"standby"}));
  EXPECT_EQ(behavior.update(BehaviorInput()).phase, Phase::standby) << "standby stays while the game does not play";
}

GTEST_TEST(MazeBehavior, PickUpBeatsTheGoal)
{
  MazeBehavior behavior = exploring();
  BehaviorInput input = pickedUp();
  input.atGoal = true;
  const BehaviorStep step = behavior.update(input);
  EXPECT_EQ(step.phase, Phase::standby);
  EXPECT_EQ(step.phrases, Said({"standby"}));
}

GTEST_TEST(MazeBehavior, StandbyNeedsAnActiveRobotAndATap)
{
  MazeBehavior behavior = standingBy();
  EXPECT_EQ(behavior.update(playingInput()).phase, Phase::standby) << "put down, but no tap yet";
  EXPECT_TRUE(behavior.update(playingInput()).phrases.empty()) << "ready is said once";
  EXPECT_EQ(behavior.update(tap(pickedUp())).phase, Phase::standby) << "a tap while it is still carried";
  EXPECT_EQ(behavior.update(tap(BehaviorInput())).phase, Phase::standby) << "a tap while the game does not play";
}

GTEST_TEST(MazeBehavior, BecomingActiveAgainInStandbySaysReadyOnceEveryTime)
{
  MazeBehavior behavior = exploring();
  Said said;
  const auto run = [&](const BehaviorInput& input)
  {
    const BehaviorStep step = behavior.update(input);
    said.insert(said.end(), step.phrases.begin(), step.phrases.end());
  };
  for(int cycle = 0; cycle < 3; ++cycle)
  {
    run(pickedUp());
    run(pickedUp());
    run(playingInput());
    run(playingInput());
    EXPECT_EQ(behavior.phase(), Phase::standby);
  }
  run(tap());
  EXPECT_EQ(said, Said({"standby", "ready", "standby", "ready", "standby", "ready", "exploring"}));
}

GTEST_TEST(MazeBehavior, ReadyAfterTheGameReturnsToPlayingInStandby)
{
  MazeBehavior behavior = exploring();
  EXPECT_EQ(behavior.update(BehaviorInput()).phrases, Said({"standby"})) << "gc initial";
  EXPECT_TRUE(behavior.update(BehaviorInput()).phrases.empty());
  EXPECT_EQ(behavior.update(playingInput()).phrases, Said({"ready"})) << "gc playing";
  EXPECT_EQ(behavior.update(tap()).phrases, Said({"exploring"}));
}

GTEST_TEST(MazeBehavior, ATapOnTheFirstActiveFrameStartsWithoutReady)
{
  MazeBehavior behavior = standingBy();
  EXPECT_EQ(behavior.update(tap()).phrases, Said({"exploring"}));
}

GTEST_TEST(MazeBehavior, DisabledAnnouncementsSayNoReadyInStandby)
{
  MazeBehavior behavior = standingBy();
  BehaviorInput input = playingInput();
  input.announce = false;
  EXPECT_TRUE(behavior.update(input).phrases.empty());
  EXPECT_EQ(behavior.phase(), Phase::standby);
}

GTEST_TEST(MazeBehavior, APenaltyIsAPickUp)
{
  MazeBehavior behavior = exploring();
  const BehaviorStep step = behavior.update(penalized()); // The game state still says playing.
  EXPECT_EQ(step.phase, Phase::standby);
  EXPECT_EQ(step.phrases, Said({"standby"}));
  EXPECT_EQ(behavior.update(tap(penalized())).phase, Phase::standby) << "no start while penalized";
  EXPECT_EQ(behavior.update(playingInput()).phrases, Said({"ready"})) << "unpenalized";
  EXPECT_EQ(behavior.update(tap()).phrases, Said({"exploring"}));
}

GTEST_TEST(MazeBehavior, APenaltyIsStandbyEvenIfActiveWasNotCleared)
{
  MazeBehavior behavior = exploring();
  BehaviorInput input = playingInput();
  input.penalized = true; // active left true: the machine does not trust it while penalized.
  EXPECT_EQ(behavior.update(input).phase, Phase::standby);
}

GTEST_TEST(MazeBehavior, PenalizedWhileWaitingForStartLeavesAndSaysReadyWhenReleased)
{
  MazeBehavior behavior = waiting();
  EXPECT_EQ(behavior.update(penalized()).phase, Phase::notPlaying);
  EXPECT_EQ(behavior.update(playingInput()).phrases, Said({"ready"}));
  EXPECT_EQ(behavior.phase(), Phase::waitingForStart);
}

GTEST_TEST(MazeBehavior, StandbyRunsTheKnownRoute)
{
  MazeBehavior behavior = standingBy();
  const BehaviorStep step = behavior.update(withRoute(tap()));
  EXPECT_EQ(step.phase, Phase::speedRun);
  EXPECT_EQ(step.phrases, Said({"speed run"}));
}

GTEST_TEST(MazeBehavior, StandbyExploresAgainWithoutARouteAndSaysExploringAgain)
{
  MazeBehavior behavior = standingBy();
  const BehaviorStep step = behavior.update(tap());
  EXPECT_EQ(step.phase, Phase::exploration);
  EXPECT_EQ(step.phrases, Said({"exploring"})) << "the second run announces itself again";
}

GTEST_TEST(MazeBehavior, EverySecondRunSaysItsPhraseAgain)
{
  MazeBehavior behavior = exploring();
  for(int run = 0; run < 3; ++run)
  {
    EXPECT_EQ(behavior.update(pickedUp()).phrases, Said({"standby"}));
    const BehaviorStep step = behavior.update(tap());
    EXPECT_EQ(step.phase, Phase::exploration);
    EXPECT_EQ(step.phrases, Said({"exploring"}));
  }
}

GTEST_TEST(MazeBehavior, TheFullCompetitionFlow)
{
  MazeBehavior behavior;
  std::vector<std::string> said;
  const auto run = [&](const BehaviorInput& input)
  {
    const BehaviorStep step = behavior.update(input);
    said.insert(said.end(), step.phrases.begin(), step.phrases.end());
  };
  run(playingInput());          // ready
  run(tap());                   // exploring
  run(atGoal());                // goal, finished
  run(pickedUp());              // standby
  run(playingInput());          // put down: ready
  run(withRoute(tap()));        // speed run
  run(atGoal());                // goal, finished
  EXPECT_EQ(behavior.phase(), Phase::finished);
  EXPECT_EQ(said, Said({"ready", "exploring", "goal", "finished", "standby", "ready", "speed run", "goal", "finished"}));
}

GTEST_TEST(MazeBehavior, SaysTheDeducedStartOnce)
{
  MazeBehavior behavior = exploring();
  BehaviorInput input = playingInput();
  input.startKnown = true;
  input.startCell = 56;
  EXPECT_EQ(behavior.update(input).phrases, Said({"start 56"}));
  EXPECT_TRUE(behavior.update(input).phrases.empty());
}

GTEST_TEST(MazeBehavior, DoesNotAnnounceAGivenStartOrOneThatWasKnownAtBoot)
{
  {
    MazeBehavior behavior;
    BehaviorInput input;
    input.startKnown = true;
    input.startGiven = true;
    input.startCell = 7;
    (void) behavior.update(input);
    input.playing = true;
    input.active = true;
    for(int i = 0; i < 3; ++i)
      for(const std::string& phrase : behavior.update(input).phrases)
        EXPECT_EQ(phrase.find("start"), std::string::npos) << phrase;
  }
  {
    MazeBehavior behavior;
    BehaviorInput input;
    input.startKnown = true; // known before the first frame, not given: nothing happened that could be announced.
    input.startCell = 63;
    EXPECT_TRUE(behavior.update(input).phrases.empty());
  }
}

GTEST_TEST(MazeBehavior, TheDeducedStartCanComeWithAPhaseChange)
{
  MazeBehavior behavior = exploring();
  BehaviorInput input = atGoal();
  input.startKnown = true;
  input.startCell = 0;
  EXPECT_EQ(behavior.update(input).phrases, Said({"goal", "finished", "start 0"}));
}

GTEST_TEST(MazeBehavior, DisabledAnnouncementsKeepThePhasesAndSayNothing)
{
  MazeBehavior behavior;
  const auto quiet = [](BehaviorInput input)
  {
    input.announce = false;
    return input;
  };
  EXPECT_EQ(behavior.update(quiet(playingInput())).phase, Phase::waitingForStart);
  EXPECT_EQ(behavior.update(quiet(tap())).phase, Phase::exploration);
  BehaviorInput goal = quiet(atGoal());
  goal.startKnown = true;
  goal.startCell = 63;
  BehaviorStep step = behavior.update(goal);
  EXPECT_EQ(step.phase, Phase::finished);
  EXPECT_TRUE(step.phrases.empty());
  step = behavior.update(quiet(pickedUp()));
  EXPECT_EQ(step.phase, Phase::standby);
  EXPECT_TRUE(step.phrases.empty());
  step = behavior.update(quiet(tap()));
  EXPECT_EQ(step.phase, Phase::exploration);
  EXPECT_TRUE(step.phrases.empty());
}

GTEST_TEST(MazeBehavior, TagPhrases)
{
  EXPECT_EQ(tagPhrase(11, true), "tag 11");
  EXPECT_EQ(tagPhrase(0, true), "tag 0");
  EXPECT_EQ(tagPhrase(11, false), "no tag");
  EXPECT_EQ(tagPhrase(-1, true), "no tag");
}

GTEST_TEST(MazeBehavior, PhaseNames)
{
  EXPECT_STREQ(phaseName(Phase::speedRun), "speedRun");
  EXPECT_STREQ(phaseName(Phase::waitingForStart), "waitingForStart");
}

GTEST_TEST(MazeAnnounce, TheShippedConfigsAnnounce)
{
  static const bool registered = (FunctionList::execute(), true);
  (void) registered;
  for(const char* scenario : {"Maze", "MazeSafe"})
  {
    AnnounceFile file;
    file.announce.enabled = false;
    EXPECT_TRUE(AnnounceFile::load(std::string(File::getBHDir()) + "/Config/Scenarios/" + scenario + "/handleMaze.cfg", file)) << scenario;
    EXPECT_TRUE(file.announce.enabled) << scenario;
  }
}

GTEST_TEST(MazeAnnounce, EnabledFalseSilencesAndOtherEntriesAreIgnored)
{
  static const bool registered = (FunctionList::execute(), true);
  (void) registered;
  // The temporary directory exists and is writable on every machine; the stream is checked so that a failure is not mistaken for a missing file.
  const std::string name = std::string("mazeAnnounceTest_") + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".cfg";
  const std::string path = (std::filesystem::temp_directory_path() / name).string();
  {
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open()) << path;
    out << "start = {cell = 63; heading = north;};\nannounce = {\n  enabled = false;\n};\n";
  }
  AnnounceFile file;
  EXPECT_TRUE(AnnounceFile::load(path, file));
  EXPECT_FALSE(file.announce.enabled);
  std::remove(path.c_str());

  AnnounceFile missing;
  EXPECT_FALSE(AnnounceFile::load(std::string(File::getBHDir()) + "/Config/Scenarios/Maze/doesNotExist.cfg", missing));
  EXPECT_TRUE(missing.announce.enabled) << "announcements are on by default";
}
