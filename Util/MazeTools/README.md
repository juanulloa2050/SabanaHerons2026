# MazeTools

Generator and validator for 8x8 maze files (`.maze`), used by the simulator scene converter,
the planner tests and the campaigns. Python 3, standard library only.

## Format

17 lines of 17 characters, trailing newline. Row 0 is the top row (tag id 0 at the top-left).

- Even/even positions are corners `+`.
- (even line, odd column) is a horizontal wall: `-` or space.
- (odd line, even column) is a vertical wall: `|` or space.
- Character (2r+1, 2c+1) is cell (r, c): `S` (start), `G` (goal) or space. Cell index = `r*8 + c`.
- The goal is always the central 2x2 block: four `G`, in cells 27, 28, 35 and 36 (rows 3-4, cols 3-4).
  There are no walls inside the block; its perimeter has one or more entrances.
  The number of entrances is open (roadmap Q12); the generator may produce one or several.

Top-left corner example:

```
+-+-+
|S  |
+ +-+
```

Full example (`--seed 1 --loops 1`):

```
+-+-+-+-+-+-+-+-+
|S|             |
+ +-+-+ + +-+-+ +
| |     |     | |
+ + + +-+-+-+ + +
|   |     |   | |
+-+-+-+-+ + +-+ +
|     |G G  | | |
+-+-+ + + +-+ + +
|     |G G|   | |
+ +-+-+-+-+ + + +
|   |   |   | | |
+ + + + + + +-+ +
| | | | | |     |
+ + + + + +-+-+-+
| |   |         |
+-+-+-+-+-+-+-+-+
```

## Generator rules

Deterministic for the same arguments (`random.Random(seed)`).

1. All walls up, then the 4 walls inside the goal block (cells 27, 28, 35, 36) are opened.
2. The goal block is one node of a randomized DFS (recursive backtracker): reaching any block cell
   visits all four. With `--loops 0` this is a perfect maze over 61 nodes (60 tree edges) plus the 4
   open block walls, i.e. 64 open internal walls.
3. The start is a leaf: the DFS runs from one of its neighbours with the start pre-visited, then the
   start is joined to that neighbour (exactly 3 walls).
4. `--loops N`: open N random closed internal walls (never border, never a side of the start),
   capped at the number available.

The outer border is always closed and every cell is reachable from the start.

## Usage

```bash
python3 Util/MazeTools/generate_maze.py --seed 1 --start 63 --loops 2 --out /tmp/m.maze
python3 Util/MazeTools/generate_maze.py --validate /tmp/m.maze   # prints OK or the first broken rule

mkdir -p Config/Mazes/generated
for s in $(seq 1 20); do python3 Util/MazeTools/generate_maze.py --seed $s --loops $((s % 4)) --out Config/Mazes/generated/maze_$s.maze; done
```

`--seed` is required when generating. `--start` is one of 0, 7, 56, 63 (default 0). Without `--out`
the maze is printed to stdout. `--validate` cannot be combined with generation options; errors go to
stderr and the exit code is 1.

## Scene converter (SimRobot)

`maze_to_scene.py` turns a `.maze` file into `Config/Scenes/Includes/Maze/current.rsi2`, which
`Config/Scenes/Maze.ros2` includes.
`current.rsi2` is git-ignored: generate it once after cloning (first command below). Step-by-step
guide and console commands: `docs/maze/SIMULACION_LABERINTO.md`.

```bash
python3 Util/MazeTools/maze_to_scene.py Config/Mazes/generated/maze_1.maze --out Config/Scenes/Includes/Maze/current.rsi2
Make/Linux/compile Develop SimRobot
Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/Maze.ros2
```

The input is validated with `generate_maze.py`; an invalid file prints the broken rule to stderr and
exits with 1. Without `--out` the scene is printed to stdout. Output is deterministic and starts with
a comment naming the source file. Regenerate `current.rsi2` after changing the maze, then reopen the scene.

- One `BoxGeometry` per `-` or `|` character, border included: 0.5 m cell side plus 1.5 cm overlap
  (`0.515m x 0.015m x 0.6m`), centred at z = 0.3 m.
- Each wall also carries a `BoxAppearance` of the same size and position (a `BoxGeometry` alone is
  invisible). `Maze.con` runs `ar off`, so the automatic referee does not penalize the robot.
- `Maze.ros2` no longer includes the SPL field (no green carpet, lines or goals). The converter emits
  a `mazeFloor` compound instead: a 6 m x 6 m matte slab (maze plus 1 m margin, top at z = 0, geometry
  and appearance) and, over the goal block (cells 27, 28, 35, 36), a 1 m x 1 m x 2 mm patch centred on the
  origin with a different colour. The patch is appearance only, so the physics floor stays flat.
- Changing colours: floor, goal patch and wall colours are placeholders until the organisers confirm them.
  Edit `FLOOR_COLOR`, `GOAL_COLOR` and `WALL_COLOR` at the top of `maze_to_scene.py`, then regenerate
  `current.rsi2`. Tests read the colours from those constants.
- The maze is centred on the simulator origin. Cell (row, col) is centred at
  `x = (col - 3.5) * 0.5 m`, `y = (3.5 - row) * 0.5 m`, so row 0 (top of the file) is the +y side.
- The robot (`mazeRobots` compound, placed in the `robots` group by `Maze.ros2`) stands at the centre of `S`
  at z = 320 mm, heading towards the single open side: +x = 0, +y = 90, -x = 180, -y = -90 degrees.
- SimRobot shows the ground-truth pose in the team frame, which is the world frame rotated by 180 degrees.

## Tests

```bash
python3 -m unittest discover -s Util/MazeTools -v
```
