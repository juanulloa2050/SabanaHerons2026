"""Unit tests for maze_to_scene (run: python3 -m unittest discover -s Util/MazeTools -v)."""

import contextlib
import io
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_maze as gm  # noqa: E402
import make_tag_textures as tags  # noqa: E402
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

    def test_same_colour_floor_uses_the_wall_colour_and_keeps_the_goal(self):
        scene = ms.convert(gm.generate(1), "m.maze", same_colour=True)

        def colour(name):
            return re.search(rf'<Surface name="{name}" diffuseColor="([^"]+)"', scene).group(1)

        self.assertEqual(colour(ms.FLOOR_SURFACE_NAME), ms.WALL_COLOR)
        self.assertEqual(colour(ms.WALL_SURFACE_NAME), ms.WALL_COLOR)
        self.assertEqual(colour(ms.GOAL_SURFACE_NAME), ms.GOAL_COLOR)
        floor = scene[scene.index('<Compound name="mazeFloor">'):scene.index('<Compound name="mazeRobots">')]
        self.assertIn(f'<BoxGeometry depth="6m" width="6m" height="0.2m" color="{ms.WALL_COLOR}">', floor)
        self.assertNotIn(ms.FLOOR_COLOR, scene)
        # The default keeps the floor colour.
        self.assertIn(ms.FLOOR_COLOR, ms.convert(gm.generate(1), "m.maze"))

    def test_same_colour_option_on_the_command_line(self):
        with tempfile.TemporaryDirectory() as directory:
            maze = Path(directory) / "m.maze"
            maze.write_text(gm.generate(1), encoding="utf-8")
            out = Path(directory) / "current.rsi2"
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(ms.main([str(maze), "--out", str(out), "--same-colour-floor"]), 0)
            self.assertNotIn(ms.FLOOR_COLOR, out.read_text(encoding="utf-8"))

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
            layout = ms.parse_tag_layout(ms.TAG_LAYOUT_PATH.read_text())
            self.assertEqual(out.read_text(), ms.convert(gm.generate(1), str(good), layout))
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

    def _pair_fixture(self, tmp):
        old, new = Path(tmp, "old.maze"), Path(tmp, "new.maze")
        old.write_text(gm.generate(1))
        new.write_text(gm.generate(2))
        out = Path(tmp, "current.rsi2")
        self.assertEqual(ms.main([str(old), "--out", str(out)]), 0)
        return old, new, out

    def test_failed_scene_replace_keeps_the_old_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            old, new, out = self._pair_fixture(tmp)
            scene_before = out.read_text()
            real_replace = os.replace
            calls = []

            def replace(src, dst):
                calls.append(dst)
                if len(calls) == 2:  # the scene, after the maze copy went through
                    raise PermissionError(13, "Permission denied")
                return real_replace(src, dst)

            err = io.StringIO()
            with mock.patch.object(ms.os, "replace", replace), contextlib.redirect_stderr(err):
                self.assertEqual(ms.main([str(new), "--out", str(out)]), 1)
            self.assertIn("cannot write", err.getvalue())
            self.assertEqual(out.read_text(), scene_before)
            self.assertEqual(Path(tmp, "current.maze").read_bytes(), old.read_bytes())
            self.assertEqual(sorted(p.name for p in Path(tmp).iterdir()), ["current.maze", "current.rsi2", "new.maze", "old.maze"])

    def test_failed_second_temp_write_changes_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            old, new, out = self._pair_fixture(tmp)
            scene_before = out.read_text()
            real_write = ms._write_temp

            def write_temp(directory, name, data):
                if name.endswith(".rsi2"):
                    raise OSError(28, "No space left on device")
                return real_write(directory, name, data)

            with mock.patch.object(ms, "_write_temp", write_temp), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ms.main([str(new), "--out", str(out)]), 1)
            self.assertEqual(out.read_text(), scene_before)
            self.assertEqual(Path(tmp, "current.maze").read_bytes(), old.read_bytes())
            self.assertEqual(sorted(p.name for p in Path(tmp).iterdir()), ["current.maze", "current.rsi2", "new.maze", "old.maze"])

    def test_failed_first_write_leaves_no_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp, "good.maze")
            good.write_text(gm.generate(1))
            out = Path(tmp, "sub", "current.rsi2")
            with mock.patch.object(ms.os, "replace", side_effect=OSError(5, "I/O error")), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ms.main([str(good), "--out", str(out)]), 1)
            self.assertEqual(list(Path(tmp, "sub").iterdir()), [])

    def test_copy_not_written_without_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp, "good.maze")
            good.write_text(gm.generate(1))
            before = sorted(p.name for p in Path(tmp).iterdir())
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(ms.main([str(good)]), 0)
            self.assertEqual(sorted(p.name for p in Path(tmp).iterdir()), before)


