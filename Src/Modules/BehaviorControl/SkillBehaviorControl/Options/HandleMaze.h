/**
 * @file HandleMaze.h
 *
 * This file defines the options of the maze behavior (ticket MZ-024, roadmap 4.7). The phase of the attempt is decided
 * by the pure state machine Maze::MazeBehavior (Tools/Maze/MazeBehavior.h); the common transition of HandleMaze feeds it and
 * goes to the CABSL state of the phase it returns:
 *
 *   notPlaying       stand, look forward
 *   waitingForStart  stand, look forward, head gestures ("ready")
 *   exploration      walk the exploration route of the MazePlan cell by cell ("exploring")
 *   speedRun         walk the speed-run route (known-open edges only), no wall scans ("speed run")
 *   standby          stand (picked up, or the game stopped playing); the map is kept ("standby")
 *   finished         stand, a goal cell is reached ("goal", "finished")
 *
 * The robot says what it does with the Say skill, in parallel with the motion and only if `announce.enabled` of
 * handleMaze.cfg is true. The head buttons (a tap is reported after the streak has ended):
 *
 *   headFront  x1  starts the attempt: exploration, or the speed run if the map holds a route to the goal
 *   headRear   x1  says the id of the last seen AprilTag ("tag 11"), or "no tag" if none was seen
 *                  in the last 2 s (self-test of the AprilTag detector, MZ-018)
 *   headMiddle x2  says "motion test" and runs the motion test sequence
 *   headMiddle x1  reserved for "positioned at start" (MZ-041)
 *
 * Triple taps stay reserved for the diagnostics in SkillBehaviorControl.cpp and are not used here.
 * headRear and headMiddle x2 only work while the robot stands (waitingForStart, standby, finished).
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

/** The maximum time a walk to a neighbouring cell may take before the plan is read again (in ms). */
static constexpr int mazeWalkTimeout = 20000;

/** The maximum time a turn may take before the plan is read again (in ms). */
static constexpr int mazeTurnTimeout = 12000;

/** The maximum time a wall scan may take before the plan is read again (in ms). */
static constexpr int mazeScanTimeout = 2500;

/** The number of steps of the motion test: walk forward, turn left, turn right, turn around. */
static constexpr int motionTestNumOfSteps = 4;

/** The sound that is played between the steps of the motion test (in Config/Sounds). */
static constexpr const char* motionTestBeep = "mazeBeep.wav";

int lastTagId = -1; /**< The id of the tag with the best decision margin in the last frame that showed a tag; -1 if none yet. */
unsigned lastTagTime = 0; /**< The time of that frame. */

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
  return Maze::tagPhrase(lastTagId, theFrameInfo.getTimeSince(lastTagTime) <= tagMemoryTime);
}

/** What the drive option of an attempt is doing. */
enum class MazeDriveKind
{
  idle,   /**< Standing: there is no route (or no pose) to follow. */
  walk,   /**< MazeFollowCorridor to the neighbouring cell. */
  turn,   /**< MazeTurn to the next heading. */
  scan,   /**< MazeScanWalls in a cell with unknown edges. */
};

/** What to do next at a cell centre. */
enum class MazeDriveNext
{
  idle,
  walk,
  turn,
  scan,
  keep, /**< The plan does not describe the current pose yet: go on with the current action. */
};

Maze::MazeBehavior mazeBehavior; /**< The phase of the attempt. */
bool announceEnabled = true; /**< `announce.enabled` of handleMaze.cfg, read by the constructor. */
std::vector<std::string> speechQueue; /**< What the robot has still to say; the first one is being said. */
bool speechRunning = false; /**< The first phrase of the queue has been handed to Say. */
unsigned speechStartTime = 0; /**< The time when the first phrase of the queue was handed to Say. */
bool speechPause = false; /**< Skip one frame so that Say forgets the last phrase and says the same one again. */
bool motionTestRunning = false; /**< The motion test (or its announcement) is running: head taps do not start an attempt. */
unsigned ignoreTapsUntil = 0; /**< The end of a motion test can report a tap late; it does not start an attempt. */

