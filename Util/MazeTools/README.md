# MazeTools

Generator and validator for 8x8 maze files (`.maze`), used by the simulator scene converter,
the planner tests and the campaigns. Python 3, standard library only.

## Format

17 lines of 17 characters, trailing newline. Row 0 is the top row (tag id 0 at the top-left).

- Even/even positions are corners `+`.
- (even line, odd column) is a horizontal wall: `-` or space.
- (odd line, even column) is a vertical wall: `|` or space.
- Character (2r+1, 2c+1) is cell (r, c): `S` (start), `G` (goal) or space. Cell index = `r*8 + c`.

Top-left corner example:

```
+-+-+
|S  |
+ +-+
```

Full example (`--seed 1 --loops 1`):

```
+-+-+-+-+-+-+-+-+
|S|     |       |
+ +-+-+ + +-+-+ +
|     |     |   |
+ + + +-+-+ + + +
|   |     | | | |
+-+-+-+ + + + + +
|      G  | | | |
+-+ +-+ + + + +-+
|   |   | | |   |
+ +-+ + + + +-+ +
|   | |   | |   |
+-+ + +-+-+-+ + +
|   |         | |
+ +-+-+-+-+-+-+ +
|               |
+-+-+-+-+-+-+-+-+
```

## Generator rules

Deterministic for the same arguments (`random.Random(seed)`).

1. All walls up. The start is a leaf: randomized DFS (recursive backtracker) runs from one of its
   neighbours with the start pre-visited, then the start is joined to that neighbour (exactly 3 walls).
2. `--loops N`: open N random internal walls (never border, never a side of the start).
3. Goal: `--goal ROW,COL`, or the cell farthest from the start (BFS, ties go to the lowest cell index).
4. Every non-border side of the goal is opened, except a side shared with the start.

The outer border is always closed and every cell is reachable from the start.

## Usage

```bash
python3 Util/MazeTools/generate_maze.py --seed 1 --start 63 --goal 3,4 --loops 2 --out /tmp/m.maze
python3 Util/MazeTools/generate_maze.py --validate /tmp/m.maze   # prints OK or the first broken rule

mkdir -p Config/Mazes/generated
for s in $(seq 1 20); do python3 Util/MazeTools/generate_maze.py --seed $s --loops $((s % 4)) --out Config/Mazes/generated/maze_$s.maze; done
```

`--seed` is required when generating. `--start` is one of 0, 7, 56, 63 (default 0). Without `--out`
the maze is printed to stdout.

## Tests

```bash
python3 -m unittest discover -s Util/MazeTools -v
```
