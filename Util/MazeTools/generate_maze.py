#!/usr/bin/env python3
"""Generate and validate 8x8 maze files (``.maze``).

Format: 17 lines of 17 characters. The character at (2r+1, 2c+1) is cell
(r, c) and holds ``S`` (start), ``G`` (goal) or a space; row 0 is the top row.
Even/even positions are corners ``+``. (even row, odd col) is a horizontal wall
(``-`` or space), (odd row, even col) is a vertical wall (``|`` or space).
Cell index = row * 8 + col.

Generation is deterministic for the same arguments (``random.Random(seed)``).
Standard library only.
"""

import argparse
import random
import sys
from collections import deque
from pathlib import Path

N = 8
SIZE = 2 * N + 1
CORNER_CELLS = (0, 7, 56, 63)
DIRS = ((-1, 0), (1, 0), (0, -1), (0, 1))  # N, S, W, E


def cell_pos(r, c):
    """Grid position (line, column) of the centre of cell (r, c)."""
    return 2 * r + 1, 2 * c + 1


def wall_pos(r, c, dr, dc):
    """Grid position of the wall on side (dr, dc) of cell (r, c)."""
    return 2 * r + 1 + dr, 2 * c + 1 + dc


def is_border(pos):
    return pos[0] in (0, SIZE - 1) or pos[1] in (0, SIZE - 1)


def in_range(r, c):
    return 0 <= r < N and 0 <= c < N


def wall_char(pos):
    return "-" if pos[0] % 2 == 0 else "|"


def parse(text):
    """Split maze text into a list of lines (no validation)."""
    return text.rstrip("\n").split("\n")


def open_neighbours(grid, r, c):
    """Yield neighbouring cells (r, c) connected without a wall."""
    for dr, dc in DIRS:
        nr, nc = r + dr, c + dc
        if in_range(nr, nc):
            wr, wc = wall_pos(r, c, dr, dc)
            if grid[wr][wc] == " ":
                yield nr, nc


def bfs(grid, start):
    """Return {(r, c): distance} for all cells reachable from start."""
    dist = {start: 0}
    queue = deque([start])
    while queue:
        cur = queue.popleft()
        for nxt in open_neighbours(grid, *cur):
            if nxt not in dist:
                dist[nxt] = dist[cur] + 1
                queue.append(nxt)
    return dist


def count_walls(grid, r, c):
    return sum(grid[wall_pos(r, c, dr, dc)[0]][wall_pos(r, c, dr, dc)[1]] != " " for dr, dc in DIRS)


def validate(text):
    """Return None if the maze is valid, else a short message for the first broken rule."""
    lines = parse(text)
    if len(lines) != SIZE:
        return f"expected {SIZE} lines, found {len(lines)}"
    for i, line in enumerate(lines):
        if len(line) != SIZE:
            return f"line {i} has length {len(line)}, expected {SIZE}"
    for i in range(0, SIZE, 2):
        for j in range(0, SIZE, 2):
            if lines[i][j] != "+":
                return f"corner missing at line {i} col {j}"
    for i in range(SIZE):
        for j in range(SIZE):
            if i % 2 == j % 2:
                continue
            allowed = "- " if i % 2 == 0 else "| "
            if lines[i][j] not in allowed:
                return f"invalid wall character {lines[i][j]!r} at line {i} col {j}"
    starts, goals = [], []
    for r in range(N):
        for c in range(N):
            ch = lines[2 * r + 1][2 * c + 1]
            if ch not in "SG ":
                return f"invalid cell character {ch!r} at cell ({r},{c})"
            if ch == "S":
                starts.append((r, c))
            elif ch == "G":
                goals.append((r, c))
    if len(starts) != 1:
        return f"expected exactly one S, found {len(starts)}"
    if len(goals) != 1:
        return f"expected exactly one G, found {len(goals)}"
    start, goal = starts[0], goals[0]
    if start[0] * N + start[1] not in CORNER_CELLS:
        return f"start {start} is not in a corner cell"
    for i in range(SIZE):
        for j in range(SIZE):
            if is_border((i, j)) and i % 2 != j % 2 and lines[i][j] == " ":
                return f"border open at line {i} col {j}"
    walls = count_walls(lines, *start)
    if walls != 3:
        return f"start has {walls} walls, expected 3"
    for dr, dc in DIRS:
        nr, nc = goal[0] + dr, goal[1] + dc
        wr, wc = wall_pos(*goal, dr, dc)
        if in_range(nr, nc) and (nr, nc) != start and lines[wr][wc] != " ":
            return f"goal has internal wall at line {wr} col {wc}"
    unreachable = N * N - len(bfs(lines, start))
    if unreachable:
        return f"{unreachable} cells unreachable from start"
    return None


