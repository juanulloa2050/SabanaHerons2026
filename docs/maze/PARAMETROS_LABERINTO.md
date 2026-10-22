# Maze Challenge Parameters

Single reference of every tunable parameter of the Maze challenge: what it does, its default, where it lives, whether it exists today or is only planned in a ticket, and whether its value waits on an unanswered organiser question. Values change by editing the `.cfg` file and redeploying, unless the section says otherwise. `Maze` and `MazeSafe` hold identical copies of every file below, except where a section states a difference.

Related: [TICKETS_LABERINTO.md](TICKETS_LABERINTO.md) (owning tickets) · [ROADMAP_LABERINTO.md](ROADMAP_LABERINTO.md) (architecture, section 11 for the questions) · [SIMULACION_LABERINTO.md](SIMULACION_LABERINTO.md) (changing values live in SimRobot).

Conventions: **exists** = present in code or config today. **planned in MZ-0xx** = named in a ticket, not implemented yet. `not set yet (MZ-0xx)` = the ticket names the parameter but gives no value.

## Open organiser questions

Only questions that affect a parameter. All are still pending (no official answer is recorded in the roadmap). Until answered, the defaults below are team assumptions. Q2 and Q5 are resolved, Q4 is withdrawn.

| Id | Question | Parameters affected |
|---|---|---|
| Q1 | Which walls carry tags (every face of every cell? only some?) and at what height is the tag centre? | `mode`, `faces`, `walls`, `centreHeight` in `mazeTagLayout.cfg` (and through it the tag-fix rate of the localizer) |
| Q3 | Does the ≤2 min return to start (after attempt 1) count inside the 5 min of the attempt? Example: attempt 1 takes 4 min to reach the goal. Does the robot still get the full 2 min to return, or only the remaining 1 min? | `returnToStart.maxDurationMs`, `returnToStart.enabled` |
| Q6 | Is the robot placed facing the open side of the start cell? | `start.cell`, `start.heading` in `handleMaze.cfg` (the deduction assumes it does) |
| Q9 | Must the robot detect the goal by itself and start the return, or may a team member trigger the return with a robot button once the judge stops the clock? | `returnToStart.trigger`, `returnToStart.enabled`, `returnToStart.delayMs` |
| Q11 | Does the clock start when the robot leaves the first cell, or when it starts to move? | `startScan.enabled` (and the other `startScan.*` values) |
| Q13 | What colour and material is the maze floor (outside the goal block)? The rules say light walls and a matte uniform floor, but not the floor colour. If the floor is also light, the wall base is hard to see. | Possible colour threshold of the wall perceptor (not a parameter yet, MZ-019); simulation colours `WALL_COLOR`, `FLOOR_COLOR`, `GOAL_COLOR` |

## 1. Tag layout

File: `Config/Scenarios/Maze/mazeTagLayout.cfg` (and `MazeSafe`). Ticket: MZ-013. Status: **exists**. Read by the oracle, the detector (tag size), the localizer and mapper (MZ-020) and `Util/MazeTools/maze_to_scene.py`. After a change, regenerate the scene (`maze_to_scene.py`) so the drawn tags match.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `mode` | `allFaces`, `faces` (only the faces in `faces`) or `walls` (only the listed cell/face pairs). | `allFaces` | Q1 |
| `faces` | Subset of `north`, `east`, `south`, `west` (maze frame), used when `mode = faces`. | `[north, east, south, west]` | Q1 |
| `walls` | List of `{cell; face;}` pairs, used when `mode = walls`. | `[]` | Q1 |
| `centreHeight` | Height of the tag centre above the floor, in mm. | `300` | Q1 |
| `size` | Edge of the black outer square, in mm (fixed by the rules). | `150` | — |

## 2. AprilTag detector

File: `Config/Scenarios/Maze/aprilTagDetector.cfg` (and `MazeSafe`, same values). Ticket: MZ-018 (merged), values tuned on the robot in MZ-018.1. Status: **exists** (untuned defaults). Can be changed live: `for Upper get parameters:AprilTagDetector`, edit, press Enter; copy the final values into both `.cfg` files.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `minDecisionMargin` | Detections with a smaller decoding margin are dropped. | `30` | — |
| `maxHamming` | Largest number of corrected bits accepted; 0 accepts only exact decodes. | `0` | — |
| `sizeTolerance` | Largest relative difference between the pose distance and the distance implied by the tag's apparent size. | `0.5` | — |
| `frameSkip` | Run on every n-th upper image only (1 = every image); raise it if the stopwatch exceeds 20 ms. | `1` | — |
| `maxDistance` | Tags farther than this are dropped, in mm. | `3000` | — |
| `maxId` | Largest accepted tag id (ids are `row * 8 + column`); larger ids are false positives. | `63` | — |
| `quadDecimate` | Downscale factor of the quad detection; faster, but far tags are lost. | `2` | — |

