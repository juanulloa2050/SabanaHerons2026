# Maze simulation — generate and run

How to build a maze, turn it into a SimRobot scene and drive the maze behaviour from the SimRobot
console. Tools: `Util/MazeTools/` (details in `Util/MazeTools/README.md`). Scene: `Config/Scenes/Maze.ros2`.

## 1. Build SimRobot

Command line:

```bash
Make/Linux/compile Develop SimRobot
```

CLion:

1. **CMake profile:** pick `Develop` in the profile selector. `Debug` compiles much more slowly.
2. **Run configuration:** pick `SimRobot` and press ▶ Run. CLion compiles and opens SimRobot.

## 2. Generate a maze (optional)

Twenty ready-made mazes live in `Config/Mazes/generated/maze_1.maze` … `maze_20.maze`. To make a new one:

```bash
python3 Util/MazeTools/generate_maze.py --seed 42 --start 0 --loops 2 --out /tmp/my.maze
python3 Util/MazeTools/generate_maze.py --validate /tmp/my.maze   # prints OK or the broken rule
```

- `--seed` (required): same seed, same maze.
- `--start {0,7,56,63}`: start corner cell (default 0, top-left).
- `--loops N`: extra internal walls removed to create loops (default 0).

## 3. Generate the scene

`Maze.ros2` includes `Config/Scenes/Includes/Maze/current.rsi2`. That file is **generated and
git-ignored**: each person creates it locally, and regenerating it never shows up in `git status`.
After cloning, create it once before opening the scene:

```bash
python3 Util/MazeTools/maze_to_scene.py Config/Mazes/generated/maze_1.maze --out Config/Scenes/Includes/Maze/current.rsi2
```

Run it again with another `.maze` file to switch mazes, then reopen the scene. Wall, floor and goal
colours are placeholders: edit `WALL_COLOR`, `FLOOR_COLOR` and `GOAL_COLOR` at the top of
`Util/MazeTools/maze_to_scene.py` and regenerate.

The same command also writes `current.maze` (the maze the oracle reads, see step 5) and puts the
AprilTags (tag36h11, 15 cm black square, centre 30 cm above the floor) on the wall faces chosen by
`Config/Scenarios/Maze/mazeTagLayout.cfg`. The tag id is the cell index (`row * 8 + col`).

### Change where the AprilTags are

1. Edit `Config/Scenarios/Maze/mazeTagLayout.cfg`:

   | `mode` | Tags on |
   |---|---|
   | `allFaces` (default) | every wall face of every cell |
   | `faces` | only the faces listed in `faces`, e.g. `faces = [east];` |
   | `walls` | only the listed pairs, e.g. `walls = [{cell = 10; face = east;}, {cell = 11; face = north;}];` |

   Faces are absolute (`north` = top of the `.maze` text). A face never carries a tag where there is no wall.
2. Regenerate the scene, so the tags you see move too:

   ```bash
   python3 Util/MazeTools/maze_to_scene.py Config/Mazes/generated/maze_1.maze --out Config/Scenes/Includes/Maze/current.rsi2
   ```

3. Reopen the scene. No compiling is needed.

The oracle reads the layout when the scene is opened, but the visible tags are written into `current.rsi2`.
Skipping step 2 leaves the tags on screen and the tags the robot "sees" out of sync.

## 4. Open the scene

```bash
Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/Maze.ros2
```

Or, in SimRobot: *File → Open → `Config/Scenes/Maze.ros2`*.

`Maze.con` runs automatically and:

- loads the normal console setup and views,
- hides the game clock and score board (`dr debugDrawing3d:representation:GameState off`): the
  maze has no game time or goals, and with the referee off the clock reaching 0 changes nothing,
- turns the automatic referee off (`ar off`), so the robot is not penalized for standing in the
  opponent half,
- puts the robot in **playing** (`gc playing`), the only state in which `HandleMaze` reacts,
- unmutes speech in simulation (`dr unmute on`).

The robot stands at the centre of the start cell, facing its open side. The goal block (cells 27, 28,
35, 36) is the coloured 1 m × 1 m patch in the centre.

## 5. Console commands