def open_wall(grid, cell, dr, dc):
    wr, wc = wall_pos(*cell, dr, dc)
    grid[wr][wc] = " "


def build_walls(seed, start, loops):
    """Steps a-b: grid with the start leaf and extra loops, before the goal is chosen."""
    rng = random.Random(seed)
    grid = [[" "] * SIZE for _ in range(SIZE)]
    for i in range(SIZE):
        for j in range(SIZE):
            if i % 2 == 0 and j % 2 == 0:
                grid[i][j] = "+"
            elif i % 2 != j % 2:
                grid[i][j] = wall_char((i, j))
    s = divmod(start, N)

    # a. Recursive backtracker; the start is pre-visited so it stays a leaf.
    first_options = [(s[0] + dr, s[1] + dc) for dr, dc in DIRS if in_range(s[0] + dr, s[1] + dc)]
    first = rng.choice(first_options)
    visited = {s, first}
    stack = [first]
    while stack:
        cur = stack[-1]
        options = [(dr, dc) for dr, dc in DIRS
                   if in_range(cur[0] + dr, cur[1] + dc) and (cur[0] + dr, cur[1] + dc) not in visited]
        if not options:
            stack.pop()
            continue
        dr, dc = rng.choice(options)
        nxt = (cur[0] + dr, cur[1] + dc)
        open_wall(grid, cur, dr, dc)
        visited.add(nxt)
        stack.append(nxt)
    open_wall(grid, s, first[0] - s[0], first[1] - s[1])

    # b. Loops: open random closed internal walls that are not sides of the start.
    start_sides = {wall_pos(*s, dr, dc) for dr, dc in DIRS}
    candidates = [(i, j) for i in range(SIZE) for j in range(SIZE)
                  if i % 2 != j % 2 and not is_border((i, j))
                  and grid[i][j] != " " and (i, j) not in start_sides]
    for i, j in rng.sample(candidates, min(loops, len(candidates))):
        grid[i][j] = " "
    return grid


def generate(seed, start=0, goal=None, loops=0):
    """Return the text of a maze for the given parameters."""
    if start not in CORNER_CELLS:
        raise ValueError(f"start must be one of {CORNER_CELLS}")
    if goal is not None:
        if not in_range(*goal):
            raise ValueError(f"goal {goal} out of range")
        if goal == divmod(start, N):
            raise ValueError("goal must differ from start")
    grid = build_walls(seed, start, loops)
    s = divmod(start, N)

    # c. Goal: given, or farthest from start (ties -> lowest cell index).
    if goal is None:
        dist = bfs(grid, s)
        goal = min(dist, key=lambda cell: (-dist[cell], cell[0] * N + cell[1]))

    # d. Open every non-border side of the goal except a side shared with the start.
    for dr, dc in DIRS:
        nb = (goal[0] + dr, goal[1] + dc)
        if in_range(*nb) and nb != s:
            open_wall(grid, goal, dr, dc)

    grid[2 * s[0] + 1][2 * s[1] + 1] = "S"
    grid[2 * goal[0] + 1][2 * goal[1] + 1] = "G"
    return "\n".join("".join(row) for row in grid) + "\n"


def build_parser():
    p = argparse.ArgumentParser(description="Generate or validate an 8x8 .maze file.")
    p.add_argument("--seed", type=int, help="random seed (required when generating)")
    p.add_argument("--start", type=int, choices=CORNER_CELLS, default=0, help="start cell index (default 0)")
    p.add_argument("--goal", help="goal as ROW,COL (default: farthest cell from start)")
    p.add_argument("--loops", type=int, default=0, help="extra internal walls to remove (default 0)")
    p.add_argument("--out", help="output file (default: stdout)")
    p.add_argument("--validate", metavar="PATH", help="validate a .maze file instead of generating")
    return p


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.validate is not None:
        if args.seed is not None or args.goal is not None or args.out is not None:
            parser.error("--validate cannot be combined with generation options")
        try:
            text = Path(args.validate).read_text()
        except OSError as exc:
            print(f"cannot read {args.validate}: {exc.strerror}")
            return 1
        error = validate(text)
        print("OK" if error is None else error)
        return 0 if error is None else 1
    if args.seed is None:
        parser.error("--seed is required when generating")
    if args.loops < 0:
        parser.error("--loops must be >= 0")
    goal = None
    if args.goal is not None:
        try:
            row, col = (int(v) for v in args.goal.split(","))
        except ValueError:
            parser.error("--goal must be ROW,COL")
        goal = (row, col)
        if not in_range(row, col):
            parser.error("--goal out of range (0..7)")
        if goal == divmod(args.start, N):
            parser.error("--goal must differ from the start cell")
    text = generate(args.seed, args.start, goal, args.loops)
    if args.out:
        out = Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