## 3. Oracle percepts (simulation only)

File: `Config/Scenarios/Maze/oracledMazePerceptsProvider.cfg` (and `MazeSafe`). Ticket: MZ-013. Status: **exists**. Fake perception from ground truth; not used on the robot. Live change: `get parameters:OracledMazePerceptsProvider`, edit, run the printed `set` line.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `mazeFile` | Maze file the oracle reads (written next to the scene by `maze_to_scene.py`). | `"Config/Scenes/Includes/Maze/current.maze"` | — |
| `mazePoseOnField` | Pose of the maze frame in the ground-truth frame (180deg rotation, 2000 mm offsets for the maze robot). | `{rotation = 180deg; translation = {x = 2000; y = 2000;};}` | — |
| `maxRange` | Cells ahead of the robot's cell that are seen. | `2` | — |
| `dropoutRate` | Probability of missing one observation; 1 gives empty percepts (used by the safe-mode tests). | `0` | — |
| `tagPositionStdDev` | Gaussian noise of the tag position per axis, in mm. | `5` | — |
| `tagRotationStdDev` | Gaussian noise of the tag rotation per axis. | `1deg` | — |
| `wallDistanceStdDev` | Gaussian noise of wall and cell distances, in mm. | `10` | — |
| `maxTagViewAngle` | Largest angle between the tag normal and the direction to the camera. | `70deg` | — |
| `maxTagDistance` | Farthest tag, in mm from the camera, that is seen. Independent of `maxRange` (walls only). Before it was `maxRange * 500 + 250` = 1250; the real detector allows 3000 (`aprilTagDetector.cfg`). | `2000` | — |

Compile-time constant (not in the `.cfg`): `reloadIntervalMs` = 1000 ms, time between two looks at the maze file (`OracledMazePerceptsProvider.h`).

## 4. Walking profiles

Files: `Config/Scenarios/Maze/walkingEngine.cfg`, `walkingEngineCommon.cfg` and the same two in `MazeSafe`. Ticket: MZ-016 (values re-tuned from lab data in MZ-028). Status: **exists**. Only the values below differ from `Config/Robots/Default/`. `Maze` = 60 % of the default speed, `MazeSafe` = 40 %.

| Parameter | Meaning | Default (`Maze` / `MazeSafe`; robot default) | Depends on question |
|---|---|---|---|
| `maxSpeed.rotation` | Maximum turning speed. | `72deg` / `48deg` (120deg) | — |
| `maxSpeed.translation` | Maximum walking speed x, y, in mm/s. | `x=150 y=120` / `x=100 y=80` (250, 200) | — |
| `minSpeed.rotation` | Minimum turning speed. | `60deg` / `40deg` (100deg) | — |
| `minSpeed.translation` | Minimum walking speed x, y, in mm/s. | `x=132 y=108` / `x=88 y=72` (220, 180) | — |
| `armParameters.armShoulderRoll` | Arm abduction while walking; smaller keeps the arms off the walls. | `4deg` in both (7deg) | — |
| `armParameters.armShoulderRollIncreaseFactor` | Factor that increases the shoulder roll with walking speed. | `1` in both (2) | — |
| `armParameters.armShoulderPitchFactor` | Arm swing factor. | `3` in both (6) | — |

MZ-028 may lower the `Maze` `maxSpeed` x by 10 % if Lab 3 shows wall contacts or clearance below 5 cm (planned, only that value). Other walking parameters (`kinematicParameters`, `sideStabilizeParameters`, `balanceParameters`) are the robot defaults, listed in roadmap 3.6.

## 5. Wall perceptor and corridor look-ahead

File: `Config/Scenarios/Maze/wallPerceptor.cfg` (and `MazeSafe`, same values). Module `Src/Modules/Perception/WallPerceptor/` (Upper), cores in `Src/Tools/Maze/MazeWallScan.h` and `MazeWallView.h`. Ticket: MZ-019. Status: **exists** (set from the simulation evaluation of MZ-019, see `SIMULACION_LABERINTO.md` step 7; not yet checked on real images, Q13 and Lab 1 logs pending). Can be changed live: `get parameters:WallPerceptor`, edit, run the printed `set` line; copy the final values into both `.cfg` files.