QUAD = re.compile(r'<ComplexAppearance name="tag(\d+)(\w+)">\s*<Surface [^>]*diffuseTexture="([^"]+)"/>\s*<Vertices>\s*'
                  r'((?:\S+ \S+ \S+\s+){4})</Vertices>')


def quads(scene):
    """Return {(cell, face): (texture, [4 vertices])}."""
    return {(int(cell), face): (texture, [tuple(map(float, row.split())) for row in vertices.strip().splitlines()])
            for cell, face, texture, vertices in QUAD.findall(scene)}


def scene_with(cfg, seed=1):
    return ms.convert(gm.generate(seed), "m.maze", ms.parse_tag_layout(cfg))


class TagLayoutTest(unittest.TestCase):
    def test_parses_the_shipped_layout(self):
        layout = ms.parse_tag_layout(ms.TAG_LAYOUT_PATH.read_text())
        self.assertEqual((layout.mode, layout.centre_height, layout.size), ("allFaces", 300.0, 150.0))
        self.assertEqual(layout.faces, ("north", "east", "south", "west"))

    def test_parses_faces_and_walls(self):
        layout = ms.parse_tag_layout("mode = faces; faces = [east, south]; // c\nwalls = [{cell = 10; face = east;}, "
                                     "{cell = 11; face = north;}]; centreHeight = 250; size = 120;")
        self.assertEqual((layout.mode, layout.faces, layout.centre_height, layout.size), ("faces", ("east", "south"), 250.0, 120.0))
        self.assertEqual(layout.walls, ((10, "east"), (11, "north")))
        self.assertEqual(layout.candidates(10), ["east", "south"])

    def test_rejects_unknown_values(self):
        for cfg in ("mode = everything;", "faces = [up];", "colour = 1;", "walls = [{cell = x; face = east;}]", "walls = [{cell = 1;}]"):
            with self.assertRaises(ValueError, msg=cfg):
                ms.parse_tag_layout(cfg)

    def test_all_faces_one_quad_per_wall_face_never_without_wall(self):
        lines = gm.parse(gm.generate(1))
        found = quads(scene_with("mode = allFaces;"))
        expected = {(r * 8 + c, f) for r in range(8) for c in range(8) for f, (dr, dc, _, _) in ms.FACES.items()
                    if lines[gm.wall_pos(r, c, dr, dc)[0]][gm.wall_pos(r, c, dr, dc)[1]] in "-|"}
        self.assertEqual(set(found), expected)
        self.assertEqual({k for k in expected if k[0] == 0}, {(0, "north"), (0, "east"), (0, "west")})  # open south side

    def test_faces_and_walls_modes(self):
        self.assertEqual({f for _, f in quads(scene_with("mode = faces; faces = [east];"))}, {"east"})
        self.assertEqual(set(quads(scene_with("mode = walls; walls = [{cell = 0; face = north;}, {cell = 0; face = south;}];"))),
                         {(0, "north")})  # cell 0 has no south wall
        self.assertEqual(quads(scene_with("mode = walls;")), {})
        self.assertNotIn("ComplexAppearance", ms.convert(gm.generate(1), "m.maze"))

    def test_quad_position_orientation_and_texture(self):
        # Cell 0 is centred at (-1.75, 1.75); its walls are 15 mm thick, the quad is 1 mm in front of the surface.
        found = quads(scene_with("mode = allFaces;"))
        r = 0.25 - 0.0075 - 0.001
        half = 0.150 * 10 / 8 / 2
        want = {  # face: (quad centre x, y, bottom-left x, y)
            "north": (-1.75, 1.75 + r, -1.75 - half, 1.75 + r),
            "east": (-1.75 + r, 1.75, -1.75 + r, 1.75 + half),
            "west": (-1.75 - r, 1.75, -1.75 - r, 1.75 - half),
        }
        for face, (cx, cy, blx, bly) in want.items():
            texture, (bl, br, tr, tl) = found[(0, face)]
            self.assertTrue(texture.endswith("/tag36_11_00000.png"), texture)
            self.assertAlmostEqual(bl[0], blx, places=5)
            self.assertAlmostEqual(bl[1], bly, places=5)
            self.assertAlmostEqual((bl[0] + tr[0]) / 2, cx, places=5)
            self.assertAlmostEqual((bl[1] + tr[1]) / 2, cy, places=5)
            self.assertAlmostEqual(bl[2], 0.3 - half, places=5)  # centred at 300 mm
            self.assertAlmostEqual(tl[2], 0.3 + half, places=5)
            self.assertEqual(bl[:2], tl[:2])
            self.assertEqual(br[:2], tr[:2])
            # Seen from inside the cell the edge bl -> br points to the viewer's right, which is d x z.
            dx, dy = (cx + 1.75), (cy - 1.75)
            self.assertAlmostEqual((br[0] - bl[0]) * dy - (br[1] - bl[1]) * dx, 2 * half * r, places=5)
            self.assertAlmostEqual((br[0] - bl[0]) * dx + (br[1] - bl[1]) * dy, 0.0, places=5)

    def test_texture_is_the_cell_id_and_size_follows_the_layout(self):
        found = quads(scene_with("mode = allFaces; size = 100; centreHeight = 200;"))
        for (cell, _), (texture, (bl, _, _, tl)) in found.items():
            self.assertTrue(texture.endswith(f"/tag36_11_{cell:05d}.png"), texture)
            self.assertAlmostEqual(tl[2] - bl[2], 0.125, places=5)
            self.assertAlmostEqual((tl[2] + bl[2]) / 2, 0.2, places=5)

    def test_main_reads_the_layout_option(self):
        with tempfile.TemporaryDirectory() as tmp:
            maze, cfg, out = Path(tmp, "m.maze"), Path(tmp, "l.cfg"), Path(tmp, "current.rsi2")
            maze.write_text(gm.generate(1))
            cfg.write_text("mode = faces; faces = [west];")
            self.assertEqual(ms.main([str(maze), "--out", str(out), "--tag-layout", str(cfg)]), 0)
            self.assertEqual({f for _, f in quads(out.read_text())}, {"west"})
            cfg.write_text("mode = bogus;")
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ms.main([str(maze), "--tag-layout", str(cfg)]), 1)
                self.assertEqual(ms.main([str(maze), "--tag-layout", str(Path(tmp, "none.cfg"))]), 1)