bool driveSpeedRun = false; /**< The route that MazeDrive follows: the speed run (true) or the exploration. */
MazeDriveKind driveKind = MazeDriveKind::idle; /**< The action that is running. */
unsigned driveStartTime = 0; /**< The time when it started. */
int lockedRow = 0; /**< The target of the running walk, or the cell of the running turn. */
int lockedCol = 0;
MazeCellPose::Heading lockedHeading = MazeCellPose::north; /**< The heading to keep (walk) or to reach (turn). */
int scannedCell = -1; /**< The cell where the walls were scanned last in this attempt. */
bool noRouteReported = false; /**< "No route" was written to the console since the last route. */

/** Queues something to say; nothing is said if the announcements are off. */
void queueSpeech(const std::string& text)
{
  if(announceEnabled)
    speechQueue.push_back(text);
}

/**
 * Says the queued phrases one after the other, each for mazeAnnouncementTime. It only calls Say, so it runs in parallel
 * with the motion skills and never blocks them. Say drops the same text twice in a row until it was not called for a frame,
 * so a frame without the call separates the phrases.
 */
void speak()
{
  if(speechQueue.empty())
    return;
  if(speechPause)
  {
    speechPause = false;
    return;
  }
  if(!speechRunning)
  {
    speechRunning = true;
    speechStartTime = theFrameInfo.time;
  }
  theSaySkill({.text = speechQueue.front()});
  if(theFrameInfo.getTimeSince(speechStartTime) >= mazeAnnouncementTime)
  {
    speechQueue.erase(speechQueue.begin());
    speechRunning = false;
    speechPause = true;
  }
}

/** @return Whether a head front tap happened in this frame (a button, or the debug request in SimRobot). */
bool headFrontTapped() const
{
  bool tapped = theEnhancedKeyStates.hitStreak[KeyStates::headFront] == 1;
  DEBUG_RESPONSE_ONCE("option:HandleMaze:headFront")
    tapped = true;
  return tapped;
}

/** @return What the phase machine needs to know about this frame. */
Maze::BehaviorInput mazeInput() const
{
  Maze::BehaviorInput input;
  input.announce = announceEnabled;
  input.playing = theGameState.isPlaying();
  input.active = Maze::isActive(input.playing, theFallDownState.state);
  input.headFront = headFrontTapped() && !motionTestRunning && theFrameInfo.time >= ignoreTapsUntil;
  input.atGoal = theMazePlan.valid && theMazePlan.atGoal;
  input.speedRunFound = theMazePlan.valid && theMazePlan.speedRun.found;
  input.startKnown = theMazeStart.known;
  input.startGiven = theMazeStart.given;
  input.startCell = theMazeStart.cell;
  return input;
}

/** The maze heading from cell (row, col) to a neighbouring cell. */
static Maze::Heading mazeHeadingBetween(int fromCell, int toCell)
{
  const int rows = Maze::cellRow(toCell) - Maze::cellRow(fromCell);
  const int cols = Maze::cellCol(toCell) - Maze::cellCol(fromCell);
  if(rows < 0)
    return Maze::Heading::north;
  if(rows > 0)
    return Maze::Heading::south;
  return cols > 0 ? Maze::Heading::east : Maze::Heading::west;
}

/** Writes the speed run to the console so that a log shows that it uses edges known to be open only. */
void logSpeedRun() const
{
  const MazeRoute& route = theMazePlan.speedRun;
  std::string cells, actions;
  int notOpen = 0;
  for(std::size_t i = 0; i < route.cells.size(); ++i)
  {
    cells += (i ? " " : "") + std::to_string(route.cells[i]);
    if(i + 1 < route.cells.size())
    {
      const Maze::Heading heading = mazeHeadingBetween(route.cells[i], route.cells[i + 1]);
      if(theMazeMap.get(Maze::cellRow(route.cells[i]), Maze::cellCol(route.cells[i]), heading) != MazeMap::open)
        ++notOpen;
    }
  }
  for(std::size_t i = 0; i < route.actions.size(); ++i)
    actions += std::string(i ? " " : "") + TypeRegistry::getEnumName(route.actions[i]);
  OUTPUT_TEXT("HandleMaze: speed run over " << static_cast<int>(route.cells.size()) << " cells [" << cells << "], actions [" << actions << "], estimated "
              << route.estimatedTimeMs << " ms, " << route.forwardSteps << " steps, " << route.turns() << " turns; "
              << (notOpen == 0 ? "all edges known to be open" : std::to_string(notOpen) + " EDGES NOT KNOWN TO BE OPEN") << ".");
}