Type them one at a time in the SimRobot console.

### Game states

| Command | Effect |
|---|---|
| `gc initial` | Initial: the robot stands, `HandleMaze` does nothing. |
| `gc playing` | Playing: `HandleMaze` reacts to the gestures below (set by `Maze.con`). |
| `ar off` / `ar on` | Automatic referee off / on. Keep it off in the maze. |

On the real robot there is no GameController: three chest-button taps bring it to playing.

### Maze behaviour (`HandleMaze`)

The head buttons cannot be pressed in SimRobot, so each gesture is simulated once with a debug request.
The robot must be in playing.

| Command | Same as | Expected result |
|---|---|---|
| `dr option:HandleMaze:headFront on` | `headFront` ×1 | Starts the attempt: says "exploring", or "speed run" if the map holds a route to the goal (full flow: step 5, *Running the full flow*). |
| `dr option:HandleMaze:headRear on` | `headRear` ×1 | Says "tag <id>" for the last seen AprilTag (within 2 s), else "no tag". |
| `dr option:HandleMaze:headMiddleTwice on` | `headMiddle` ×2 | Says "motion test", walks about 2 m (4 cells) forward, then turns 90° left, 90° right and 180°, with a beep in each pause. |
| `dr option:HandleMaze:abortMotionTest on` | Any head button during the test | Says "motion test aborted" and stops the test. |

The motion test walks straight for 2 m and does not know about walls: inside a maze it will run into
one unless the start corridor is long enough. A step that does not finish within its time limit
(30 s walking, 15 s turning) aborts the test on its own.

### Maze perception (what the robot sees)

In simulation, `OracledMazePerceptsProvider` computes the tags and walls the robot would see from its true
pose and `current.maze`. It does not use the camera image. On the real robot the tags come from the camera
(`AprilTagDetector`, MZ-018), see step 6.

