"""Unit tests for maze_to_scene (run: python3 -m unittest discover -s Util/MazeTools -v)."""

import contextlib
import io
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_maze as gm  # noqa: E402
import maze_to_scene as ms  # noqa: E402

BOX = re.compile(r'<BoxGeometry depth="([^"]+)" width="([^"]+)" height="([^"]+)" color="([^"]+)">\s*'
                 r'<Translation x="([^"]+)" y="([^"]+)" z="([^"]+)"/>')
APPEARANCE = re.compile(r'<BoxAppearance depth="([^"]+)" width="([^"]+)" height="([^"]+)">\s*'
                        r'<Surface ref="mazeWall"/>\s*'
                        r'<Translation x="([^"]+)" y="([^"]+)" z="([^"]+)"/>')
ROBOT = re.compile(r'<Translation x="([^"]+)" y="([^"]+)" z="320mm"/>\s*<Rotation z="(-?\d+)degree"/>')


def boxes(scene):
    return BOX.findall(scene)


def robot(scene):
    x, y, heading = ROBOT.search(scene).groups()
    return x, y, int(heading)


def surface_def(name):
    return re.search(rf'<Surface name="{name}" diffuseColor="([^"]+)"', ms.convert(gm.generate(1), "m.maze")).group(1)


class ConvertTest(unittest.TestCase):
    def test_one_box_per_wall_character(self):
        for seed in (1, 2, 3):
            text = gm.generate(seed, loops=seed)
            walls = sum(line.count("-") + line.count("|") for line in text.splitlines())
            self.assertEqual(len(boxes(ms.convert(text, "m.maze"))), walls)

    def test_each_wall_has_matching_appearance(self):
        for seed in (1, 2, 3):
            text = gm.generate(seed, loops=seed)
            wall_chars = sum(line.count("-") + line.count("|") for line in text.splitlines())
            scene = ms.convert(text, "m.maze")
            appearances = APPEARANCE.findall(scene)
            self.assertEqual(len(appearances), wall_chars)
            self.assertEqual(len(boxes(scene)), wall_chars)
            for (d, w, h, x, y, z), (gd, gw, gh, _, gx, gy, gz) in zip(appearances, boxes(scene)):
                self.assertEqual((d, w, h, x, y, z), (gd, gw, gh, gx, gy, gz))

    def test_wall_surface_defined_once(self):
        scene = ms.convert(gm.generate(1), "m.maze")
        self.assertEqual(scene.count('<Surface name="mazeWall"'), 1)
        self.assertLess(scene.index('<Surface name="mazeWall"'), scene.index('<Compound name="maze">'))

    def test_floor_slab_geometry_and_appearance(self):
        scene = ms.convert(gm.generate(1), "m.maze")
        floor = scene[scene.index('<Compound name="mazeFloor">'):scene.index('<Compound name="mazeRobots">')]
        size = 'depth="6m" width="6m" height="0.2m"'
        self.assertEqual(floor.count(f"<BoxAppearance {size}>"), 1)
        self.assertEqual(floor.count(f"<BoxGeometry {size} "), 1)
        self.assertEqual(floor.count('<Translation z="-0.1m"/>'), 2)  # top surface at z = 0
        self.assertIn(f'<Surface ref="{ms.FLOOR_SURFACE_NAME}"/>', floor)

    def test_goal_patch_is_appearance_only_and_centred(self):
        scene = ms.convert(gm.generate(1), "m.maze")
        floor = scene[scene.index('<Compound name="mazeFloor">'):scene.index('<Compound name="mazeRobots">')]
        patch = re.findall(r'<BoxAppearance depth="1m" width="1m" height="0.002m">\s*'
                           rf'<Surface ref="{ms.GOAL_SURFACE_NAME}"/>\s*<Translation z="0.001m"/>', floor)
        self.assertEqual(len(patch), 1)  # no x/y translation: centred on the origin
        self.assertEqual(floor.count("<BoxGeometry"), 1)  # the floor slab only

    def test_surfaces_defined_once_with_distinct_colours(self):
        scene = ms.convert(gm.generate(1), "m.maze")
        for name in (ms.WALL_SURFACE_NAME, ms.FLOOR_SURFACE_NAME, ms.GOAL_SURFACE_NAME):
            self.assertEqual(scene.count(f'<Surface name="{name}"'), 1)
            self.assertLess(scene.index(f'<Surface name="{name}"'), scene.index('<Compound name="maze">'))
        self.assertEqual(surface_def(ms.WALL_SURFACE_NAME), ms.WALL_COLOR)
        self.assertEqual(surface_def(ms.FLOOR_SURFACE_NAME), ms.FLOOR_COLOR)
        self.assertEqual(surface_def(ms.GOAL_SURFACE_NAME), ms.GOAL_COLOR)
        self.assertEqual(len({ms.WALL_COLOR, ms.FLOOR_COLOR, ms.GOAL_COLOR}), 3)

    def test_known_walls(self):
        scene = ms.convert(gm.generate(1), "m.maze")
        found = boxes(scene)
        # Top border, first cell: along x, half a cell right of the left edge, at the top edge.
        self.assertEqual(found[0], ("0.515m", "0.015m", "0.6m", ms.WALL_COLOR, "-1.75m", "2m", "0.3m"))
        # Left border of the first cell row: along y, at the left edge.
        self.assertIn(("0.015m", "0.515m", "0.6m", ms.WALL_COLOR, "-2m", "1.75m", "0.3m"), found)
        # Bottom-right border corner side: along x at the bottom edge, last column.
        self.assertIn(("0.515m", "0.015m", "0.6m", ms.WALL_COLOR, "1.75m", "-2m", "0.3m"), found)
        # Right border of the last cell row: along y at the right edge.
        self.assertIn(("0.015m", "0.515m", "0.6m", ms.WALL_COLOR, "2m", "-1.75m", "0.3m"), found)

    def test_start_pose_for_each_corner(self):
        # Position is the centre of the corner cell; heading faces the single open side.
        for start, x, y in ((0, "-1.75m", "1.75m"), (7, "1.75m", "1.75m"),
                            (56, "-1.75m", "-1.75m"), (63, "1.75m", "-1.75m")):
            for seed in range(1, 6):
                text = gm.generate(seed, start=start)
                lines = gm.parse(text)
                r, c = divmod(start, gm.N)
                open_sides = [(dr, dc) for dr, dc in gm.DIRS if lines[gm.wall_pos(r, c, dr, dc)[0]][gm.wall_pos(r, c, dr, dc)[1]] == " "]
                self.assertEqual(len(open_sides), 1)
                expected = {(-1, 0): 90, (1, 0): -90, (0, -1): 180, (0, 1): 0}[open_sides[0]]
                self.assertEqual(robot(ms.convert(text, "m.maze")), (x, y, expected))

    def test_deterministic(self):
        text = gm.generate(4, start=56, loops=2)
        self.assertEqual(ms.convert(text, "m.maze"), ms.convert(text, "m.maze"))

    def test_header_names_source(self):
        scene = ms.convert(gm.generate(1), "Config/Mazes/generated/maze_1.maze")
        self.assertTrue(scene.startswith("<!-- Generated by Util/MazeTools/maze_to_scene.py from "
                                         "Config/Mazes/generated/maze_1.maze."))

    def test_invalid_maze_rejected(self):
        with self.assertRaises(ValueError):
            ms.convert("not a maze\n", "m.maze")
        with self.assertRaises(ValueError):
            ms.convert(gm.generate(1).replace("S", " "), "m.maze")


