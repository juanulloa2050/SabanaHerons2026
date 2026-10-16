# Maze Challenge Parameters

Single reference of every tunable parameter of the Maze challenge: what it does, its default, where it lives, whether it exists today or is only planned in a ticket, and whether its value waits on an unanswered organiser question. Values change by editing the `.cfg` file and redeploying, unless the section says otherwise. `Maze` and `MazeSafe` hold identical copies of every file below, except where a section states a difference.

Related: [TICKETS_LABERINTO.md](TICKETS_LABERINTO.md) (owning tickets) · [ROADMAP_LABERINTO.md](ROADMAP_LABERINTO.md) (architecture, section 11 for the questions) · [SIMULACION_LABERINTO.md](SIMULACION_LABERINTO.md) (changing values live in SimRobot).

Conventions: **exists** = present in code or config today. **planned in MZ-0xx** = named in a ticket, not implemented yet. `not set yet (MZ-0xx)` = the ticket names the parameter but gives no value.

## Open organiser questions

Only questions that affect a parameter. All are still pending (no official answer is recorded in the roadmap). Until answered, the defaults below are team assumptions. Q2 and Q5 are resolved, Q4 is withdrawn.

| Id | Question | Parameters affected |
|---|---|---|
| Q1 | Which walls carry tags (every face of every cell? only some?) and at what height is the tag centre? | `mode`, `faces`, `walls`, `centreHeight` in `mazeTagLayout.cfg` |
| Q3 | Does the ≤2 min return to start (after attempt 1) count inside the 5 min of the attempt? Example: attempt 1 takes 4 min to reach the goal. Does the robot still get the full 2 min to return, or only the remaining 1 min? | `returnToStart.maxDurationMs`, `returnToStart.enabled` |
| Q6 | Is the robot placed facing the open side of the start cell? | `start.cell` (deduction assumes it does) |
| Q9 | Must the robot detect the goal by itself and start the return, or may a team member trigger the return with a robot button once the judge stops the clock? | `returnToStart.trigger`, `returnToStart.enabled`, `returnToStart.delayMs` |
| Q11 | Does the clock start when the robot leaves the first cell, or when it starts to move? | `startScan.enabled` (and the other `startScan.*` values) |
| Q13 | What colour and material is the maze floor (outside the goal block)? The rules say light walls and a matte uniform floor, but not the floor colour. If the floor is also light, the wall base is hard to see. | Possible colour threshold of the wall perceptor (not a parameter yet, MZ-019); simulation colours `WALL_COLOR`, `FLOOR_COLOR`, `GOAL_COLOR` |

## 1. Tag layout

File: `Config/Scenarios/Maze/mazeTagLayout.cfg` (and `MazeSafe`). Ticket: MZ-013. Status: **exists**. Read by the oracle, the detector (tag size), the localizer (MZ-020) and `Util/MazeTools/maze_to_scene.py`. After a change, regenerate the scene (`maze_to_scene.py`) so the drawn tags match.

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

File: not created yet (the ticket does not name a `.cfg`; module `Src/Modules/Perception/WallPerceptor/`). Ticket: MZ-019. Status: **planned in MZ-019**. The three values are set from the range-error measurements of that ticket.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `lookAheadRangeWalking` | Cells ahead whose side walls are reported while the robot walks (the torso pitches). | `2` | — |
| `lookAheadRangeStanding` | Cells ahead reported while the robot stands and scans (up to 4 if the measurements allow). | `3` | — |
| `snapToleranceMm` | A wall measurement is kept only if it lies within this distance of a 500 mm grid edge. | `150` | — |

Q13 does not change these values. The boundary is found by brightness gradient, not colour; a colour threshold may be added only after Q13 is answered and shows a clear contrast (no such parameter exists yet).

## 6. Localizer and mapper

File: not created yet (modules `MazeLocalizer`, `MazeMapper`). Ticket: MZ-020. Status: **planned in MZ-020**. The ticket gives these behaviours but names no config keys.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| Hysteresis count (unnamed) | Consistent observations needed before an edge goes `unknown → wall/open`. | `3` (roadmap 6) | — |
| Distance weight (unnamed) | How fast the weight of a wall-percept observation drops with distance. | not set yet (MZ-020) | — |
| Complementary-filter gain (unnamed) | Blend of odometry and tag fix in the pose (roadmap 4.5). | not set yet (MZ-020) | — |
| Square-on tag threshold for heading snap (unnamed) | How square-on a tag must be to re-align the gyro offset. | not set yet (MZ-020) | — |

## 7. Planner costs

