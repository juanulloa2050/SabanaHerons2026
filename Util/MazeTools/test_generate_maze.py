"""Unit tests for generate_maze (run: python3 -m unittest discover -s Util/MazeTools -v)."""

import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_maze as gm  # noqa: E402

STARTS = gm.CORNER_CELLS


def grid_of(text):
    return gm.parse(text)


def find(text, ch):
    for r in range(gm.N):
        for c in range(gm.N):
            if grid_of(text)[2 * r + 1][2 * c + 1] == ch:
                return r, c
    raise AssertionError(ch)


def open_edges(text):
    """Count open internal walls (edges between two cells)."""
    g = grid_of(text)
    n = 0
    for i in range(1, gm.SIZE - 1):
        for j in range(1, gm.SIZE - 1):
            if i % 2 != j % 2 and g[i][j] == " ":
                n += 1
    return n


def replace_char(text, line, col, ch):
    lines = text.split("\n")
    lines[line] = lines[line][:col] + ch + lines[line][col + 1:]
    return "\n".join(lines)


class GenerateTests(unittest.TestCase):
    def test_same_seed_same_text(self):
        self.assertEqual(gm.generate(5, 0, None, 2), gm.generate(5, 0, None, 2))

    def test_different_seeds_differ(self):
        self.assertNotEqual(gm.generate(1), gm.generate(2))

    def test_shape(self):
        lines = grid_of(gm.generate(3))
        self.assertEqual(len(lines), 17)
        self.assertTrue(all(len(line) == 17 for line in lines))
        self.assertTrue(gm.generate(3).endswith("\n"))

    def test_border_closed(self):
        for start in STARTS:
            g = grid_of(gm.generate(7, start))
            for k in range(gm.SIZE):
                for pos in ((0, k), (16, k), (k, 0), (k, 16)):
                    self.assertNotEqual(g[pos[0]][pos[1]], " ", pos)

    def test_start_has_three_walls(self):
        for start in STARTS:
            for seed in range(10):
                text = gm.generate(seed, start, None, 3)
                g = grid_of(text)
                self.assertEqual(gm.count_walls(g, *find(text, "S")), 3)
                self.assertEqual(find(text, "S"), divmod(start, 8))

    def test_all_cells_reachable(self):
        for seed in range(10):
            text = gm.generate(seed, 63, None, seed % 4)
            self.assertEqual(len(gm.bfs(grid_of(text), find(text, "S"))), 64)

    def test_goal_has_no_internal_walls(self):
        for seed in range(10):
            text = gm.generate(seed, 0, (0, 1), 2)  # goal next to start
            self.assertIsNone(gm.validate(text))
            text = gm.generate(seed, 7, (4, 4), 1)
            g = grid_of(text)
            r, c = find(text, "G")
            for dr, dc in gm.DIRS:
                if gm.in_range(r + dr, c + dc):
                    wr, wc = gm.wall_pos(r, c, dr, dc)
                    self.assertEqual(g[wr][wc], " ")

    def test_default_goal_is_farthest(self):
        # Farthest is measured before the goal sides are opened (generator step c).
        for seed in range(5):
            for start in STARTS:
                dist = gm.bfs(gm.build_walls(seed, start, seed % 3), divmod(start, 8))
                best = min(dist, key=lambda cell: (-dist[cell], cell[0] * 8 + cell[1]))
                self.assertEqual(find(gm.generate(seed, start, None, seed % 3), "G"), best)

    def test_loops_create_cycles(self):
        # Before the goal is opened, loops=0 is a perfect maze (a spanning tree).
        self.assertEqual(open_edges("\n".join("".join(r) for r in gm.build_walls(4, 0, 0))), 63)
        self.assertEqual(open_edges("\n".join("".join(r) for r in gm.build_walls(4, 0, 3))), 66)
        self.assertGreater(open_edges(gm.generate(4, 0, None, 3)), 63)

    def test_invalid_args(self):
        with self.assertRaises(ValueError):
            gm.generate(1, start=5)
        with self.assertRaises(ValueError):
            gm.generate(1, start=0, goal=(0, 0))
        with self.assertRaises(ValueError):
            gm.generate(1, goal=(8, 0))


class ValidateTests(unittest.TestCase):
    def test_generated_mazes_are_valid(self):
        for seed in range(1, 21):
            for start in STARTS:
                for loops in range(4):
                    self.assertIsNone(gm.validate(gm.generate(seed, start, None, loops)))

    def test_ticket_example_shape_is_rejected(self):
        self.assertIn("expected 17 lines", gm.validate("+-+-+\n|S  |\n+ +-+\n"))

    def test_bad_line_length(self):
        lines = gm.generate(1).split("\n")
        lines[3] = lines[3][:-1]
        self.assertIn("line 3 has length 16", gm.validate("\n".join(lines)))

    def test_open_border(self):
        text = replace_char(gm.generate(1), 0, 5, " ")
        self.assertEqual(gm.validate(text), "border open at line 0 col 5")

    def test_start_with_two_walls(self):
        text = gm.generate(1)  # start (0,0): open the wall that is still closed
        g = grid_of(text)
        for line, col in ((2, 1), (1, 2)):
            if g[line][col] != " ":
                text = replace_char(text, line, col, " ")
        self.assertEqual(gm.validate(text), "start has 2 walls, expected 3")

    def test_unreachable_cell(self):
        text = gm.generate(1, 0, None, 0)
        # Close every side of the cell (3,3) (goal-free in seed 1 is not guaranteed).
        r, c = 3, 3
        if find(text, "G") == (r, c):
            r, c = 4, 4
        for dr, dc in gm.DIRS:
            wr, wc = gm.wall_pos(r, c, dr, dc)
            text = replace_char(text, wr, wc, gm.wall_char((wr, wc)))
        self.assertEqual(gm.validate(text), "1 cells unreachable from start")

    def test_missing_goal_and_bad_chars(self):
        text = gm.generate(1)
        gr, gc = find(text, "G")
        no_goal = replace_char(text, 2 * gr + 1, 2 * gc + 1, " ")
        self.assertEqual(gm.validate(no_goal), "expected exactly one G, found 0")
        self.assertIn("invalid wall character", gm.validate(replace_char(text, 0, 1, "x")))
        self.assertIn("corner missing", gm.validate(replace_char(text, 0, 0, "-")))

    def test_start_not_in_corner(self):
        text = gm.generate(1)
        text = replace_char(text, 1, 1, " ")
        text = replace_char(text, 3, 3, "S")
        self.assertIn("not in a corner", gm.validate(text))


class CliTests(unittest.TestCase):
    def run_main(self, argv):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            code = gm.main(argv)
        return code, buf.getvalue()

    def test_validate_ok(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "sub", "m.maze")
            self.assertEqual(self.run_main(["--seed", "9", "--loops", "2", "--out", path])[0], 0)
            self.assertEqual(self.run_main(["--validate", path]), (0, "OK\n"))

    def test_validate_broken(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "bad.maze")
            with open(path, "w") as f:
                f.write("+-+\n")
            code, out = self.run_main(["--validate", path])
            self.assertEqual(code, 1)
            self.assertEqual(len(out.strip().splitlines()), 1)

    def test_stdout_generation_matches_function(self):
        code, out = self.run_main(["--seed", "3", "--start", "63"])
        self.assertEqual((code, out), (0, gm.generate(3, 63)))

    def test_seed_required_and_goal_checks(self):
        for argv in ([], ["--seed", "1", "--goal", "0,0"], ["--seed", "1", "--goal", "9,9"]):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                gm.main(argv)


if __name__ == "__main__":
    unittest.main()