### Look-ahead and grid snap

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `lookAheadRangeWalking` | Cells ahead of the robot's cell whose side walls are reported while the robot walks (the torso pitches). Walking distance error (table below) is 35 / 90 mm (mean / p95) at 1.5 m but 85-128 / 157-918 mm at 2.0 m, beyond `snapToleranceMm`, so only the next cell is reported. | `1` | — |
| `lookAheadRangeStanding` | Cells ahead reported while the robot stands and scans. Standing p95 stays at or under 24 mm up to 2.5 m (the farthest bin measured). | `3` | — |
| `snapToleranceMm` | A boundary point is kept only if it lies within this distance of a 500 mm grid line. 150 covers the standing error (24 mm p95 up to 2.5 m) and the walking error (95 mm p95 up to 1.5 m); a point 250 mm from a line cannot be told from its neighbour anyway. | `150` | — |
| `minSupportPoints` | Points needed on an edge for a wall. | `3` | — |
| `openClearanceMm` | An edge is open if the sight line crosses it and the floor is visible at least this far beyond it, in mm. | `150` | — |
| `vertexMarginMm` | Points closer than this to a grid vertex are ignored, in mm; 50 mm of noise there credited a neighbour edge. | `60` | — |
| `noGoalBlockWalls` | By the rules the four cells of the goal block have no walls between them: those edges are never reported as walls. | `true` | — |

The scan of MZ-047 relies on `lookAheadRangeStanding`: the robot stands at a cell centre with the head at ±30° or 0°, the gaze snaps to the robot heading, and the near side walls of the next cell are reported (see section 8, `MazeScanWalls`).

The percept carries the range it used (`WallPercept.lookAheadRange`), because the Upper and Cognition copies of `MotionInfo` can differ for a few frames around a start or a stop; the evaluator checks the reported cells against that range.

### Boundary scan

The boundary is the brightness step between floor and wall in the grey `ECImage`, in either direction: in the simulator the walls render **darker** than the floor (grey 106 against 254), by the rules they are lighter, and the real polarity is unknown (Q13). Q13 does not change these values; a colour threshold may be added only after Q13 is answered and shows a clear contrast.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `columnStep` | Distance between the scanned image columns, in pixels. | `8` | — |
| `smoothRows` | Rows averaged on each side of a candidate boundary. | `3` | — |
| `columnHalfWidth` | Columns on each side of the scanned one that are averaged. | `1` | — |
| `minGradient` | Least brightness step of the floor-to-wall boundary, in grey levels (the minimum wall/floor contrast). | `12` | Q13 |
| `maxFloorDeviation` | The bottom rows of a column and the rows below a boundary must be at most this far from the floor reference, in grey levels. A column with wall at its bottom has no boundary (the edges of an AprilTag are no boundary). | `30` | — |
| `minWallContrast` | Least brightness of the wall above the floor reference, in grey levels; rejects the far edge of the goal patch. | `8` | Q13 |
| `minWallCheckHeightMm` | Height up to which a column has to stay wall, in mm: near the camera height, below the wall top (600 mm). | `550` | — |
| `minWallRowFraction` | Part of the rows up to that height that must not look like floor (a tag counts as wall). | `0.75` | — |
| `maxFloorRunRows` | Longest run of floor-like rows allowed; floor behind a goal patch or a marking shows as such a run. | `4` | — |
| `minCheckRows` | Rows that have to remain for the check when the check height is above the top of the image. | `3` | — |
| `floorReferenceRows` | Bottom image rows that give the floor reference. | `8` | — |
| `maxWallSaturation` | The goal is marked by a floor colour, walls and floor have none: a step whose region above is more saturated than the floor (levels of the `ECImage` saturated channel) by more than this is a floor marking, not a wall. 0 switches the veto off. Measured in simulation: goal patch 51, wall 0, floor 0; camera noise on a real grey is about 10. | `25` | Q13 |
| `maxScanDistanceMm` | Floor farther than this from the camera is not searched, in mm; sets the horizon row. | `3500` | — |

### Learned floor

A column without a boundary is free floor (evidence for "open" up to the horizon) only if its bottom rows match the floor brightness learned from earlier frames; otherwise a near wall fills the column. A step that was seen but not accepted gives no clear evidence either.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `floorTolerance` | A column without a boundary is free floor only if its bottom rows are this close to the learned floor brightness, in grey levels. | `8` | — |
| `floorTrackingRate` | Part of the difference to a new floor reference that the learned floor follows per image with enough boundaries (at least half of the columns). | `0.1` | — |
| `floorMaxAgeMs` | The learned floor is forgotten after this time without such an image, in ms. It is also reset when the robot becomes active (`PlayingTracker::resetPose`) and when the clock goes back. | `10000` | — |

### Measured distance error

