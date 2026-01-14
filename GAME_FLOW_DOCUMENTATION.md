# Game Flow Implementation Documentation

## Overview
This document describes the implementation of the SPL game flow for kick-offs in the SabanaHerons2026 codebase. The implementation follows the RoboCup Standard Platform League rules for game initialization and kick-off procedures.

**Note**: This documentation references specific code locations and line numbers that were accurate at the time of writing (January 2026). As the codebase evolves, these references may shift, but the conceptual organization should remain similar.

## Game Flow States

### 1. UNSTIFF State
**Purpose**: Initial state where robots are placed on touchlines (< 2 min before kick-off)

**Implementation**:
- `GameState::PlayerState::unstiff` (GameState.h:69)
- Robots are physically unstiff
- GameController messages are ignored while in this state

**Transition to Active**:
- **Manual**: Press chest button → triggers transition to active (GameStateProvider.cpp:119-124)
- **GameController**: Receives PENALTY_NONE → becomes active (GameStateProvider.cpp:942-943)

### 2. INITIAL State (beforeHalf)
**Purpose**: Robots placed on touchlines facing opposite side (< 1 min before game)

**Implementation**:
- `GameState::beforeHalf` (GameState.h:21)
- Corresponds to GameController STATE_INITIAL (GameStateProvider.cpp:845-848)

**Robot Behavior**:
- Stands high with head looking straight (HandleGameState.h:31-40)
- Stationary, no movement

### 3. STANDBY State
**Purpose**: Waiting for signal to transition to READY

**Implementation**:
- `GameState::standby` (GameState.h:22)
- Corresponds to GameController STATE_STANDBY (GameStateProvider.cpp:850-853)

**Robot Behavior**:
- Same as INITIAL: stands high, looks straight
- No human intervention allowed after this point

**Key Note**: Robots must be in `active` PlayerState to process game state transitions.

### 4. READY State (Setup Phase)
**Purpose**: Robots walk to legal kick-off positions

**Implementation**:
- `GameState::setupOwnKickOff` or `GameState::setupOpponentKickOff`
- Corresponds to GameController STATE_READY (GameStateProvider.cpp:855-863)

**Two Transition Methods**:

#### Method A: Referee Gesture Detection (Optional)
- **Module**: InitialToReadyHandler (InitialToReadyHandler.cpp)
- **Trigger**: Visual detection of referee "ready" gesture
- **Process**:
  1. Robot with referee in sight detects gesture (RefereePercept.gesture == initialToReady)
  2. Detection shared with team via TeamData
  3. Pawn sacrifice mechanism: robots incrementally move to avoid mass penalty
  4. Once pawn is not penalized, team transitions
- **Code**: GameStateProvider.cpp:342-350
- **Flag**: Sets `gameStateOverridden = true` (optimistic transition)

#### Method B: GameController Signal (Authoritative)
- **Trigger**: GameController sends STATE_READY
- **Code**: GameStateProvider.cpp:855-863
- **Authority**: This is the definitive confirmation
- **Integration**: Confirms or overrides gesture-based transition

**Robot Behavior**:
- Walks to legal kick-off position (HandleGameState.h:42-57)
- Uses SkillRequest to determine target position
- Arm contact detection active
- Obstacle avoidance active (except goalkeeper)

**Duration**: 45 seconds (kickOffSetupDuration parameter)

**Legal Positions**:
- **Kick-off team**: Own half + center circle (one robot allowed in circle)
- **Defending team**: Own half excluding center circle
- **Both teams**: Can be in own goal, not on border strip

### 5. SET State (Wait Phase)
**Purpose**: Robots stop and remain stationary until kick-off

**Implementation**:
- `GameState::waitForOwnKickOff` or `GameState::waitForOpponentKickOff`
- Corresponds to GameController STATE_SET (GameStateProvider.cpp:865-873)

**Transition**: GameController sends STATE_SET

**Robot Behavior**:
- Stops all movement (HandleGameState.h:59-76)
- Stands high
- Looks at center mark (kick-off) or penalty mark (penalty kick)
- **CRITICAL**: No movement allowed - illegal motion results in penalty

**Rules Enforced**:
- Robots reaching illegal positions are penalized (PENALTY_SPL_ILLEGAL_POSITION)
- Robots moving during SET are penalized (PENALTY_SPL_ILLEGAL_MOTION_IN_SET)
- Ball moved by robot is placed back by referee

