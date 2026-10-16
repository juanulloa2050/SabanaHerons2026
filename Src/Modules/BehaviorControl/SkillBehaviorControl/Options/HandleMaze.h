/**
 * @file HandleMaze.h
 *
 * This file defines the options of the maze behavior stub. In the playing state, the
 * robot stands still and reacts to single and double head button taps:
 *
 *   headFront  x1  says "start"
 *   headRear   x1  says the id of the last seen AprilTag ("tag 11"), or "no tag" if none was seen
 *                  in the last 2 s (self-test of the AprilTag detector, MZ-018)
 *   headMiddle x2  says "motion test" and runs the motion test sequence
 *   headMiddle x1  reserved for "positioned at start" (MZ-041)
 *
 * Triple taps stay reserved for the diagnostics in SkillBehaviorControl.cpp
 * and are not used here. Hit streaks are only reported after the streak has ended,
 * so a triple tap never triggers the single or double tap actions.
 *
 * Pressing any head button during a walk or turn of the motion test aborts it.
 * A step that does not finish within its time limit aborts the test as well.
 *
 * In SimRobot, the head buttons cannot be pressed. Each gesture can be simulated
 * once with a debug request instead (the robot must be in the playing state):
 *
 *   dr option:HandleMaze:headFront on        same as headFront x1
 *   dr option:HandleMaze:headRear on         same as headRear x1
 *   dr option:HandleMaze:headMiddleTwice on  same as headMiddle x2
 *   dr option:HandleMaze:abortMotionTest on  same as pressing a head button during the motion test
 */

/** The edge length of one maze cell in mm (the maze consists of 50 cm x 50 cm cells). */
static constexpr float mazeCellSize = 500.f;

/** The number of cells walked forward in the first step of the motion test. */
static constexpr int motionTestCells = 4;

/** The time the robot stands still (and beeps once) between two steps of the motion test (in ms). */
static constexpr int motionTestPauseTime = 2000;

/** The time the robot waits after announcing something until it accepts the next tap (in ms). */
static constexpr int mazeAnnouncementTime = 1500;

/** A tag that was last seen longer ago than this is announced as "no tag" (in ms). */
static constexpr int tagMemoryTime = 2000;

/** The maximum time the walking step of the motion test may take before the test is aborted (in ms). */
static constexpr int motionTestWalkTimeout = 30000;

/** The maximum time a turning step of the motion test may take before the test is aborted (in ms). */
static constexpr int motionTestTurnTimeout = 15000;

/** The number of steps of the motion test: walk forward, turn left, turn right, turn around. */
static constexpr int motionTestNumOfSteps = 4;

/** The sound that is played between the steps of the motion test (in Config/Sounds). */
static constexpr const char* motionTestBeep = "mazeBeep.wav";

int lastTagId = -1; /**< The id of the tag with the best decision margin in the last frame that showed a tag; -1 if none yet. */
unsigned lastTagTime = 0; /**< The time of that frame. */
std::string tagAnnouncement; /**< What the robot says when announcing the last seen tag. */

/** Remembers the best tag of the current frame, if there is one. */
void updateLastTag()
{
  const AprilTagPercept::Detection* best = nullptr;
  for(const AprilTagPercept::Detection& detection : theAprilTagPercept.detections)
    if(!best || detection.decisionMargin > best->decisionMargin)
      best = &detection;
  if(best)
  {
    lastTagId = best->id;
    lastTagTime = theAprilTagPercept.timestamp; // the percept can repeat over frames, so use its own time
  }
}

/** @return What to say about the last seen tag: "tag <id>" or "no tag". */
std::string currentTagAnnouncement() const
{
  if(lastTagId >= 0 && theFrameInfo.getTimeSince(lastTagTime) <= tagMemoryTime)
    return "tag " + std::to_string(lastTagId);
  return "no tag";
}

int motionTestStep = 0; /**< The step of the motion test that is currently executed (or the next one during a pause). */
Pose2f motionTestStartOdometry; /**< The odometry when the current step of the motion test started. */

/**
 * The angle of a turning step of the motion test.
 * @return The angle relative to the orientation at the beginning of the step.
 */
Angle motionTestTurnAngle() const
{
  switch(motionTestStep)
  {
    case 1:
      return 90_deg;
    case 2:
      return -90_deg;
    default:
      return 180_deg;
  }
}