Source of `lookAheadRangeWalking`, `lookAheadRangeStanding` and `snapToleranceMm`: rendered images in simulation (`MazeEval`, mazes 1, 19 and 7, one 420 s run each, ground-truth pose; the T3.1 run at HEAD `e2ebdbce`). Distance of each boundary point to the nearest true wall line, mean / p95 in mm, binned by distance from the robot (bin ±250 mm). The projection did not change between the first and the second matrix, so the values hold for the current code.

| Distance | Standing (mean / p95) | Walking (mean / p95) |
|---|---|---|
| 0.5 m | 3-13 / 3-7 | 14-18 / 22-69 |
| 1.0 m | 3-4 / 5 | 17-22 / 42-53 |
| 1.5 m | 2-8 / 6-20 (maze_7: 27 / 52) | 32-36 / 70-94 |
| 2.0 m | 6 / 15 (maze_7: 42 / 70, 36 samples) | 85-128 / 157-918 |
| 2.5 m | 13 / 24 (maze_19 only) | 64-96 / 179-203 |

Ranges are over the three mazes; the same-colour scene gives the same order (walking 1.5 m: 37 / 101). Standing 2.0 m and 2.5 m have few or no samples in two of the mazes. These are rendered-image values: Lab 1 logs (MZ-017) do not exist, so nothing here is measured on the real camera.

### Interplay with the mapper

The wall percept feeds `MazeMapper` (section 6) through `cellsAhead`; the mapper's `minObservationInterval`, distance weights and `maxOffsetFromCentre` apply to it as to any wall observation. Tags win over the wall percept: once an edge has a seen tag (a wall by definition), open observations from the wall percept for that edge are ignored. The flag is per edge and never clears while the fusion lives, so a misdecoded tag blocks open evidence for that edge for good. The observation of the cell the robot stands in (from its position) still counts.

Known limits: while the robot stands on the goal patch every frame is rejected (the floor reference is the coloured patch), so the goal cells give no evidence from inside; the saturation veto and `noGoalBlockWalls` handle the patch seen from outside.

## 6. Localizer and mapper

Files: `Config/Scenarios/Maze/mazeLocalizer.cfg` and `mazeMapper.cfg` (and `MazeSafe`, identical). Modules `MazeLocalizer`, `MazeMapper` (Cognition). Ticket: MZ-020. Status: **exists** (offline-tested defaults; SimRobot run and the real gyro drift pending). The start cell and heading are no longer here: both modules read `start` from `handleMaze.cfg` (section 8) through `MazeStart`. Background: [GYRO_HEADING.md](GYRO_HEADING.md).

### `mazeLocalizer.cfg`

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `useGyro` | `true`: heading from the gyro yaw, re-aligned by square-on tags; `false`: odometry rotation. | `true` | — |
| `maxFaceAngleError` | Largest angle between the measured and the expected tag normal; a tag beyond it is rejected (false-positive filter). Keep equal to the mapper's. | `45deg` | Q1 (through the layout) |
| `translationGain` | Share of the position error that a tag fix corrects at zero distance. | `0.5` | — |
| `farDistance` | Tag distance at which the gain is scaled by `farGainScale`, in mm. | `2000` | — |
| `farGainScale` | Gain factor at `farDistance`. | `0.4` | — |
| `rotationGain` | Share of the heading error that a square-on fix corrects. | `0.5` | — |
| `squareOnAngle` | Largest angle between the camera ray and the tag normal for the heading correction. | `25deg` | — |
| `superbMaxTime` / `superbMaxDistance` | `RobotPose.quality` is superb up to this time (ms) and distance (mm) since the last tag fix. | `10000` / `1500` | — |
| `okayMaxTime` / `okayMaxDistance` | Same for okay; beyond it the quality is poor (MZ-025 reads it). | `30000` / `4000` | — |
| `injectedGyroDriftPerMinute` | **Simulation only.** Degrees per minute added to the gyro yaw. Leave at 0 on the robot. | `0` | — |
| `injectedOdometryTranslationNoise` | **Simulation only.** Std. dev. of the relative error of each odometry translation delta (0.05 = 5 %). | `0` | — |
| `injectedOdometryRotationNoise` | **Simulation only.** Std. dev. of the rotation error per metre walked; only matters with `useGyro = false`. | `0deg` | — |

The localizer reads `mazeTagLayout.cfg` (section 1) to know which faces carry tags, so a different layout is a config change only.

