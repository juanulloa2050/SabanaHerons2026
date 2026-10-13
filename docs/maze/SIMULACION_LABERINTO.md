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
| `dr option:HandleMaze:headFront on` | `headFront` ×1 | Says "start". |
| `dr option:HandleMaze:headRear on` | `headRear` ×1 | Says "maze ready". |
| `dr option:HandleMaze:headMiddleTwice on` | `headMiddle` ×2 | Says "motion test", walks about 2 m (4 cells) forward, then turns 90° left, 90° right and 180°, with a beep in each pause. |
| `dr option:HandleMaze:abortMotionTest on` | Any head button during the test | Says "motion test aborted" and stops the test. |

The motion test walks straight for 2 m and does not know about walls: inside a maze it will run into
one unless the start corridor is long enough. A step that does not finish within its time limit
(30 s walking, 15 s turning) aborts the test on its own.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Scene fails to load, include not found | `current.rsi2` was never generated | Run step 3. |
| Robot leaves the arena and comes back at a sideline | Automatic referee penalty | `ar off`, then `gc playing`. |
| Nothing happens after a `dr option:HandleMaze:…` command | Robot not in playing | `gc playing`. |
| No speech | Speech muted in simulation | `dr unmute on`. |
