"""Unit tests for generate_maze (run: python3 -m unittest discover -s Util/MazeTools -v)."""

import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_maze as gm  # noqa: E402

STARTS = gm.CORNER_CELLS


def grid_of(text):
    return gm.parse(text)


def find(text, ch):
    """Cell indexes (row * 8 + col) holding ch."""
    g = grid_of(text)
    return [r * gm.N + c for r in range(gm.N) for c in range(gm.N) if g[2 * r + 1][2 * c + 1] == ch]


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
        self.assertEqual(gm.generate(5, 0, 2), gm.generate(5, 0, 2))

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

    def test_goal_is_central_block(self):
        for seed in range(10):
            for start in STARTS:
                text = gm.generate(seed, start, seed % 4)
                self.assertEqual(find(text, "G"), [27, 28, 35, 36])
                self.assertEqual(find(text, "S"), [start])

    def test_no_walls_inside_block(self):
        for seed in range(10):
            g = grid_of(gm.generate(seed, 0, seed % 4))
            for line, col in ((7, 8), (9, 8), (8, 7), (8, 9)):  # between the 4 block cells
                self.assertEqual(g[line][col], " ", (line, col))

    def test_block_has_an_entrance(self):
        for seed in range(20):
            text = gm.generate(seed)
            g = grid_of(text)
            ring = [(6, 7), (6, 9), (10, 7), (10, 9), (7, 6), (9, 6), (7, 10), (9, 10)]
            self.assertTrue(any(g[i][j] == " " for i, j in ring), seed)

    def test_start_has_three_walls(self):
        for start in STARTS:
            for seed in range(10):
                g = grid_of(gm.generate(seed, start, 3))
                self.assertEqual(gm.count_walls(g, *divmod(start, 8)), 3)

    def test_all_cells_reachable(self):
        for start in STARTS:
            for seed in range(10):
                text = gm.generate(seed, start, seed % 4)
                self.assertEqual(len(gm.bfs(grid_of(text), divmod(start, 8))), 64)

    def test_loops_add_open_edges(self):
        # 61 nodes -> 60 tree edges, plus the 4 open walls inside the block.
        base = open_edges(gm.generate(4, 0, 0))
        self.assertEqual(base, 64)
        for loops in (1, 3, 10):
            self.assertEqual(open_edges(gm.generate(4, 0, loops)), base + loops)

    def test_loops_are_capped(self):
        # 112 internal walls - 64 open - 1 closed start side = 47 candidates.
        self.assertEqual(open_edges(gm.generate(4, 0, 1000)), 64 + 47)

    def test_invalid_start(self):
        with self.assertRaises(ValueError):
            gm.generate(1, start=5)


class ValidateTests(unittest.TestCase):
    def test_generated_mazes_are_valid(self):
        for seed in range(1, 21):
            for start in STARTS:
                for loops in range(4):
                    self.assertIsNone(gm.validate(gm.generate(seed, start, loops)))

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
        text = gm.generate(1)
        g = grid_of(text)
        # Pick a dead end (one open side) that is not the start or in the block.
        for cell in range(1, gm.N * gm.N):
            r, c = divmod(cell, gm.N)
            if cell in gm.GOAL_CELLS:
                continue
            opens = [(dr, dc) for dr, dc in gm.DIRS
                     if gm.in_range(r + dr, c + dc) and g[gm.wall_pos(r, c, dr, dc)[0]][gm.wall_pos(r, c, dr, dc)[1]] == " "]
            if len(opens) == 1:
                wr, wc = gm.wall_pos(r, c, *opens[0])
                text = replace_char(text, wr, wc, gm.wall_char((wr, wc)))
                break
        else:
            self.fail("no dead end found")
        self.assertEqual(gm.validate(text), "1 cells unreachable from start")

    def test_goal_missing_or_misplaced(self):
        text = gm.generate(1)
        no_goal = replace_char(text, 7, 7, " ")  # cell 27
        self.assertIn("G must be in exactly cells 27, 28, 35, 36", gm.validate(no_goal))
        moved = replace_char(replace_char(text, 7, 7, " "), 3, 5, "G")  # cell 2 instead of 27
        self.assertIn("G must be in exactly", gm.validate(moved))
        extra = replace_char(text, 3, 5, "G")
        self.assertIn("G must be in exactly", gm.validate(extra))

    def test_wall_inside_block(self):
        text = replace_char(gm.generate(1), 8, 9, "-")
        self.assertEqual(gm.validate(text), "wall inside goal block at line 8 col 9")

    def test_bad_chars(self):
        text = gm.generate(1)
        self.assertIn("invalid wall character", gm.validate(replace_char(text, 0, 1, "x")))
        self.assertIn("corner missing", gm.validate(replace_char(text, 0, 0, "-")))

    def test_crlf_is_tolerated(self):
        self.assertIsNone(gm.validate(gm.generate(1).replace("\n", "\r\n")))

    def test_start_not_in_corner(self):
        text = gm.generate(1)
        text = replace_char(text, 1, 1, " ")
        text = replace_char(text, 3, 3, "S")
        self.assertIn("not in a corner", gm.validate(text))


class CliTests(unittest.TestCase):
    def run_main(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = gm.main(argv)
        return code, out.getvalue(), err.getvalue()

    def test_validate_ok(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "sub", "m.maze")
            self.assertEqual(self.run_main(["--seed", "9", "--loops", "2", "--out", path])[0], 0)
            self.assertEqual(self.run_main(["--validate", path]), (0, "OK\n", ""))

    def test_validate_broken(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "bad.maze")
            with open(path, "w") as f:
                f.write("+-+\n")
            code, out, err = self.run_main(["--validate", path])
            self.assertEqual((code, out), (1, ""))
            self.assertEqual(len(err.strip().splitlines()), 1)

    def test_validate_non_utf8(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "bin.maze")
            with open(path, "wb") as f:
                f.write(b"\xff\xfe\x00\x80")
            code, out, err = self.run_main(["--validate", path])
            self.assertEqual((code, out), (1, ""))
            self.assertEqual(len(err.strip().splitlines()), 1)

    def test_validate_missing_file(self):
        code, out, err = self.run_main(["--validate", "/nonexistent/x.maze"])
        self.assertEqual((code, out), (1, ""))
        self.assertEqual(len(err.strip().splitlines()), 1)

    def test_stdout_generation_matches_function(self):
        code, out, _ = self.run_main(["--seed", "3", "--start", "63"])
        self.assertEqual((code, out), (0, gm.generate(3, 63)))

    def test_parser_errors(self):
        cases = ([], ["--seed", "1", "--goal", "0,0"], ["--validate", "x", "--seed", "1"],
                 ["--validate", "x", "--loops", "2"])
        for argv in cases:
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                gm.main(argv)


class CommittedMazesTests(unittest.TestCase):
    def test_generated_fixtures_validate(self):
        root = Path(__file__).resolve().parents[2]
        files = sorted((root / "Config" / "Mazes" / "generated").glob("*.maze"))
        self.assertEqual(len(files), 20)
        for path in files:
            self.assertIsNone(gm.validate(path.read_text(encoding="utf-8")), path.name)


if __name__ == "__main__":
    unittest.main()