| Command | Shows |
|---|---|
| `get representation:AprilTagPercept` | The tags seen now: `id`, `tagInRobot` (position in mm: x ahead, y left, z up) and `decisionMargin`. `detections = []` means none. |
| `get representation:WallPercept` | Walls seen now: `frontDistance`, `leftDistance`, `rightDistance` (mm, `-1` = no wall) and `cellsAhead` (cells along the gaze, `0` = the robot's cell). |
| `vfd worldState representation:AprilTagPercept:field` | Draws the seen tag ids in the `worldState` field view. |
| `vfd worldState representation:WallPercept:field` | Draws the seen walls in the `worldState` field view. |

`get` prints one snapshot: run it again after moving the robot. For a live view, double-click
`representation:AprilTagPercept` or `representation:WallPercept` under the robot's `data` entry in the scene tree.

Oracle parameters (noise, dropout, range) live in `Config/Scenarios/Maze/oracledMazePerceptsProvider.cfg`.
To change one for this session only, run `get parameters:OracledMazePerceptsProvider`, copy the printed
`set parameters:OracledMazePerceptsProvider …` line, edit the value (e.g. `dropoutRate = 1;` gives empty
percepts) and run it.

### Localizer and mapper (what the robot believes)

`MazeLocalizer` gives the pose in the maze frame (`RobotPose`, `MazeCellPose`) and `MazeMapper` the wall map (`MazeMap`). Both run in Cognition, from the oracle percepts. Background: `GYRO_HEADING.md`; every key: `PARAMETROS_LABERINTO.md`, section 6.

| Command | Shows |
|---|---|
| `vfd worldState representation:MazeMap` | The believed walls: decided edges as wall or open, `unknown` edges left empty. |
| `vfd worldState module:MazeLocalizer:pose` | The estimated pose (cell and heading) in the maze frame. |
| `vfd worldState module:MazeMapper:lookAhead` | The cells seen along the gaze (orange). A red dashed rectangle means the look-ahead was skipped this frame: the estimate is more than `maxOffsetFromCentre` from its cell centre. |
| `get representation:MazeCellPose` | One snapshot: cell, heading, `headingSource`, `headingDrift` (net gyro offset drift since the last reset), `tagFixes`, `tagRejections`, `resets`. |
| `get representation:RobotPose` | One snapshot: pose in mm (maze frame) and `quality`. |
| `get representation:MazeMap` | One snapshot: the edges, visited cells, `attempt` and `changeCounter`. |

To set up the world state view, paste these lines into the console:

```
vfd worldState representation:MazeMap
vfd worldState module:MazeLocalizer:pose
vfd worldState module:MazeMapper:lookAhead
```

The map and the pose are drawn in the maze frame (origin at the bottom-left outer corner, no 180° turn), so the drawing
can appear shifted or turned against the 3D maze. Compare the shape, not the position on the field.

`RobotPose` is in the maze frame, not in the SimRobot field frame, so compare it with the maze, not with the ground-truth robot position.

**Move or turn the robot.** With the mouse in the 3D scene view: drag to move it, Shift + drag to turn it about its own
axis, Ctrl + Shift + drag to turn it in the world frame. From the console: `mv <x> <y> <z> <rotx> <roty> <rotz>` (mm and
degrees, team frame). For the maze robot the centre of cell (row, col) is `x = 1750 − 500·col`, `y = −1750 + 500·row`,
`z = 300`, and `rotz` is 90 (south), 180 (east), −90 (north) or 0 (west). Example, cell 19 facing south then east:

```
mv 250 -750 300 0 0 90
mv 250 -750 300 0 0 180
```

Moving the robot this way is a teleport: odometry does not see it, so only a tag fix brings the estimate to the new cell.
A teleported turn of 90° or more is larger than `maxFaceAngleError`, so the localizer may reject the tags
(`tagRejections` grows) until the heading matches again. On the real robot nobody teleports it; to test turns, let the
robot turn itself (`headMiddleTwice` motion test).

**Pick-up and Standby.** `gc initial`, drag the robot back to the start cell facing its open side, `gc playing`. The robot
is not moved by the code; the localizer resets the pose to the start cell (`resets` grows), and `MazeMap` keeps its edges
with `attempt` one higher.

- **Start cell and heading:** edit `start.cell` and `start.heading` in `Config/Scenarios/Maze/handleMaze.cfg` (one place for the localizer and the mapper). `start.cell = -1` (default) makes the robot deduce its start from the tags and walls; `0`, `7`, `56` or `63` gives it, and then `start.heading` says which of the two open sides it faces (cell 0: `east`/`south`, 7: `south`/`west`, 56: `north`/`east`, 63: `north`/`west`; the default `south` matches `--start 0`). An invalid value prints an error and is treated as `-1`. The start pose is taken on every transition into active. Details: `PARAMETROS_LABERINTO.md`, section 8.
- **Inject drift and noise** (SimRobot has none): set `injectedGyroDriftPerMinute`, `injectedOdometryTranslationNoise` (0.05 = 5 %) or `injectedOdometryRotationNoise` in `mazeLocalizer.cfg`. To change them for this session only, run `get parameters:MazeLocalizer`, copy the printed `set parameters:MazeLocalizer …` line, edit the value and run it.
- **Hide the tags** to see the quality drop: set `dropoutRate = 1;` as shown above, then watch `quality` in `RobotPose` go from superb to okay to poor.
- **Other layouts:** change `mazeTagLayout.cfg` and regenerate the scene (step 3). The localizer needs no change.
- The map learns only in `playing`, and a pick-up (Standby) does not clear it.

Checked in SimRobot on 16 Oct: the cell estimate follows the robot (cells 0, 3, 19, 20) and walls already seen stay in the
map when the robot crosses a cell boundary. The offline results are in `GYRO_HEADING.md`.

### Running the full flow (MZ-024)

`HandleMaze` runs the phases `waitingForStart` → `exploration` → `finished` → `standby` → `speedRun`. The robot says each change (the console prints the `Say` text and `HandleMaze: <phase>.`). Start from a freshly opened scene (`Maze.con` already did `gc playing`).

| Step | Console | Expected |
|---|---|---|
| 1. Ready | `gc playing` (already set by `Maze.con`) | Says "ready". |
| 2. Attempt 1 | `dr option:HandleMaze:headFront on` | Says "exploring"; the robot scans unknown walls and walks cell by cell. With `start.cell = -1` it says "start <id>" once the corner is deduced. |
| 3. Goal | (wait) | In a central cell: says "goal", then "finished", and stands. |
| 4. Pick-up | `gc initial`, drag the robot back to the start cell facing its open side, `gc playing` | Says "standby" after `gc initial`, then "ready" after `gc playing`. The pose is reset to the start and `MazeMap` keeps its edges with `attempt` one higher. A chest-button penalty does not reach `HandleMaze` (`HandlePlayerState` acts while penalized): `pr manual` makes B-Human say "Penalized" and the robot stands; `pr none` makes it say "Not penalized", then `HandleMaze` goes to `standby` and says "ready" (no "standby" is said). The robot stays standing until `dr option:HandleMaze:headFront on`. |
| 5. Attempt 2 | `dr option:HandleMaze:headFront on` | Says "speed run" if the known-open edges reach the goal (the console prints the route: cells, actions, estimated time, and "all edges known to be open"), otherwise "exploring" again. |
| 6. Gestures | `dr option:HandleMaze:headRear on`, `dr option:HandleMaze:headMiddleTwice on` | Only while standing (`waitingForStart`, `standby`, `finished`): "tag <id>" / "no tag", and the motion test. |

`announce.enabled = false` in `handleMaze.cfg` silences all speech; the phases do not change (the console lines stay).

### Running the return to start (MZ-024.1)

The return is off by default (`returnToStart.enabled = false`: the robot stands at the goal, as in step 3 above). To try it:

1. In `Config/Scenarios/Maze/handleMaze.cfg` (or `MazeSafe`) set `returnToStart = {enabled = true; trigger = timer; delayMs = 5000; maxDurationMs = 120000;};`. Use `trigger = button` to start it by hand. Reopen the scene (the file is read when the behaviour is created).
2. Run attempt 1 as above. At the goal the robot says "goal" only (no "finished"). Only a goal reached by the exploration (attempt 1) arms the return; a speed-run goal still says "goal", "finished" and stays.
3. `trigger = timer`: after `delayMs` it says "returning" and walks back, trying unknown edges. `trigger = button`: it stands until `dr option:HandleMaze:headFront on` (the tap is free at the goal), then says "returning". Before the first move the robot makes one wall scan (the "goal look", even if the goal cell has no unknown edges): its look-ahead shows the edges of the neighbouring goal cells, so the first return plan already knows them.
   When the return begins the robot first makes one wall scan (the "goal look", even if the goal cell has no unknown edges): its look-ahead shows the edges of the neighbouring goal cells, so the first return plan already knows them.
4. The return ends at the start cell, or after `maxDurationMs` (also if no route is found), or when the robot is picked up (`gc initial`): it says "standby" and stands. `dr option:HandleMaze:headFront on` then starts the speed run with the edges found on the way (console: `HandleMaze: speed run over ...`).

Watch `get representation:MazePlan` (`returnToStart` route, `atStart`, `startCell`) and `vfd worldState representation:MazePlan` (the green dotted line is the return route). The unit test `MazeRoutes.TheReturnTriesTheUnknownShortcutInsteadOfTheWalkedLoop` checks the `maze_19.maze` example offline.

What to look at:

| Command | Shows |
|---|---|
| `get representation:MazeStart` | `known`, `given`, the working hypothesis (`cell`, `heading`), `aliveCount`, `revision`, `conflicts`. |
| `get representation:MazePlan` | `exploration` and `speedRun` routes (`found`, `nextAction`, `cells`, `actions`, `estimatedTimeMs`), `atGoal`, `valid`, `revision`. |
| `vfd worldState representation:MazePlan` | The routes drawn in the `worldState` field view (the drawing id is `representation:MazePlan`; maze frame, so compare the shape). |
| `get parameters:MazePlannerProvider` | The planner costs; edit and run the printed `set` line to change `turn90Ms` for this session and watch the plan change. |

Mazes with a known start: `Config/Mazes/fixtures/start_<cell>_<heading>.maze` (cells 0, 7, 56, 63, each with its two open sides; plus `boundary_test.maze`). Load one into the scene like any other maze, then set `start.cell`/`start.heading` to match, or `start.cell = -1` to test the deduction:

```bash
python3 Util/MazeTools/maze_to_scene.py Config/Mazes/fixtures/start_63_west.maze --out Config/Scenes/Includes/Maze/current.rsi2
```

Reopen the scene afterwards. The robot appears in the start cell of the maze file.

Dragging the robot in the 3D view lifts it. The code treats `FallDownState` `pickedUp` as a pick-up (a fall is not one), so a drag that SimRobot reports as picked up ends the attempt (`standby`) and the pose is reset to the start when the robot is active again, wherever it was put down. The tags it then sees contradict that pose, which the mapper counts as conflicts and warns about (`MazeMapper: an observation contradicts ... Latest: tag`); the start does not change once it is known. Check `get representation:FallDownState` while dragging: whether SimRobot reports a drag as `pickedUp` has not been verified yet. `gc initial` then `gc playing` always works as a pick-up.

## 6. Real robot: see the AprilTags (RemoteRobot)

The detector (`AprilTagDetector`) runs on the upper camera of the NAO, only in the `Maze` scenario.

1. Build and deploy with the `Maze` scenario:

   ```bash
   Make/Linux/compile Develop Nao
   Make/Common/deploy Develop <robot-ip> -s Maze
   ```

2. Open the remote scene and connect to the robot (the dialog asks for its IP):

   ```bash
   Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/RemoteRobot.ros2
   ```

3. In the console, type:

   ```
   call Includes/AprilTagViews
   ```

   | Command (run by `AprilTagViews`) | Shows |
   |---|---|
   | `vid upper module:AprilTagDetector:tags` | Each detected tag in the `upper` image: green outline and `id N`. |
   | `vf aprilTags` + `vfd aprilTags representation:AprilTagPercept:field` | A field view `aprilTags` with the tags around the robot (bar = tag, arrow = its normal, which must point back at the robot). |
   | `for Upper get representation:AprilTagPercept` | One snapshot: `id`, `corners` (pixels), `tagInRobot` (mm: x ahead, y left, z up) and `decisionMargin`. `detections = []` means none. |

4. Run `for Upper get representation:AprilTagPercept` again for a new snapshot. For a live view,
   double-click `representation:AprilTagPercept` under the robot's `data` → `Upper` entry in the scene tree.

Without the computer: a single `headRear` tap makes the robot say "tag <id>" for the best tag seen in the
last 2 s, or "no tag" (the robot must be in playing: three chest-button taps).

Detector parameters live in `Config/Scenarios/Maze/aprilTagDetector.cfg` (what each one does: ticket
MZ-018.1 in `TICKETS_LABERINTO.md`). To change one for this session only, run
`for Upper get parameters:AprilTagDetector`, edit the printed `set parameters:AprilTagDetector …` line and
run it. The time per image is in the robot's `timing` → `Upper` view, row `AprilTagDetector` (target ≤ 20 ms).

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Scene fails to load, include not found | `current.rsi2` was never generated | Run step 3. |
| Robot leaves the arena and comes back at a sideline | Automatic referee penalty | `ar off`, then `gc playing`. |
| Nothing happens after a `dr option:HandleMaze:…` command | Robot not in playing | `gc playing`. |
| No speech | Speech muted in simulation | `dr unmute on`. |
| Tags on screen differ from `get representation:AprilTagPercept` | `mazeTagLayout.cfg` changed without regenerating the scene | Run step 3, reopen the scene. |
| Percepts always empty | `dropoutRate = 1`, or `current.maze` missing (warning in the console) | Reset `dropoutRate`; run step 3. |
| RemoteRobot: no green outlines, `get` on `AprilTagDetector` fails | The robot runs another scenario | Deploy again with `-s Maze` (step 6). |
| RemoteRobot: the `upper` image is empty | The image is not streamed | `for Upper Lower mr JPEGImage CameraProvider`. |
| RemoteRobot: a tag is visible but not detected | Too far, too dark or filtered by `minDecisionMargin` | Move closer; check `decisionMargin` and the parameters (step 6). |