### 6. PLAYING State (Kick-off Phase)
**Purpose**: Execute kick-off and transition to regular play

**Implementation - Initial Transition (SET → PLAYING)**:

#### Method A: Whistle Detection
- **Code**: GameStateProvider.cpp:299-336
- **Process**:
  1. Team robots listen for whistle
  2. Majority vote from robots that heard whistle
  3. Average confidence must exceed threshold (minWhistleAverageConfidence)
  4. Minimum number of voters required (minVotersForWhistle)
- **Function**: `checkForWhistle()` (GameStateProvider.cpp:563-600)

#### Method B: GameController Signal
- **Trigger**: GameController sends STATE_PLAYING
- **Code**: GameStateProvider.cpp:875-900
- **Delay**: 15 seconds after referee whistle (playingSignalDelay = 15000ms)
- **Note**: GameController may delay this transition

**Kick-off Phase** (First 15 seconds):
- **States**: `GameState::ownKickOff` or `GameState::opponentKickOff`
- **Duration**: 15 seconds (kickOffDuration parameter)
- **Special Rules**:
  - Kick-off team can be in center circle
  - Defending team must stay out of center circle
  - Ball must be kicked to be "in play"

**Transition to Regular Play**:
- **Trigger 1**: Ball moves (detected by checkBallHasMoved)
- **Trigger 2**: 15 seconds elapse
- **Code**: GameStateProvider.cpp:243-251
- **Result**: State becomes `GameState::playing`

## Key Implementation Components

### GameStateProvider Module
**Location**: `Src/Modules/Infrastructure/GameStateProvider/`

**Responsibilities**:
- Receives GameController data (UDP packets on port 3838)
- Converts GameController states to internal game states
- Manages state transitions
- Handles whistle detection for transitions
- Tracks ball movement for kick-off completion
- Manages penalty states

**Key Functions**:
- `convertGameControllerDataToState()`: Maps GC states to internal states
- `convertPenaltyToPlayerState()`: Maps GC penalties to player states
- `checkForWhistle()`: Whistle detection via team consensus
- `checkBallHasMoved()`: Ball movement detection for kick-off

### InitialToReadyHandler Module
**Location**: `Src/Modules/BehaviorControl/InitialToReadyHandler/`

**Responsibilities**:
- Detects referee "ready" gesture visually
- Coordinates pawn sacrifice to avoid penalties
- Shares detection with team
- Triggers optimistic STANDBY → READY transition

**States**:
- `observing`: Looking for referee gesture
- `waiting`: Waiting for pawn to be penalized
- `transition`: Ready to transition to READY state

**Pawn Sacrifice Mechanism**:
1. Gesture detected by one or more robots
2. Robot closest to referee (with referee in sight) becomes "pawn"
3. Pawn attempts transition first (likely to be penalized for illegal motion)
4. If pawn penalized, next robot tries
5. If pawn not penalized after timeout, all robots transition
6. Maximum 2 pawns sacrificed before giving up

### HandleGameState Option
**Location**: `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleGameState.h`

**Responsibilities**:
- Defines robot behavior for each game state
- Manages skill execution based on state

**States and Behaviors**:
- `initial`: Stand high, look straight
- `ready`: Walk to position or stand
- `set`: Stand high, look at ball position
- `playing`: Execute strategy
- `finished`: Stand, look forward

### GameController Communication
**Location**: `Src/Modules/Communication/GameControllerDataProvider/`

**Protocol**:
- **Receive**: UDP port 3838 (game state packets)
- **Send**: UDP port 3939 (robot status packets)
- **Frequency**: Robot sends status every 500ms when GC active
- **Timeout**: GameController considered inactive after 2000ms

**Status Data Sent**:
- Player number and team number
- Fallen state
- Robot pose (x, y, theta)
- Ball age and position

## Configuration Parameters

**GameStateProvider Parameters** (loaded from config):
- `kickOffSetupDuration`: 45000ms (duration of READY phase)
- `kickOffDuration`: 15000ms (duration of kick-off restrictions)
- `playingSignalDelay`: 15000ms (GC delay for PLAYING signal)
- `gameControllerTimeout`: 2000ms (time before GC considered inactive)
- `minVotersForWhistle`: 2 (minimum robots for whistle consensus)
- `maxWhistleTimeDifference`: 3000ms (time window for whistle detections)
- `minWhistleAverageConfidence`: 0.5 (minimum confidence for whistle)