/**
 * Whether any head button is currently pressed, which aborts a running motion test.
 * @return true if at least one head button is pressed.
 */
bool anyHeadButtonPressed() const
{
  return theEnhancedKeyStates.pressed[KeyStates::headFront] ||
         theEnhancedKeyStates.pressed[KeyStates::headMiddle] ||
         theEnhancedKeyStates.pressed[KeyStates::headRear];
}

option(HandleMaze)
{
  common_transition
  {
    updateLastTag();
    if(theGameState.isPlaying())
      goto playing;
    else
      goto notPlaying;
  }

  initial_state(notPlaying)
  {
    action
    {
      theLookForwardSkill();
      theStandSkill({.high = true});
    }
  }

  state(playing)
  {
    action
    {
      HandleMazeButtons();
    }
  }
}

option(HandleMazeButtons)
{
  initial_state(idle)
  {
    transition
    {
      DEBUG_RESPONSE_ONCE("option:HandleMaze:headFront")
        goto announceStart;
      DEBUG_RESPONSE_ONCE("option:HandleMaze:headRear")
      {
        tagAnnouncement = currentTagAnnouncement();
        goto announceTag;
      }
      DEBUG_RESPONSE_ONCE("option:HandleMaze:headMiddleTwice")
        goto announceMotionTest;
      if(theEnhancedKeyStates.hitStreak[KeyStates::headFront] == 1)
        goto announceStart;
      else if(theEnhancedKeyStates.hitStreak[KeyStates::headRear] == 1)
      {
        tagAnnouncement = currentTagAnnouncement();
        goto announceTag;
      }
      else if(theEnhancedKeyStates.hitStreak[KeyStates::headMiddle] == 2)
        goto announceMotionTest;
      // headMiddle x1 is reserved for "positioned at start" (MZ-041).
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
    }
  }

  state(announceStart)
  {
    transition
    {
      if(state_time > mazeAnnouncementTime)
        goto idle;
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
      theSaySkill({.text = "start"});
    }
  }

  state(announceTag)
  {
    transition
    {
      if(state_time > mazeAnnouncementTime)
        goto idle;
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
      theSaySkill({.text = tagAnnouncement});
    }
  }

  state(announceMotionTest)
  {
    transition
    {
      if(state_time > mazeAnnouncementTime)
      {
        motionTestStep = 0;
        motionTestStartOdometry = Pose2f(theOdometryData);
        goto motionTestWalk;
      }
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
      theSaySkill({.text = "motion test"});
    }
  }

  state(motionTestWalk)
  {
    transition
    {
      DEBUG_RESPONSE_ONCE("option:HandleMaze:abortMotionTest")
        goto motionTestAborted;
      if(anyHeadButtonPressed() || state_time > motionTestWalkTimeout)
        goto motionTestAborted;
      if(state_time > 500 && theWalkToPointSkill.isDone())
        goto motionTestPause;
    }
    action
    {
      // The target is fixed in the odometry frame, so it is independent of the self-localization.
      const Pose2f target = Pose2f(theOdometryData).inverse() * motionTestStartOdometry * Pose2f(motionTestCells * mazeCellSize, 0.f);
      theLookForwardSkill();
      theWalkToPointSkill({.target = target,
                           .disableObstacleAvoidance = true,
                           .disableAvoidFieldBorder = true});
    }
  }

  state(motionTestTurn)
  {
    transition
    {
      DEBUG_RESPONSE_ONCE("option:HandleMaze:abortMotionTest")
        goto motionTestAborted;
      if(anyHeadButtonPressed() || state_time > motionTestTurnTimeout)
        goto motionTestAborted;
      if(state_time > 500 && theTurnAngleSkill.isDone())
        goto motionTestPause;
    }
    action
    {
      theLookForwardSkill();
      theTurnAngleSkill({.angle = motionTestTurnAngle()});
    }
  }

  state(motionTestPause)
  {
    transition
    {
      if(state_time > motionTestPauseTime)
      {
        if(++motionTestStep >= motionTestNumOfSteps)
          goto idle;
        else
          goto motionTestTurn;
      }
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
      thePlaySoundSkill({.name = motionTestBeep});
    }
  }

  state(motionTestAborted)
  {
    transition
    {
      if(state_time > mazeAnnouncementTime)
        goto idle;
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
      theSaySkill({.text = "motion test aborted"});
    }
  }
}
