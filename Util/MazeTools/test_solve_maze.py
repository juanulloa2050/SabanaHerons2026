"""Unit tests for solve_maze (run: python3 -m unittest discover -s Util/MazeTools -v)."""

import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_maze as gm  # noqa: E402
import solve_maze as sm  # noqa: E402

MAZE_DIR = Path(__file__).resolve().parents[2] / "Config" / "Mazes" / "generated"
MAZES = [MAZE_DIR / f"maze_{i}.maze" for i in range(1, 21)]


def read(path):
    return Path(path).read_bytes()


def grid_of(path):
    return gm.parse(read(path).decode("utf-8"))


def rotate90(grid):
    """Rotate the character grid 90 degrees clockwise, swapping wall orientation."""
    swap = {"-": "|", "|": "-"}
    size = gm.SIZE
    return ["".join(swap.get(grid[size - 1 - j][i], grid[size - 1 - j][i]) for j in range(size))
            for i in range(size)]


def run_cli(args):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = sm.main(args)
    return code, out.getvalue(), err.getvalue()


class HashTests(unittest.TestCase):
    def test_known_vectors(self):
        self.assertEqual(sm.fnv1a32(b""), "811c9dc5")
        self.assertEqual(sm.fnv1a32(b"a"), "e40c292c")

    def test_editing_bytes_changes_sha(self):
        data = read(MAZES[2])
        edited = data.replace(b"S", b" ", 1)
        self.assertNotEqual(sm.fnv1a32(data), sm.fnv1a32(edited))

    def test_fmt_s(self):
        self.assertEqual(sm.fmt_s(21400), "21.400")
        self.assertEqual(sm.fmt_s(5), "0.005")


class ChoiceTests(unittest.TestCase):
    ALL_OPEN = staticmethod(lambda r, c, h: True)

    def test_left_beats_right_on_tie(self):
        state = (0, 0, 1, 1)
        table = {(0, 0, 0, 0): (500, 0), (0, 0, 2, 0): (500, 0)}
        choice = sm.choose_action(state, lambda s: table.get(s, sm.INF), self.ALL_OPEN)
        self.assertEqual(choice[0], "left")

    def test_forward_beats_turn_on_tie(self):
        state = (0, 0, 1, 1)
        table = {(0, 1, 1, 1): (0, 1), (0, 0, 0, 0): (500, 0)}
        choice = sm.choose_action(state, lambda s: table.get(s, sm.INF), self.ALL_OPEN)
        self.assertEqual(choice[0], "forward")

    def test_fewer_turns_break_time_tie(self):
        state = (0, 0, 1, 1)
        table = {(0, 1, 1, 1): (0, 3), (0, 0, 0, 0): (500, 0)}  # forward ties on time, loses on turns
        choice = sm.choose_action(state, lambda s: table.get(s, sm.INF), self.ALL_OPEN)
        self.assertEqual(choice[0], "left")

    def test_closed_edge_forbids_forward(self):
        names = [a[0] for a in sm.actions((0, 0, 0, 0), lambda r, c, h: False)]
        self.assertEqual(names, ["left", "right", "around"])

    def test_standing_forward_costs_extra(self):
        forward = next(iter(sm.actions((3, 3, 1, 0), self.ALL_OPEN)))
        self.assertEqual(forward[2], 3300)
        forward = next(iter(sm.actions((3, 3, 1, 1), self.ALL_OPEN)))
        self.assertEqual(forward[2], 2500)