File: `Config/Scenarios/Maze/mazePlanner.cfg` (and `MazeSafe`). Ticket: MZ-024 creates the file; MZ-028 replaces the values with the Lab 2 means. Status: **file planned in MZ-024**; the defaults **exist** as the `MazeCosts` struct in `Src/Tools/Maze/MazePlanner.h` (fields `forwardMs`, `startMs`, `turn90Ms`, `turn180Ms`; the tickets call them `t_fwd`, `t_start`, `t_turn90`, `t_turn180`). The Python solver and `expected.csv` use the same defaults and must be regenerated if they change.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `t_fwd` | Time for one cell forward while already walking. | `2.5 s` (2500 ms) | — |
| `t_start` | Extra time for the first step after standing (after a turn or at the start). | `0.8 s` (800 ms) | — |
| `t_turn90` | Time for a 90° turn in place. | `2.0 s` (2000 ms) | — |
| `t_turn180` | Time for a 180° turn in place. | `3.5 s` (3500 ms) | — |

Backlog (MZ-043, defaults stated in the ticket): `rollingTurns.enabled` = `false` and `diagonals.enabled` = `false` in `mazePlanner.cfg`; the rolling-turn and diagonal action costs are not set yet (MZ-043).

## 8. HandleMaze behaviour

File: `Config/Scenarios/Maze/handleMaze.cfg` (and `MazeSafe`). Ticket: MZ-024. Status: **planned in MZ-024** (the file does not exist yet).

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `announce.enabled` | Speak every state change and button gesture; `false` silences all speech. | not set yet (MZ-024); the ticket says `false` silences it | — |
| `start.cell` | Start corner: `0`, `7`, `56`, `63` when known, `-1` to deduce it from tags. Other values are rejected at load. | `-1` | Q6 |

### Compile-time constants of the current stub

In `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h`. Status: **exists**; changing them needs a rebuild, not a config edit.

| Constant | Meaning | Value |
|---|---|---|
| `mazeCellSize` | Edge of one maze cell, in mm. | `500` |
| `motionTestCells` | Cells walked forward in the first step of the motion test. | `4` |
| `motionTestPauseTime` | Pause (with a beep) between motion-test steps, in ms. | `2000` |
| `mazeAnnouncementTime` | Time after an announcement before the next tap is accepted, in ms. | `1500` |
| `tagMemoryTime` | A tag last seen longer ago than this is announced as "no tag", in ms. | `2000` |
| `motionTestWalkTimeout` | Time limit of the walking step before the test aborts, in ms. | `30000` |
| `motionTestTurnTimeout` | Time limit of a turning step before the test aborts, in ms. | `15000` |
| `motionTestNumOfSteps` | Steps of the motion test (walk, left, right, around). | `4` |
| `motionTestBeep` | Sound played between steps (in `Config/Sounds`). | `"mazeBeep.wav"` |

## 9. Return to start

File: `Config/Scenarios/Maze/handleMaze.cfg`. Ticket: MZ-024.1 (proposed). Status: **planned in MZ-024.1**.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `returnToStart.enabled` | `false`: stand at the goal (`Finished`); `true`: explore back to the start. | `false` | Q3, Q9 |
| `returnToStart.trigger` | `timer`: start `delayMs` after the goal is confirmed; `button`: start on a `headFront` tap at the goal. | `timer` | Q9 |
| `returnToStart.delayMs` | Wait after the goal is confirmed, for `trigger = timer`. | `5000` | Q9 |
| `returnToStart.maxDurationMs` | Return time limit (rules: ≤2 min). | `120000` | Q3 |

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

Head scans are limited to ≤2 s (`MazeScanWalls`, MZ-024; roadmap 6), not yet a named parameter.

## 12. Wall pose correction

File: not named in the ticket (module `MazeLocalizer`). Ticket: MZ-045 (proposed, built only if the MZ-020 logs show the cell estimate is lost between tag fixes). Status: **planned in MZ-045**.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| `wallFix.maxRangeMm` | Walls farther than this are ignored for pose correction. | not set yet (MZ-045) | — |

## 13. Active perception (backlog)

File: `Config/Scenarios/Maze/` head-scan parameters. Ticket: MZ-037 (backlog). Status: **planned in MZ-037**.

| Parameter | Meaning | Default | Depends on question |
|---|---|---|---|
| Head strategy of `MazeScanWalls` (unnamed) | Chooses H0 fixed head, H1 stop and scan, H2 scan while walking, or H3 look-ahead and information-driven gaze. | H1 until the comparison is done | — |

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
