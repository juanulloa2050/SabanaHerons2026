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

## Tests

```bash
python3 -m unittest discover -s Util/MazeTools -v
```