class MainTest(unittest.TestCase):
    def test_writes_file_and_rejects_invalid_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            good, bad = Path(tmp, "good.maze"), Path(tmp, "bad.maze")
            good.write_text(gm.generate(1))
            bad.write_text("nope\n")
            out = Path(tmp, "sub", "current.rsi2")
            self.assertEqual(ms.main([str(good), "--out", str(out)]), 0)
            self.assertEqual(out.read_text(), ms.convert(gm.generate(1), str(good)))
            self.assertEqual(ms.main([str(bad), "--out", str(Path(tmp, "x.rsi2"))]), 1)
            self.assertFalse(Path(tmp, "x.rsi2").exists())
            self.assertEqual(ms.main([str(Path(tmp, "missing.maze"))]), 1)

    def test_out_also_copies_the_maze_next_to_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp, "good.maze")
            good.write_text(gm.generate(1))
            out = Path(tmp, "sub", "current.rsi2")
            self.assertEqual(ms.main([str(good), "--out", str(out)]), 0)
            self.assertEqual(Path(tmp, "sub", "current.maze").read_bytes(), good.read_bytes())

    def test_copy_follows_the_latest_maze(self):
        with tempfile.TemporaryDirectory() as tmp:
            first, second = Path(tmp, "a.maze"), Path(tmp, "b.maze")
            first.write_text(gm.generate(1))
            second.write_text(gm.generate(2))
            out = Path(tmp, "current.rsi2")
            self.assertEqual(ms.main([str(first), "--out", str(out)]), 0)
            self.assertEqual(ms.main([str(second), "--out", str(out)]), 0)
            self.assertEqual(Path(tmp, "current.maze").read_bytes(), second.read_bytes())

    def test_copy_not_written_for_invalid_maze(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp, "bad.maze")
            bad.write_text("nope\n")
            self.assertEqual(ms.main([str(bad), "--out", str(Path(tmp, "current.rsi2"))]), 1)
            self.assertFalse(Path(tmp, "current.maze").exists())
            self.assertFalse(Path(tmp, "current.rsi2").exists())

    def test_copy_not_written_without_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp, "good.maze")
            good.write_text(gm.generate(1))
            before = sorted(p.name for p in Path(tmp).iterdir())
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(ms.main([str(good)]), 0)
            self.assertEqual(sorted(p.name for p in Path(tmp).iterdir()), before)


if __name__ == "__main__":
    unittest.main()