/** Bookkeeping when a phase begins. */
void enterPhase(Maze::Phase phase)
{
  OUTPUT_TEXT("HandleMaze: " << Maze::phaseName(phase) << ".");
  driveKind = MazeDriveKind::idle;
  scannedCell = -1;
  noRouteReported = false;
  if(phase == Maze::Phase::speedRun)
    logSpeedRun();
}

/** @return Whether the plan was computed for the pose the robot has now. */
bool planCurrent() const
{
  return theMazePlan.valid && theMazePlan.row == theMazeCellPose.row && theMazePlan.col == theMazeCellPose.col &&
         theMazePlan.heading == theMazeCellPose.heading;
}

/** @return Whether any edge of the cell the robot stands in is unknown. */
bool currentCellHasUnknownEdges() const
{
  for(int heading = 0; heading < Maze::headingCount; ++heading)
    if(theMazeMap.get(theMazeCellPose.row, theMazeCellPose.col, static_cast<Maze::Heading>(heading)) == MazeMap::unknown)
      return true;
  return false;
}

/** @return Whether the action that runs is over: done, or too slow, so that the plan is read again. */
bool driveActionOver() const
{
  switch(driveKind)
  {
    case MazeDriveKind::walk:
      return theMazeFollowCorridorSkill.isDone() || theFrameInfo.getTimeSince(driveStartTime) > mazeWalkTimeout;
    case MazeDriveKind::turn:
      return theMazeTurnSkill.isDone() || theFrameInfo.getTimeSince(driveStartTime) > mazeTurnTimeout;
    case MazeDriveKind::scan:
      return theMazeScanWallsSkill.isDone() || theFrameInfo.getTimeSince(driveStartTime) > mazeScanTimeout;
    default:
      return true;
  }
}

/**
 * Reads the plan at a cell centre and locks the next action until its skill is done. The exploration route changes
 * whenever the map learns an edge, so only its first action is binding; the speed run takes the first action of the
 * route as well, which is the same list because the map no longer changes along known-open edges.
 */
MazeDriveNext chooseNextDriveAction(bool speedRun)
{
  if(!planCurrent())
    return theMazePlan.valid ? MazeDriveNext::keep : MazeDriveNext::idle;
  const MazeRoute& route = speedRun ? theMazePlan.speedRun : theMazePlan.exploration;
  if(!route.found || route.nextAction == MazeRoute::none)
  {
    if(!noRouteReported)
      OUTPUT_TEXT("HandleMaze: no " << (speedRun ? "speed run" : "exploration") << " route from cell " << theMazeCellPose.cell() << ", standing.");
    noRouteReported = true;
    return MazeDriveNext::idle;
  }
  noRouteReported = false;
  const unsigned now = theFrameInfo.time;
  if(!speedRun && scannedCell != theMazeCellPose.cell() && currentCellHasUnknownEdges())
  {
    scannedCell = theMazeCellPose.cell();
    driveStartTime = now;
    driveKind = MazeDriveKind::scan;
    return MazeDriveNext::scan;
  }
  lockedRow = route.targetRow;
  lockedCol = route.targetCol;
  lockedHeading = route.targetHeading;
  driveStartTime = now;
  if(route.nextAction == MazeRoute::forward)
  {
    driveKind = MazeDriveKind::walk;
    return MazeDriveNext::walk;
  }
  driveKind = MazeDriveKind::turn;
  return MazeDriveNext::turn;
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
    const Maze::BehaviorStep step = mazeBehavior.update(mazeInput());
    for(const std::string& phrase : step.phrases)
      queueSpeech(phrase);
    if(step.changed)
      enterPhase(step.phase);
    switch(step.phase)
    {
      case Maze::Phase::waitingForStart:
        goto waitingForStart;
      case Maze::Phase::exploration:
        goto exploration;
      case Maze::Phase::standby:
        goto standby;
      case Maze::Phase::speedRun:
        goto speedRun;
      case Maze::Phase::finished:
        goto finished;
      default:
        goto notPlaying;
    }
  }

  initial_state(notPlaying)
  {
    action
    {
      speak();
      theLookForwardSkill();
      theStandSkill({.high = true});
    }
  }

  state(waitingForStart)
  {
    action
    {
      speak();
      HandleMazeButtons();
    }
  }

  state(exploration)
  {
    action
    {
      speak();
      driveSpeedRun = false;
      MazeDrive();
    }
  }

  state(standby)
  {
    action
    {
      speak();
      HandleMazeButtons();
    }
  }

  state(speedRun)
  {
    action
    {
      speak();
      driveSpeedRun = true;
      MazeDrive();
    }
  }

  state(finished)
  {
    action
    {
      speak();
      HandleMazeButtons();
    }
  }
}