class TagTextureTest(unittest.TestCase):
    @staticmethod
    def decode(data):
        """Decode one of our PNGs back into a pixel-per-module grid of grey values."""
        import struct
        import zlib
        assert data[:8] == b"\x89PNG\r\n\x1a\n"
        width, height, depth, colour = struct.unpack(">IIBB", data[16:26])
        assert (depth, colour) == (8, 2)  # RGB, which SimRobot can load
        raw = zlib.decompress(data[data.index(b"IDAT") + 4:data.index(b"IEND") - 8])
        stride = width * 3 + 1
        pixels = [[raw[y * stride + 1 + x * 3] for x in range(width)] for y in range(height)]
        return [[pixels[m * tags.SCALE][n * tags.SCALE] for n in range(width // tags.SCALE)] for m in range(height // tags.SCALE)]

    def test_modules_match_the_apriltag_library_data(self):
        codes, bit_x, bit_y, border, total = tags.load_family()
        self.assertEqual((border, total, len(codes)), (8, 10, 587))
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(tags.main(["--out", tmp]), 0)
            self.assertEqual(len(list(Path(tmp).glob("tag36_11_*.png"))), 64)
            for tag_id in (0, 1, 33, 63):
                grid = self.decode(Path(tmp, tags.texture_name(tag_id)).read_bytes())
                self.assertEqual(len(grid), 10)
                self.assertTrue(all(v == 255 for v in grid[0] + grid[9] + [row[0] for row in grid] + [row[9] for row in grid]))
                self.assertTrue(all(v == 0 for v in grid[1][1:9] + grid[8][1:9] + [row[1] for row in grid[1:9]] + [row[8] for row in grid[1:9]]))
                for i in range(36):
                    bit = codes[tag_id] >> (35 - i) & 1
                    self.assertEqual(grid[bit_y[i] + 1][bit_x[i] + 1], 255 if bit else 0, f"id {tag_id} bit {i}")

    def test_committed_textures_are_up_to_date(self):
        codes, bit_x, bit_y, border, total = tags.load_family()
        for tag_id in range(64):
            expected = tags.png_bytes(tags.tag_modules(codes[tag_id], bit_x, bit_y, border, total))
            self.assertEqual((tags.OUT_DIR / tags.texture_name(tag_id)).read_bytes(), expected, tag_id)


if __name__ == "__main__":
    unittest.main()