### `mazeMapper.cfg`

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `threshold` | Consistent observations of full weight before an edge leaves `unknown`; flipping a decided edge needs the same amount of contrary evidence (hysteresis). | `3` | — |
| `fullWeightDistance` | Wall observations up to this distance have full weight, in mm. | `500` | — |
| `zeroWeightDistance` | Wall observations from this distance on have no weight (linear in between), in mm. | `2500` | — |
| `tagWeight` | Weight of a decoded tag, which counts as a wall at any distance. | `1` | Q1 (through the layout) |
| `minObservationInterval` | An edge accepts at most one observation (wall percept or tag) per this many ms. Cognition repeats the scene at about 60 Hz, which is not independent evidence; without this limit 3 frames (50 ms) decided or flipped an edge. With `3` observations and 250 ms an edge needs at least 0.5 s of consistent evidence. | `250` | — |
| `maxOffsetFromCentre` | The wall look-ahead is used only while the estimated position is within this distance (mm, per axis) of the centre of its estimated cell. The oracle (and later the camera) describes the cell the robot is really in; near a boundary the estimated cell may be the neighbour and the percept would be written on the wrong cell. Tags are not gated (the id names the cell). A cell is 500 mm, so `150` skips a band of 200 mm around every boundary. | `150` | — |
| `maxFaceAngleError` | Same filter as the localizer's; keep both equal. | `45deg` | Q1 (through the layout) |
| `worstAcceptedQuality` | The map only learns while the localizer quality is at least this good (`superb`, `okay`, `poor`); `poor` accepts everything. | `poor` | — |

A decided edge flips only after the same evidence again, now spread over at least 0.5 s. An estimate that stays in the wrong cell (a robot dragged in the scene with no tag in view) is not protected by either key: it is corrected by the first tag fix, and the map there stays wrong until the localizer recovers.

The map learns only while the robot is active (game state `playing` and `FallDownState` not `pickedUp`), and survives pick-up and Standby. The attempt counter increments, and the localizer puts the pose at the start (MazeLocalizer logs "pose reset to the start" with the reason), on each transition into active. A fall is not a pick-up and changes neither.

## 7. Planner costs

File: `Config/Scenarios/Maze/mazePlanner.cfg` (and `MazeSafe`). Module `MazePlannerProvider` (Cognition). Ticket: MZ-024 creates the file; MZ-028 replaces the values with the Lab 2 means. Status: **exists**. The keys are the fields of `MazeCosts` in `Src/Tools/Maze/MazePlanner.h`; the tickets call them `t_fwd`, `t_start`, `t_turn90`, `t_turn180`. The Python solver and `expected.csv` use the same defaults and must be regenerated if they change.

| Key | Ticket name | Meaning | Default | Depends on question |
|---|---|---|---|---|
| `forwardMs` | `t_fwd` | Time for one cell forward while already walking. | `2500` ms | — |
| `startMs` | `t_start` | Extra time for the first step after standing (after a turn or at the start). | `800` ms | — |
| `turn90Ms` | `t_turn90` | Time for a 90° turn in place. | `2000` ms | — |
| `turn180Ms` | `t_turn180` | Time for a 180° turn in place. | `3500` ms | — |

Rules:

- `forwardMs`, `turn90Ms` and `turn180Ms` must be greater than 0 and `startMs` not negative. Otherwise the module prints an error and uses the defaults of **all four** keys.
- It can be changed live: `get parameters:MazePlannerProvider`, edit, run the printed `set` line. The plan is recomputed on the next frame (the costs are part of the cache key), and an invalid value falls back to the defaults the same way.
- Both routes are always computed (`MazePlan.exploration`, `MazePlan.speedRun`), so there is no mode key.

Backlog (MZ-043, defaults stated in the ticket): `rollingTurns.enabled` = `false` and `diagonals.enabled` = `false` in `mazePlanner.cfg`; the rolling-turn and diagonal action costs are not set yet (MZ-043).

## 8. HandleMaze behaviour

File: `Config/Scenarios/Maze/handleMaze.cfg` (and `MazeSafe`, identical). Ticket: MZ-024. Status: **exists**. `MazeMapper` reads `start`, `SkillBehaviorControl` reads `announce`; each ignores the entries it does not know. The localizer follows the start through `MazeStart`.

| Key | Meaning | Default | Depends on question |
|---|---|---|---|
| `start.cell` | Start corner: `0`, `7`, `56` or `63` when known (deduction skipped), `-1` to deduce it from tags and walls. Any other value is an error at load and is treated as `-1`. | `-1` | Q6 |
| `start.heading` | Maze heading the robot faces at the start: `north`, `east`, `south` or `west`. A corner has two open sides, so it is used when `start.cell` is given and ignored when it is `-1`. It must point into the maze: cell 0 `east`/`south`, 7 `south`/`west`, 56 `north`/`east`, 63 `north`/`west`. Otherwise: error at load, `start.cell` treated as `-1`. | `south` | Q6 |
| `announce.enabled` | Speak every state change and button gesture; `false` silences all speech (the behaviour does not change). | `true` | — |

