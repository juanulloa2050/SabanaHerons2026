# Migrating B-Human 2023 to the HSL 2026 Rules

The B-Human 2023 code release targets the Standard Platform League (SPL
rules 2023/2024 and GameController protocol v18). Sabana Herons plays the
Humanoid Soccer League (HSL) 2026 with NAO robots, so the code base was
migrated to the HSL GameController protocol (v20) and to the HSL 2026 rules
(Small field, Foundation 4v4). This document lists what was changed; it is the
code we played with at RoboCup 2026. It may be useful to other teams that
start from a B-Human release for the HSL.

References used: SPL rules 2024, HSL rules 2026 (draft of 2026-05-26), the
SPL GameController (protocol v18) and the HSL GameController (protocol v20).

## GameController protocol v20

Main files: [RoboCupGameControlData.h](../Src/Libs/Network/RoboCupGameControlData.h),
[GameStateProvider](../Src/Modules/Infrastructure/GameStateProvider),
[GameState.h](../Src/Representations/Infrastructure/GameState.h),
[SimulatedNao/GameController.cpp](../Src/Libs/SimulatedNao/GameController.cpp).

- `RoboCupGameControlData.h` updated to protocol version 20.
- The removed `competitionPhase` field is replaced by `stopped`, which maps to
  a new internal `GameState::stopped` (Stop Play).
- `RobotInfo::cautions` added and used in behavior/debug output.
- HSL competition types (Small, Middle, Large) and phases (normal, penalty
  shoot-out, extra time, timeout).
- SPL set plays replaced by the HSL ones: direct free kick, indirect free
  kick, throw-in, goal kick, corner kick, penalty kick.
- SPL penalty constants replaced by the HSL ones and mapped to
  `GameState::PlayerState`. `PENALTY_SENT_OFF` is a permanent removal (the
  robot no longer takes a setup-pose slot nor counts as an active opponent),
  and `PENALTY_MOTION_IN_STOP` is distinct from `PENALTY_MOTION_IN_SET`.
- The SPL-era names used internally by other modules are kept as source-level
  aliases; the wire format is v20 only (no v18 compatibility).
- The simulated GameController exposes the HSL commands directly.

## HSL 2026 rules

### Timing and restarts

- Free kicks last 45 s; the delayed playing signal is 10 s.
- After 45 s a free kick locally turns into `playing` ("ball free"), even if
  the GameController or operator lags.
- Direct and indirect free kicks are handled separately; direct goals are
  blocked only on our own indirect free kicks (not on our corner kicks or
  direct free kicks).
- Throw-in is distinguished from kick-in where behavior needs it.
- Own and opponent strategies for direct/indirect free kicks, throw-ins, and
  opponent goal kicks. On opponent goal kicks, our robots stay outside the
  whole penalty area.

### Dropped ball

- Dropped ball (READY/SET with `kickingTeam == KICKING_TEAM_NONE`) is handled
  as "no set play, no kicking team".
- Positioning is enforced through illegal areas: own half, outside the center
  circle during SET. A READY-specific path lets 3v3/4v4 tactics reuse normal
  positions.
- The simulator and referee scripts use dropped ball for global game stuck.

### Kick-off

- No direct goals from kick-off. With three or more robots the two-touch rule
  is enforced; with two or fewer, the kicker must touch the ball again outside
  the center circle before it may score.
- The restriction survives the transition from `ownKickOff` to `playing`,
  using the timestamps of our own kicks shared through team communication.
- The designated kicker is respected when one robot is inside the center
  circle in its own half.
- In our final 4v4 setup the kick-off is a short lateral pass.

### Penalty kicks

- Placement: keeper on the goal line, one striker in the opponent penalty
  area, everybody else outside the penalty area and one center-circle radius
  away from the penalty mark.
- The kicker never touches the ball a second time after it clearly moved; the
  keeper stays on its feet until the kick is taken.

### Field and returning robots

- `Default`, `4v4_Full`, and `3v3_Full` locations use the HSL 2026 Small field
  dimensions.
- A robot returning from a penalty is placed on the touchline at the height of
  its own penalty mark. `SelfLocator` seeds its particles there; a side hint
  remembered before the penalty resolves the left/right symmetry when it is
  confident, otherwise both sides are kept as hypotheses.

### Ball search for restarts

Restart placements are often not visible from where a robot stands.
`RestartBallSearchProvider` creates rule-based candidates for where the ball
should be placed, ranked by remembered local/team ball information, and
`SearchRestartBall` and `BallSearchAreasProvider` integrate them into the team
search. These hypotheses are kept separate from real ball sightings.