**InitialToReadyHandler Parameters**:
- `waitForPawnSacrifice`: 10000ms (time to wait for pawn penalty)
- `pawnsLeftToSacrifice`: 2 (maximum pawns to sacrifice)
- `thresholdForIndependentDetections`: 3000ms (time between detections)

## State Override Mechanism

The system implements an "optimistic state transition" mechanism that allows early transitions based on whistle/gesture detection, with GameController providing confirmation:

**How it works**:
1. **Optimistic Transition**: Robot detects whistle/gesture and changes state
2. **Set Override Flag**: `gameStateOverridden = true`
3. **Wait for Confirmation**: GameController sends matching state
4. **Validation**:
   - If GC state matches: Clear override flag, transition confirmed
   - If GC state differs: Revert to GC state
   - If timeout: Revert based on heuristics

**Benefits**:
- Faster transitions (don't wait for GC delay)
- Reduced reaction time in gameplay
- Fallback to authoritative GC state

**Code**: GameStateProvider.cpp:176-241, 370-374

## Testing the Implementation

### Manual Testing Steps

1. **Test UNSTIFF → ACTIVE**:
   - Start robot in unstiff state
   - Press chest button
   - Verify robot transitions to active and processes GC messages

2. **Test INITIAL → STANDBY**:
   - Set GameController to STATE_INITIAL
   - Verify robot in beforeHalf state, standing high
   - Set GameController to STATE_STANDBY
   - Verify robot in standby state

3. **Test STANDBY → READY (GameController)**:
   - Robot in standby, active state
   - Set GameController to STATE_READY
   - Verify robot transitions to setupOwnKickOff or setupOpponentKickOff
   - Verify robot walks to position

4. **Test READY → SET**:
   - Robot in READY state
   - Set GameController to STATE_SET
   - Verify robot transitions to waitForOwnKickOff or waitForOpponentKickOff
   - Verify robot stops moving

5. **Test SET → PLAYING**:
   - Robot in SET state
   - Set GameController to STATE_PLAYING
   - Verify robot transitions to ownKickOff or opponentKickOff
   - Verify 15-second restriction period

6. **Test KICK-OFF → PLAYING**:
   - Robot in kick-off state
   - Wait 15 seconds OR move ball
   - Verify robot transitions to playing state

### Debug Commands

Enable debug logging:
```
# In GameStateProvider, logging is already active
# Check console output for state transitions
```

View current state:
```
# Use SimRobot or bush to query:
# - theGameState.state
# - theGameState.playerState
# - theGameControllerData.state
```

## Troubleshooting

### Issue: Robot doesn't transition from UNSTIFF
**Cause**: Robot not receiving chest button press or GC message
**Solution**: 
- Verify chest button works (check EnhancedKeyStates)
- Verify GameController sends PENALTY_NONE

### Issue: Robot doesn't process GameController messages
**Cause**: Robot still in unstiff or calibration state
**Solution**: Transition to active state first (chest button)

### Issue: Gesture detection not working
**Cause**: Referee not in robot's field of view
**Solution**: 
- Verify robot position on touchline
- Check RefereePercept module is active
- Verify referee is in camera view

### Issue: Robot moves during SET
**Cause**: Behavior not properly handling SET state
**Solution**:
- Verify HandleGameState set state uses StandSkill
- Check for motion requests in playing state

## References

- **SPL Rules**: RoboCup Standard Platform League Rules (this implementation follows the 2023 rules as used by B-Human Code Release 2023)
- **B-Human Framework**: Based on B-Human Code Release 2023 (https://github.com/bhuman/BHumanCodeRelease or https://wiki.b-human.de/)
- **GameController Protocol**: See RoboCupGameControlData.h for packet structure (follows GameController v18)

## Summary

The game flow implementation is **complete and functional**. All required transitions are implemented:

✅ UNSTIFF → ACTIVE (manual or GameController)
✅ INITIAL → STANDBY (GameController)
✅ STANDBY → READY (gesture detection + GameController)
✅ READY → SET (GameController)
✅ SET → PLAYING (whistle + GameController)
✅ KICK-OFF → PLAYING (ball movement + timeout)

The implementation handles both optimistic transitions (whistle/gesture) and authoritative GameController signals, with proper fallback mechanisms. All SPL rules for positioning, movement restrictions, and penalties are enforced.