class SolveTests(unittest.TestCase):
    def test_maze_3_optimal(self):
        optimal, _ = sm.solve(grid_of(MAZES[2]))
        self.assertEqual((optimal.cells, optimal.turns90, optimal.turns180, optimal.ms), (6, 2, 0, 21400))
        self.assertEqual(sm.fmt_s(optimal.ms), "21.400")

    def test_all_generated_mazes(self):
        for path in MAZES:
            with self.subTest(maze=path.name):
                grid = grid_of(path)
                optimal, to_go = sm.optimal_run(grid)
                self.assertEqual(optimal.ms, to_go[0])
                explore = sm.explore_run(grid)
                self.assertIn(explore.visited[-1], gm.GOAL_CELLS)
                self.assertIn(optimal.visited[-1], gm.GOAL_CELLS)
                self.assertGreaterEqual(explore.ms, optimal.ms)
                is_open = sm.actual_open(grid)
                self.assertTrue(all(is_open(*divmod(cell, gm.N), h) for cell, h in explore.leaves))

    def test_rotation_keeps_optimum(self):
        for path in MAZES[:6]:
            with self.subTest(maze=path.name):
                grid = grid_of(path)
                rotated = rotate90(grid)
                self.assertIsNone(gm.validate("\n".join(rotated) + "\n"))
                a, _ = sm.optimal_run(grid)
                b, _ = sm.optimal_run(rotated)
                self.assertEqual((a.ms, a.cells, a.turns90, a.turns180), (b.ms, b.cells, b.turns90, b.turns180))


class CliTests(unittest.TestCase):
    def test_summary_line(self):
        code, out, _ = run_cli([str(MAZES[2])])
        self.assertEqual(code, 0)
        self.assertTrue(out.startswith("maze_3.maze: optimal 6 cells, 2 turns90, 0 turns180, 21.400 s; explore "))

    def test_draw_arrow_count(self):
        code, out, _ = run_cli([str(MAZES[2]), "--draw"])
        self.assertEqual(code, 0)
        head, rest = out.split("optimal route:\n")
        route, expl = rest.split("exploration (visited cells):\n")
        self.assertEqual(sum(route.count(a) for a in sm.ARROWS), 6 + 1)
        explore = sm.explore_run(grid_of(MAZES[2]))
        self.assertEqual(expl.count("*"), len(set(explore.visited)))

    def test_update_is_idempotent_and_sorted(self):
        with tempfile.TemporaryDirectory() as tmp:
            csv_path = Path(tmp) / "expected.csv"
            self.assertEqual(run_cli([str(MAZES[2]), "--update", str(csv_path)])[0], 0)
            self.assertEqual(run_cli([str(MAZES[0]), "--update", str(csv_path)])[0], 0)
            first = csv_path.read_bytes()
            self.assertEqual(run_cli([str(MAZES[2]), "--update", str(csv_path)])[0], 0)
            self.assertEqual(csv_path.read_bytes(), first)
            lines = first.decode().split("\n")
            self.assertEqual(lines[0], ",".join(sm.CSV_HEADER))
            self.assertEqual([l.split(",")[0] for l in lines[1:] if l], ["maze_1.maze", "maze_3.maze"])
            self.assertNotIn(b"\r", first)
            self.assertEqual(sum(l.startswith("maze_3.maze,") for l in lines), 1)

    def test_update_rejects_foreign_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            csv_path = Path(tmp) / "bad.csv"
            csv_path.write_text("a,b\n1,2\n")
            code, _, err = run_cli([str(MAZES[2]), "--update", str(csv_path)])
            self.assertEqual(code, 1)
            self.assertIn("header", err)

    def test_invalid_maze_exits_1_without_csv(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "bad.maze"
            bad.write_bytes(read(MAZES[2]).replace(b"S", b"G", 1))
            csv_path = Path(tmp) / "out.csv"
            code, out, err = run_cli([str(MAZES[0]), str(bad), "--update", str(csv_path)])
            self.assertEqual(code, 1)
            self.assertIn("bad.maze:", err)
            self.assertFalse(csv_path.exists())

    def test_missing_file_exits_1(self):
        code, _, err = run_cli(["/nonexistent/none.maze"])
        self.assertEqual(code, 1)
        self.assertIn("none.maze:", err)


if __name__ == "__main__":
    unittest.main()