/**
 * Walks the route of the plan, one action at a time: the plan is read at a cell centre, its first action is locked until
 * its skill is done, then the plan is read again. In the exploration, a cell with unknown edges is scanned first.
 * The member `driveSpeedRun` tells which route to follow: the speed-run route (known-open edges only, no scans) or the
 * exploration route.
 */
option(MazeDrive)
{
  common_transition
  {
    if(driveActionOver())
    {
      switch(chooseNextDriveAction(driveSpeedRun))
      {
        case MazeDriveNext::walk:
          goto walk;
        case MazeDriveNext::turn:
          goto turn;
        case MazeDriveNext::scan:
          goto scan;
        case MazeDriveNext::idle:
          goto idle;
        default: // keep
          break;
      }
    }
  }

  initial_state(idle)
  {
    action
    {
      theLookForwardSkill();
      theStandSkill();
    }
  }

  state(walk)
  {
    action
    {
      theLookForwardSkill();
      theMazeFollowCorridorSkill({.targetRow = lockedRow,
                                  .targetCol = lockedCol,
                                  .heading = lockedHeading});
    }
  }

  state(turn)
  {
    action
    {
      theLookForwardSkill();
      theMazeTurnSkill({.heading = lockedHeading});
    }
  }

  state(scan)
  {
    action
    {
      theStandSkill();
      theMazeScanWallsSkill();
    }
  }
}

option(HandleMazeButtons)
{
  initial_state(idle)
  {
    transition
    {
      // headFront x1 starts the attempt, see HandleMaze.
      DEBUG_RESPONSE_ONCE("option:HandleMaze:headRear")
      {
        queueSpeech(currentTagAnnouncement());
        goto announceTag;
      }
      DEBUG_RESPONSE_ONCE("option:HandleMaze:headMiddleTwice")
      {
        queueSpeech("motion test");
        motionTestRunning = true;
        goto announceMotionTest;
      }
      if(theEnhancedKeyStates.hitStreak[KeyStates::headRear] == 1)
      {
        queueSpeech(currentTagAnnouncement());
        goto announceTag;
      }
      else if(theEnhancedKeyStates.hitStreak[KeyStates::headMiddle] == 2)
      {
        queueSpeech("motion test");
        motionTestRunning = true;
        goto announceMotionTest;
      }
      // headMiddle x1 is reserved for "positioned at start" (MZ-041).
    }
    action
    {
      motionTestRunning = false;
      theLookForwardSkill();
      theStandSkill();
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
    }
  }

  state(motionTestWalk)
  {
    transition
    {
      DEBUG_RESPONSE_ONCE("option:HandleMaze:abortMotionTest")
      {
        queueSpeech("motion test aborted");
        goto motionTestAborted;
      }
      if(anyHeadButtonPressed() || state_time > motionTestWalkTimeout)
      {
        queueSpeech("motion test aborted");
        goto motionTestAborted;
      }
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
      {
        queueSpeech("motion test aborted");
        goto motionTestAborted;
      }
      if(anyHeadButtonPressed() || state_time > motionTestTurnTimeout)
      {
        queueSpeech("motion test aborted");
        goto motionTestAborted;
      }
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
        {
          ignoreTapsUntil = theFrameInfo.time + mazeAnnouncementTime;
          goto idle;
        }
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
      {
        ignoreTapsUntil = theFrameInfo.time + mazeAnnouncementTime;
        goto idle;
      }
    }
    action
    {
      theLookForwardSkill();
      theStandSkill();
    }
  }
}
