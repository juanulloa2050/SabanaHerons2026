/**
 * @file HandleMaze.h
 *
 * This file defines the options of the maze behavior stub. In the playing state, the
 * robot stands still and reacts to single and double head button taps:
 *
 *   headFront  x1  says "start"
 *   headRear   x1  says "maze ready"
 *   headMiddle x2  says "motion test" and runs the motion test sequence
 *   headMiddle x1  reserved for "positioned at start" (MZ-041)
 *
 * Triple taps stay reserved for the diagnostics in SkillBehaviorControl.cpp
 * and are not used here. Hit streaks are only reported after the streak has ended,
 * so a triple tap never triggers the single or double tap actions.
 *
 * Pressing any head button during a walk or turn of the motion test aborts it.
 * A step that does not finish within its time limit aborts the test as well.
 */

/** The edge length of one maze cell in mm (the maze consists of 50 cm x 50 cm cells). */
static constexpr float mazeCellSize = 500.f;

/** The number of cells walked forward in the first step of the motion test. */
static constexpr int motionTestCells = 4;

/** The time the robot stands still (and beeps once) between two steps of the motion test (in ms). */
static constexpr int motionTestPauseTime = 2000;

/** The time the robot waits after announcing something until it accepts the next tap (in ms). */
static constexpr int mazeAnnouncementTime = 1500;

/** The maximum time the walking step of the motion test may take before the test is aborted (in ms). */
static constexpr int motionTestWalkTimeout = 30000;

/** The maximum time a turning step of the motion test may take before the test is aborted (in ms). */
static constexpr int motionTestTurnTimeout = 15000;

/** The number of steps of the motion test: walk forward, turn left, turn right, turn around. */
static constexpr int motionTestNumOfSteps = 4;

/** The sound that is played between the steps of the motion test (in Config/Sounds). */
static constexpr const char* motionTestBeep = "mazeBeep.wav";

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
      if(theEnhancedKeyStates.hitStreak[KeyStates::headFront] == 1)
        goto announceStart;
      else if(theEnhancedKeyStates.hitStreak[KeyStates::headRear] == 1)
        goto announceMazeReady;
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

  state(announceMazeReady)
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
      theSaySkill({.text = "maze ready"});
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