A tag or wall that contradicts a given start is reported with a warning that names the kind of observation (`tag`, `boundary edge`, `position`); the given start does not change. The warning is limited to one per 2 s.

### Blocked walk (`blockedWalk`)

Ticket: MZ-046. Same files (`Maze` and `MazeSafe`). A forward walk into a wall the map did not know is detected, the robot steps back to the centre of the cell the walk started in, and the edge becomes a wall that no wall percept can open again. All values were measured in SimRobot (maze_19, `MazeVisionEval`); **recalibrate `gyroStdThreshold` and `bumperHoldMs` on the robot**: the real gait and panel give other levels, and the foot bumpers do not exist in SimRobot.

| Key | Meaning | Default | Source |
|---|---|---|---|
| `blockedWalk.gyroWindowMs` | Window of the standard deviation of the yaw rate (`InertialData.gyro.z`), ms. | `250` | Walking free: 0.06 rad/s (p99 ≤ 0.19). Blocked: ≥ 0.96, mean 1.74. |
| `blockedWalk.gyroStdThreshold` | Deviation (rad/s) above which a commanded forward walk looks blocked. | `0.5` | Between the free p99 and the blocked minimum. |
| `blockedWalk.minWalkSpeed` | Commanded forward speed (mm/s) from which a walk can be blocked; a standing or turning robot never triggers. | `100` | Free walk 150 mm/s; the commanded speed while turning is 30–50. |
| `blockedWalk.blockedTimeoutMs` | How long the gyroscope alarm must hold without a gap, ms. Detection takes about this + 0.1–0.3 s after the contact. | `1000` | The start and stop jolts of a walk last up to ~120 ms. |
| `blockedWalk.walkGapMs` | How long the commanded speed may stay under `minWalkSpeed` without dropping the alarm, ms. | `400` | The speed dips to 80–90 mm/s for up to ~300 ms, also while pushing. |
| `blockedWalk.bumperHoldMs` | How long a foot bumper contact during a forward walk must hold, ms (robot only). | `100` | Guess; to be measured in the lab (MZ-046 T6). |

Odometry and `RobotPose` follow the commanded walk, so they cannot detect the block, and after the detection the pose is 130–190 mm ahead of the true position (the step back goes to the origin cell of the walk, not to the pose cell).

### Compile-time constants of the behaviour

In `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` and `Src/Tools/Maze/MazeDrive.h`. Status: **exists**; changing them needs a rebuild, not a config edit.

| Constant | Meaning | Value |
|---|---|---|
| `mazeCellSize` | Edge of one maze cell, in mm. | `500` |
| `motionTestCells` | Cells walked forward in the first step of the motion test. | `4` |
| `motionTestPauseTime` | Pause (with a beep) between motion-test steps, in ms. | `2000` |
| `Maze::announcementTimeMs` (`mazeAnnouncementTime`) | Time one phrase is said, and the time after an announcement before the next tap is accepted, in ms. | `1500` |
| `Maze::maxQueuedPhrases` | Phrases that may wait to be said; when full, the oldest waiting one is dropped. | `8` |
| `tagMemoryTime` | A tag last seen longer ago than this is announced as "no tag", in ms. | `2000` |
| `Maze::walkTimeoutMs` | A walk to a neighbouring cell that takes longer is dropped and the plan is read again, in ms. | `20000` |
| `Maze::turnTimeoutMs` | Same for a turn, in ms. | `12000` |
| `Maze::scanTimeoutMs` | Same for a wall scan, in ms. Above the held scan of MZ-047 (about 3.62 s); the wide goal look takes under 2 s. | `5000` (was `2500`) |
| `motionTestWalkTimeout` | Time limit of the walking step before the test aborts, in ms. | `30000` |
| `motionTestTurnTimeout` | Time limit of a turning step before the test aborts, in ms. | `15000` |
| `motionTestNumOfSteps` | Steps of the motion test (walk, left, right, around). | `4` |
| `motionTestBeep` | Sound played between steps (in `Config/Sounds`). | `"mazeBeep.wav"` |

### Skill parameters

`DEFINES_PARAMETERS` of the skills in `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/`. Status: **exists**; compile-time, not in a `.cfg`.

