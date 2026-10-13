#!/usr/bin/env python3
"""Reference solver for 8x8 maze files (``.maze``).

Computes, for each maze, the optimal route on the full map and the simulated
exploration run of attempt 1 (only the border is known, edges are learned on
arrival). The rules are documented in README.md ("Reference solver") and must
be replicated exactly by the C++ planner (MZ-011).

State is ``(row, col, heading, moving)``; headings 0..3 are N, E, S, W.
Costs are integer milliseconds. The cost-to-go is a pair ``(time_ms, turns)``
compared lexicographically, computed by a backward Dijkstra from every goal
state. Standard library only.
"""

import argparse
import csv
import heapq
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import generate_maze as gm  # noqa: E402

HEAD_DELTAS = ((-1, 0), (0, 1), (1, 0), (0, -1))  # N, E, S, W
ARROWS = "^>v<"
FORWARD_MS = 2500
STANDING_START_MS = 800
TURN90_MS = 2000
TURN180_MS = 3500
INF = (1 << 60, 1 << 60)
STEP_GUARD = 10000
CSV_HEADER = ["file", "maze_sha", "cells", "turns90", "turns180", "time_s",
              "explore_cells", "explore_turns", "explore_time_s"]


def cell_index(r, c):
    return r * gm.N + c


def fnv1a32(data):
    """FNV-1a 32-bit hash of bytes, as 8 lowercase hex digits."""
    h = 0x811C9DC5
    for byte in data:
        h = ((h ^ byte) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def fmt_s(ms):
    return f"{ms // 1000}.{ms % 1000:03d}"


def edge_pos(r, c, h):
    return gm.wall_pos(r, c, *HEAD_DELTAS[h])


def actual_open(grid):
    """is_open(r, c, h) for the real map."""
    return lambda r, c, h: grid[edge_pos(r, c, h)[0]][edge_pos(r, c, h)[1]] == " "


def optimistic_open(grid, known):
    """is_open(r, c, h) where edges not yet in ``known`` count as open (border is known closed)."""
    def is_open(r, c, h):
        pos = edge_pos(r, c, h)
        if gm.is_border(pos):
            return False
        return grid[pos[0]][pos[1]] == " " if pos in known else True
    return is_open


def is_goal(state):
    return cell_index(state[0], state[1]) in gm.GOAL_CELLS


def actions(state, is_open):
    """Legal actions in tie-break order: (name, next_state, ms, turns)."""
    r, c, h, moving = state
    if is_open(r, c, h):
        dr, dc = HEAD_DELTAS[h]
        ms = FORWARD_MS + (0 if moving else STANDING_START_MS)
        yield "forward", (r + dr, c + dc, h, 1), ms, 0
    yield "left", (r, c, (h + 3) % 4, 0), TURN90_MS, 1
    yield "right", (r, c, (h + 1) % 4, 0), TURN90_MS, 1
    yield "around", (r, c, (h + 2) % 4, 0), TURN180_MS, 1


def all_states():
    return [(r, c, h, m) for r in range(gm.N) for c in range(gm.N) for h in range(4) for m in (0, 1)]


def compute_ctg(is_open):
    """Backward Dijkstra from all goal states. Returns {state: (time_ms, turns)}."""
    reverse = {}
    for s in all_states():
        if is_goal(s):
            continue
        for _, nxt, ms, turns in actions(s, is_open):
            reverse.setdefault(nxt, []).append((s, ms, turns))
    ctg = {}
    heap = []
    for s in all_states():
        if is_goal(s):
            ctg[s] = (0, 0)
            heap.append(((0, 0), s))
    heapq.heapify(heap)
    while heap:
        value, s = heapq.heappop(heap)
        if value > ctg.get(s, INF):
            continue
        for prev, ms, turns in reverse.get(s, ()):
            cand = (value[0] + ms, value[1] + turns)
            if cand < ctg.get(prev, INF):
                ctg[prev] = cand
                heapq.heappush(heap, (cand, prev))
    return ctg


def choose_action(state, ctg, is_open):
    """First minimum of (action + ctg(next)) in action order, or None if all are unreachable."""
    best, best_value = None, INF
    for name, nxt, ms, turns in actions(state, is_open):
        to_go = ctg(nxt)
        if to_go == INF:
            continue
        value = (ms + to_go[0], turns + to_go[1])
        if value < best_value:
            best, best_value = (name, nxt, ms, turns), value
    return best


class Run:
    """Accumulates the result of a simulated run."""

    def __init__(self, start):
        self.state = start
        self.ms = 0
        self.cells = 0
        self.turns90 = 0
        self.turns180 = 0
        self.leaves = []  # (cell, heading) for every forward action
        self.visited = [cell_index(start[0], start[1])]

    def apply(self, name, nxt, ms):
        if name == "forward":
            self.cells += 1
            self.leaves.append((cell_index(self.state[0], self.state[1]), self.state[2]))
            self.visited.append(cell_index(nxt[0], nxt[1]))
        elif name == "around":
            self.turns180 += 1
        else:
            self.turns90 += 1
        self.ms += ms
        self.state = nxt

    @property
    def turns(self):
        return self.turns90 + self.turns180


def start_state(grid):
    """Start cell, moving 0, heading = the single open side of the start corner."""
    for r in range(gm.N):
        for c in range(gm.N):
            if grid[2 * r + 1][2 * c + 1] == "S":
                opened = [h for h in range(4) if actual_open(grid)(r, c, h)]
                if len(opened) != 1:
                    raise ValueError(f"start has {len(opened)} open sides, expected 1")
                return r, c, opened[0], 0
    raise ValueError("no start cell")


def optimal_run(grid):
    """Follow the action rule on the full map. Returns (run, ctg(start))."""
    is_open = actual_open(grid)
    ctg = compute_ctg(is_open)
    start = start_state(grid)
    run = Run(start)
    while not is_goal(run.state):
        if run.cells + run.turns > STEP_GUARD:
            raise RuntimeError("step guard exceeded")
        choice = choose_action(run.state, lambda s: ctg.get(s, INF), is_open)
        if choice is None:
            raise RuntimeError("goal unreachable")
        run.apply(choice[0], choice[1], choice[2])
    return run, ctg[start]


def learn(grid, known, r, c):
    for h in range(4):
        known.add(edge_pos(r, c, h))


def explore_run(grid):
    """Attempt 1: border known, edges learned on arrival, replan before every action."""
    real_open = actual_open(grid)
    known = set()
    run = Run(start_state(grid))
    learn(grid, known, run.state[0], run.state[1])
    steps = 0
    while not is_goal(run.state):
        steps += 1
        if steps > STEP_GUARD:
            raise RuntimeError("step guard exceeded")
        is_open = optimistic_open(grid, known)
        ctg = compute_ctg(is_open)
        choice = choose_action(run.state, lambda s: ctg.get(s, INF), is_open)
        if choice is None:
            raise RuntimeError("goal unreachable")
        name, nxt, ms, _ = choice
        if name == "forward" and not real_open(run.state[0], run.state[1], run.state[2]):
            raise RuntimeError("forward through a closed edge")
        run.apply(name, nxt, ms)
        if name == "forward":
            learn(grid, known, nxt[0], nxt[1])
    return run


def draw(grid, marks):
    """Return the maze text with ``marks`` {cell_index: char} drawn in the cell characters."""
    out = [list(row) for row in grid]
    for cell, ch in marks.items():
        r, c = divmod(cell, gm.N)
        out[2 * r + 1][2 * c + 1] = ch
    return "\n".join("".join(row) for row in out)


def route_marks(run):
    marks = {cell: ARROWS[h] for cell, h in run.leaves}
    marks[cell_index(run.state[0], run.state[1])] = ARROWS[run.state[2]]
    return marks


def solve(grid):
    """Solve one maze grid. Returns (optimal run, exploration run)."""
    optimal, to_go = optimal_run(grid)
    if optimal.ms != to_go[0]:
        raise RuntimeError(f"route time {optimal.ms} != cost-to-go {to_go[0]}")
    return optimal, explore_run(grid)


def csv_row(name, data, optimal, explore):
    return {
        "file": name, "maze_sha": fnv1a32(data),
        "cells": str(optimal.cells), "turns90": str(optimal.turns90), "turns180": str(optimal.turns180),
        "time_s": fmt_s(optimal.ms),
        "explore_cells": str(explore.cells), "explore_turns": str(explore.turns),
        "explore_time_s": fmt_s(explore.ms),
    }


def update_csv(path, rows):
    """Upsert rows keyed by ``file``; sorted by file; ``\\n`` line endings."""
    path = Path(path)
    merged = {}
    if path.exists():
        with path.open(newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            if reader.fieldnames != CSV_HEADER:
                raise ValueError(f"{path}: unexpected CSV header")
            for row in reader:
                merged[row["file"]] = row
    for row in rows:
        merged[row["file"]] = row
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_HEADER, lineterminator="\n")
        writer.writeheader()
        for name in sorted(merged):
            writer.writerow(merged[name])


def build_parser():
    p = argparse.ArgumentParser(description="Reference solver: optimal route and exploration run for .maze files.")
    p.add_argument("mazes", nargs="+", metavar="MAZE", help=".maze files to solve")
    p.add_argument("--draw", action="store_true", help="draw the optimal route and the visited cells")
    p.add_argument("--update", metavar="CSV", help="upsert the results into this CSV (keyed by file name)")
    return p


def load(path):
    """Return (data, grid) or raise ValueError with a short message."""
    try:
        data = Path(path).read_bytes()
        text = data.decode("utf-8")
    except OSError as exc:
        raise ValueError(f"cannot read: {exc.strerror}")
    except UnicodeDecodeError:
        raise ValueError("cannot read: not valid UTF-8 text")
    error = gm.validate(text)
    if error is not None:
        raise ValueError(error)
    return data, gm.parse(text)


def main(argv=None):
    args = build_parser().parse_args(argv)
    loaded = []
    failed = False
    for path in args.mazes:
        try:
            loaded.append((path, *load(path)))
        except ValueError as exc:
            print(f"{path}: {exc}", file=sys.stderr)
            failed = True
    if failed:
        return 1
    rows = []
    for path, data, grid in loaded:
        name = Path(path).name
        optimal, explore = solve(grid)
        print(f"{name}: optimal {optimal.cells} cells, {optimal.turns90} turns90, {optimal.turns180} turns180, "
              f"{fmt_s(optimal.ms)} s; explore {explore.cells} cells, {explore.turns} turns, {fmt_s(explore.ms)} s")
        if args.draw:
            print("optimal route:")
            print(draw(grid, route_marks(optimal)))
            print("exploration (visited cells):")
            print(draw(grid, {cell: "*" for cell in explore.visited}))
        rows.append(csv_row(name, data, optimal, explore))
    if args.update:
        try:
            update_csv(args.update, rows)
        except (ValueError, OSError) as exc:
            print(f"{args.update}: {exc}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