| Skill | Parameter | Meaning | Value |
|---|---|---|---|
| `MazeFollowCorridor` | `positionMargin` | Done when the robot is this close to the cell centre, in mm. | `70` |
| `MazeFollowCorridor` | `headingMargin` | ... and its heading differs from the target by less than this. | `15deg` |
| `MazeFollowCorridor` | `walkSpeed`, `turnSpeed` | Walking and rotation speed as ratio of the maximum, in [0, 1]. | `1`, `1` |
| `MazeTurn` | `margin` | Done when the remaining angle is smaller than this. | `6deg` |
| `MazeTurn` | `turnSpeed` | Rotation speed as ratio of the maximum, in [0, 1]. | `1` |
| `MazeScanWalls` | `wide` (skill argument) | `true`: the wide goal look of MZ-024.1 with the `panAngle` sequence below. `false`: the held scan of the next cell (MZ-047) with the `hold*` values. `HandleMaze` passes `DriveController::wideScan()`. | – |
| `MazeScanWalls` | `panAngle` | Wide: how far the head turns to each side. | `60deg` |
| `MazeScanWalls` | `tilt` | Wide: head tilt during the scan (as `LookForward`). | `0.38` rad |
| `MazeScanWalls` | `headSpeed` | Head speed, both sequences. | `200deg/s` |
| `MazeScanWalls` | `leftTime`, `rightTime`, `frontTime` | Wide: time to look left, cross and look right, and come back to the front, in ms (about 1.8 s in total). | `550`, `800`, `450` |
| `MazeScanWalls` | `holdPan` | Held scan: how far the head turns to each side and holds. At ±30° the gaze snaps to the robot heading. | `30deg` |
| `MazeScanWalls` | `holdTilt` | Held scan: head tilt (the next cell's wall is seen at 0.38 and ±30°). | `0.38` rad |
| `MazeScanWalls` | `holdTime` | Held scan: how long the head holds each of +30°, −30° and 0°, in ms, counted after the head travel at `headSpeed`. The mapper needs about 3 observations at one per 250 ms. The whole scan takes about 3.62 s (measured). | `1000` |

## 9. Return to start

File: `Config/Scenarios/Maze/handleMaze.cfg` and `Config/Scenarios/MazeSafe/handleMaze.cfg`. Ticket: MZ-024.1. Status: **implemented in MZ-024.1** (`returnToStart = {...}`; a file without the entry keeps the defaults below, so the old flow stays). Both files set `enabled = true` and `trigger = button` (18 Oct 2026).

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `returnToStart.enabled` | `false`: stand at the goal (`Finished`, says "goal" and "finished"); `true`: says "goal" only, then explores back to the start (says "returning"). | `false` | Q3, Q9 |
| `returnToStart.trigger` | `timer`: start `delayMs` after the goal is confirmed; `button`: start on a `headFront` tap at the goal. | `timer` | Q9 |
| `returnToStart.delayMs` | Wait after the goal is confirmed, for `trigger = timer`. | `5000` | Q9 |
| `returnToStart.maxDurationMs` | Return time limit (rules: ≤2 min), counted from "returning". The return also ends at the start cell or on a pick-up; it always goes to `Standby` and says "standby". | `120000` | Q3 |

The return plans to the start cell of `MazeMap` with unknown edges open and keeps scanning the next cell before each walk (held scan, like the exploration), so what it sees is in the map for attempt 2. Only a goal reached by the exploration (attempt 1) arms the return, and it starts with one wall scan at the goal cell (the "goal look": the wide ±60° sequence, forced even without unknown edges) so that the look-ahead shows the edges of the neighbouring goal cells. It does not change the attempt counter.

## 10. Start scan

File: `Config/Scenarios/Maze/handleMaze.cfg` and `Config/Scenarios/MazeSafe/handleMaze.cfg`. Ticket: MZ-041 (proposed). Status: **planned in MZ-041**. The ticket notes that `set parameters:` at run time must be checked for this module.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `startScan.enabled` | Pan the head between *positioned at start* and the whistle. Allowed only if the clock does not run during it. | not set yet (MZ-041) | Q11 |
| `startScan.durationMs` | Time the head pans from side to side. | `5000` | Q11 |
| `startScan.yawRangeDeg` | Head yaw range of the pan. | not set yet (MZ-041) | Q11 |
| `startScan.pitchDeg` | Head pitch during the scan (the tag is low, so it must look down). | not set yet (MZ-041) | Q11 |

## 11. Watchdog and safe mode

File: `Config/Scenarios/Maze/handleMaze.cfg` ("watchdog parameters"). Ticket: MZ-025. Status: **planned in MZ-025**. The ticket says the thresholds live in this file but names no keys.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| Watchdog stall time (unnamed) | No progress for this long triggers a replan (the judge may end the attempt after 5 s). | `3 s` (ticket and roadmap) | — |
| Safe-mode speed (unnamed) | Reduced speed for cell-by-cell walking with a tag re-fix at every cell. | not set yet (MZ-025) | — |
| Pose-quality threshold (unnamed) | Localizer quality below which safe mode starts. | not set yet (MZ-025) | — |

Head scans were limited to ≤2 s (`MazeScanWalls`, MZ-024; roadmap 6): the wide sequence in section 8 adds up to 1.8 s. The held scan of MZ-047 takes about 3.62 s, beyond that earlier limit (roadmap 6 still says ≤2 s; MZ-047 trades the time for the walls it sees).

## 12. Wall pose correction

File: not named in the ticket (module `MazeLocalizer`). Ticket: MZ-045 (proposed, built only if the MZ-020 logs show the cell estimate is lost between tag fixes). Status: **planned in MZ-045**.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `wallFix.maxRangeMm` | Walls farther than this are ignored for pose correction. | not set yet (MZ-045) | — |

## 13. Active perception (backlog)

File: `Config/Scenarios/Maze/` head-scan parameters. Ticket: MZ-037 (backlog). Status: **planned in MZ-037**.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| Head strategy of `MazeScanWalls` (unnamed) | Chooses H0 fixed head, H1 stop and scan, H2 scan while walking, or H3 look-ahead and information-driven gaze. | H1 until the comparison is done (since MZ-047, H1 scans the next cell with the held ±30° look, see section 8) | — |

## 14. Maze tools (command-line flags)

Not config files; flags of the scripts in `Util/MazeTools/`. Status: **exists** (MZ-009, MZ-010).

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `generate_maze.py --seed` | Random seed; the same seed gives the same maze. | required | — |
| `generate_maze.py --start` | Start corner cell, one of `0`, `7`, `56`, `63`. | `0` | — |
| `generate_maze.py --loops` | Extra internal walls removed to create loops. | `0` | — |
| `generate_maze.py --out` | Output file. | stdout | — |
| `generate_maze.py --validate` | Validate a `.maze` file instead of generating (prints `OK` or the first broken rule). | off | — |
| `maze_to_scene.py --out` | Output `.rsi2`; a copy of the maze is written next to it as `.maze`. | stdout | — |
| `maze_to_scene.py --tag-layout` | Tag layout file used to draw the tags. | `Config/Scenarios/Maze/mazeTagLayout.cfg` | Q1 (through the file) |
| `maze_to_scene.py` `WALL_COLOR`, `FLOOR_COLOR`, `GOAL_COLOR` | Scene colours, constants in the script; placeholders until the organisers confirm them. | `rgb(235, 235, 235)`, `rgb(200, 190, 170)`, `rgb(60, 120, 200)` | Q13 |
| `make_tag_textures.py --out` | Output directory of the tag36h11 textures (ids 0 to 63). | `Config/Scenes/Textures/AprilTags` | — |
| `run_sim_campaign.py --mazes` | Folder of mazes to run (planned in MZ-021). | `Config/Mazes/generated` (ticket example) | — |
| `run_sim_campaign.py --attempts` | Attempts per maze (planned in MZ-021). | not set yet (MZ-021); examples use 1 and 2 | — |
| `run_sim_campaign.py --noise` | Noise level `low`, `mid` or `high` (planned in MZ-021). | not set yet (MZ-021) | — |
| `run_sim_campaign.py --timeout-s` | Time limit per maze, in seconds (planned in MZ-021). | `300` (ticket example) | — |
| `run_sim_campaign.py --out` | Result CSV (planned in MZ-021). | not set yet (MZ-021) | — |

## 15. Logger

File: `Config/Scenarios/Maze/logger.cfg` (and `MazeSafe`). Ticket: MZ-007 (representations extended by MZ-039 and MZ-044). Status: **exists**. Standard B-Human logger settings, not maze-specific tuning; the maze-relevant one is `representationsPerThread`, which must keep `InertialData` for the gyro drift measurement (MZ-020).

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `enabled` | Logging on or off. | `true` | — |
| `path` | Directory of the log file on the robot. | `"/home/nao/logging"` | — |
| `numOfBuffers` | Number of buffers allocated. | `12000` | — |
| `sizeOfBuffer` | Size of each buffer, in bytes. | `200000` | — |
| `writePriority` | Scheduling priority of the writer thread. | `-2` | — |
| `minFreeDriveSpace` | Logging stops below this free space, in MB. | `100` | — |
| `representationsPerThread` | Representations logged per thread (outside *Initial* only). | `Upper`: `CameraInfo`, `CameraMatrix`, `FrameInfo`, `JPEGImage`, plus the other threads in the file | — |

## Doc versus config mismatches

- None found for the parameters above: the real `.cfg` values equal the documented defaults.
- Roadmap 3.6 lists the walking `maxSpeed` default as 120°/s, 250 mm/s, 200 mm/s; this matches `Config/Robots/Default/walkingEngine.cfg`. The Maze (60 %) and MazeSafe (40 %) values in section 4 are the real `.cfg` values.
