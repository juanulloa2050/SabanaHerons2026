#!/usr/bin/env python3
"""Convert a ``.maze`` file into a SimRobot include (``.rsi2``).

The include defines two compounds that ``Config/Scenes/Maze.ros2`` references:

* ``maze``: per wall character (``-`` or ``|``) of the file, border included, one
  ``BoxAppearance`` (visual) and one ``BoxGeometry`` (collision) of the same size and
  position. Each box is one 0.5 m cell side plus a 1.5 cm overlap.
  The wall faces that carry an AprilTag (see ``Config/Scenarios/Maze/mazeTagLayout.cfg``) also get a
  textured quad (appearance only) inside this compound, placed where the oracle assumes the tag.
* ``mazeFloor``: a matte floor slab (geometry and appearance, top at z = 0) with a margin around the
  maze, and an appearance-only patch over the goal block (cells 27, 28, 35, 36).
* ``mazeRobots``: the robot placed at the centre of ``S``, facing the open side.

The maze is centred on the simulator origin: cell (row, col) has its centre at
x = (col - 3.5) * 0.5 m, y = (3.5 - row) * 0.5 m (row 0 is the top row, so it
is the +y side). Output is deterministic. Standard library only.
"""

import argparse
import os
import re
import sys
import tempfile
from pathlib import Path

import generate_maze as gm
import make_tag_textures as tags

# Colours (placeholders until the organisers confirm them; edit here, then regenerate current.rsi2).
WALL_COLOR = "rgb(235, 235, 235)"
FLOOR_COLOR = "rgb(200, 190, 170)"
GOAL_COLOR = "rgb(60, 120, 200)"  # must differ from FLOOR_COLOR

CELL = 0.5
WALL_LENGTH = "0.515m"
WALL_THICKNESS = "0.015m"
WALL_HEIGHT_M = 0.6
WALL_HEIGHT = f"{WALL_HEIGHT_M:g}m"
WALL_SURFACE_NAME = "mazeWall"
FLOOR_SURFACE_NAME = "mazeFloorSurface"
GOAL_SURFACE_NAME = "mazeGoal"
FLOOR_SIZE_M = gm.N * CELL + 2.0  # the maze plus a 1 m margin on each side
FLOOR_THICKNESS_M = 0.2
GOAL_SIZE_M = 2 * CELL  # the central 2x2 block of cells
GOAL_THICKNESS_M = 0.002
ROBOT_HEIGHT = "320mm"
# Heading in degrees (+x = 0, +y = 90) for each (dr, dc); row 0 is the +y side.
HEADINGS = {(-1, 0): 90, (1, 0): -90, (0, -1): 180, (0, 1): 0}

# AprilTags. Faces as in Maze::Heading (north is +y); each has (dr, dc) towards its wall and the unit vector
# (dx, dy) from the cell centre to it.
TAG_LAYOUT_PATH = Path(__file__).resolve().parents[2] / "Config/Scenarios/Maze/mazeTagLayout.cfg"
FACES = {"north": (-1, 0, 0, 1), "east": (0, 1, 1, 0), "south": (1, 0, 0, -1), "west": (0, -1, -1, 0)}
TAG_MODES = ("allFaces", "faces", "walls")
TAG_OFFSET_M = 0.001  # in front of the wall surface, against z-fighting
TEXTURE_DIR = "../../Textures/AprilTags/"  # relative to Includes/Maze/current.rsi2


def metres(value):
    return f"{value:g}m"


def surface(name, color, ambient, specular, shininess):
    """Return a Surface definition: matte parameters, diffuse colour from the same constant as the geometry."""
    return (f'<Surface name="{name}" diffuseColor="{color}" ambientColor="rgb({ambient}%, {ambient}%, {ambient}%)" '
            f'specularColor="rgb({specular}%, {specular}%, {specular}%)" shininess="{shininess}"/>')


def box_size(depth, width, height):
    return f'depth="{metres(depth)}" width="{metres(width)}" height="{metres(height)}"'


def cell_centre(r, c):
    return (c - gm.N / 2 + 0.5) * CELL, (gm.N / 2 - 0.5 - r) * CELL


def wall_centre(i, j):
    """Centre (x, y) of the wall at grid position (i, j); half a cell off the cell centres."""
    return (j - gm.N) * CELL / 2, (gm.N - i) * CELL / 2


def walls(lines):
    """Yield (x, y, along_x) for every wall character, in row-major order."""
    for i, line in enumerate(lines):
        for j, ch in enumerate(line):
            if ch in "-|":
                yield (*wall_centre(i, j), ch == "-")


def start_pose(lines):
    """Return (x, y, heading in degrees) of the start cell, facing its single open side."""
    for r in range(gm.N):
        for c in range(gm.N):
            if lines[2 * r + 1][2 * c + 1] == "S":
                for (dr, dc), heading in HEADINGS.items():
                    wr, wc = gm.wall_pos(r, c, dr, dc)
                    if lines[wr][wc] == " ":
                        return (*cell_centre(r, c), heading)
    raise ValueError("no start cell with an open side")


class TagLayout:
    """Which wall faces carry a tag; the same rules as Maze::MazeTagLayout::hasTag, minus the wall check."""

    def __init__(self, mode="allFaces", faces=tuple(FACES), walls=(), centre_height=300.0, size=150.0):
        self.mode, self.faces, self.walls = mode, tuple(faces), tuple(walls)
        self.centre_height, self.size = centre_height, size

    def candidates(self, cell):
        if self.mode == "allFaces":
            return list(FACES)
        if self.mode == "faces":
            return [f for f in FACES if f in self.faces]
        return [f for f in FACES if (cell, f) in self.walls]


def _split_top_level(text, separator):
    """Split on `separator` outside of [] and {}."""
    parts, depth, start = [], 0, 0
    for i, ch in enumerate(text):
        depth += ch in "[{"
        depth -= ch in "]}"
        if ch == separator and depth == 0:
            parts.append(text[start:i])
            start = i + 1
    parts.append(text[start:])
    return [part.strip() for part in parts if part.strip()]


def _face(name):
    if name not in FACES:
        raise ValueError(f"unknown face '{name}'")
    return name


def _pair(field):
    key, _, value = field.partition("=")
    return key.strip(), value.strip()


def parse_tag_layout(text):
    """Parse a mazeTagLayout.cfg (key = value; // comment); raise ValueError on anything unknown."""
    values = {}
    for statement in _split_top_level(re.sub(r"//[^\n]*", "", text), ";"):
        key, sep, value = statement.partition("=")
        if not sep:
            raise ValueError(f"cannot parse '{statement}'")
        values[key.strip()] = value.strip()
    unknown = set(values) - {"mode", "faces", "walls", "centreHeight", "size"}
    if unknown:
        raise ValueError(f"unknown key '{sorted(unknown)[0]}'")
    layout = TagLayout()
    try:
        if "mode" in values:
            if values["mode"] not in TAG_MODES:
                raise ValueError(f"unknown mode '{values['mode']}'")
            layout.mode = values["mode"]
        if "faces" in values:
            layout.faces = tuple(_face(f.strip()) for f in values["faces"].strip("[]").split(",") if f.strip())
        if "walls" in values:
            walls = []
            for entry in _split_top_level(values["walls"].strip("[]"), ","):
                fields = dict(_pair(f) for f in _split_top_level(entry.strip("{}"), ";"))
                walls.append((int(fields["cell"]), _face(fields["face"])))
            layout.walls = tuple(walls)
        if "centreHeight" in values:
            layout.centre_height = float(values["centreHeight"])
        if "size" in values:
            layout.size = float(values["size"])
    except (KeyError, ValueError) as exc:
        raise ValueError(f"invalid tag layout: {exc}") from None
    return layout


def tag_quads(lines, layout):
    """Yield (cell, face, corners) for each wall face that carries a tag; corners are 4 (x, y, z) in metres,
    bottom-left, bottom-right, top-right, top-left as seen from inside the cell looking at the wall."""
    half = layout.size / 1000 * tags.TOTAL_WIDTH / tags.WIDTH_AT_BORDER / 2  # the white ring is part of the texture
    z0, z1 = layout.centre_height / 1000 - half, layout.centre_height / 1000 + half
    reach = CELL / 2 - float(WALL_THICKNESS[:-1]) / 2 - TAG_OFFSET_M  # cell centre to the quad
    for cell in range(gm.N * gm.N):
        r, c = divmod(cell, gm.N)
        cx, cy = cell_centre(r, c)
        for face in layout.candidates(cell):
            dr, dc, dx, dy = FACES[face]
            wr, wc = gm.wall_pos(r, c, dr, dc)
            if lines[wr][wc] not in "-|":
                continue
            x, y = cx + dx * reach, cy + dy * reach
            rx, ry = dy * half, -dx * half  # right when looking at the wall, tag x axis
            yield cell, face, [(x - rx, y - ry, z0), (x + rx, y + ry, z0), (x + rx, y + ry, z1), (x - rx, y - ry, z1)]


def tag_appearance(cell, face, corners):
    vertices = "\n".join("        " + " ".join(f"{v:.6g}" for v in corner) for corner in corners)
    return [f'    <ComplexAppearance name="tag{cell}{face}">',
            f'      <Surface diffuseColor="rgb(100%, 100%, 100%)" diffuseTexture="{TEXTURE_DIR}{tags.texture_name(cell)}"/>',
            "      <Vertices>", vertices, "      </Vertices>",
            "      <Quads>0 1 2 3</Quads>",
            "      <TexCoords>0 0 1 0 1 1 0 1</TexCoords>",
            "    </ComplexAppearance>"]


def convert(text, source, layout=None, same_colour=False):
    """
    Return the .rsi2 text for a valid maze; raise ValueError otherwise. Tags only with a `layout`.
    With `same_colour` the floor is painted in the wall colour (the goal patch keeps its own), to test the wall perceptor
    when only the lighting tells floor and walls apart.
    """
    error = gm.validate(text)
    if error is not None:
        raise ValueError(error)
    lines = gm.parse(text)
    floor_color = WALL_COLOR if same_colour else FLOOR_COLOR
    out = [
        "<!-- Generated by Util/MazeTools/maze_to_scene.py from " + source + ". Do not edit. -->",
        "<Simulation>",
        "",
        "  " + surface(WALL_SURFACE_NAME, WALL_COLOR, 60, 30, 20),
        "  " + surface(FLOOR_SURFACE_NAME, floor_color, 70, 5, 2),
        "  " + surface(GOAL_SURFACE_NAME, GOAL_COLOR, 70, 5, 2),
        "",
        '  <Compound name="maze">',
    ]
    for x, y, along_x in walls(lines):
        depth, width = (WALL_LENGTH, WALL_THICKNESS) if along_x else (WALL_THICKNESS, WALL_LENGTH)
        size = f'depth="{depth}" width="{width}" height="{WALL_HEIGHT}"'
        translation = f'      <Translation x="{metres(x)}" y="{metres(y)}" z="{metres(WALL_HEIGHT_M / 2)}"/>'
        out += [f"    <BoxAppearance {size}>", f'      <Surface ref="{WALL_SURFACE_NAME}"/>', translation, "    </BoxAppearance>"]
        out += [f'    <BoxGeometry {size} color="{WALL_COLOR}">', translation, "    </BoxGeometry>"]
    if layout is not None:
        for cell, face, corners in tag_quads(lines, layout):
            out += tag_appearance(cell, face, corners)
    out.append("  </Compound>")
    slab = box_size(FLOOR_SIZE_M, FLOOR_SIZE_M, FLOOR_THICKNESS_M)
    slab_z = f'      <Translation z="{metres(-FLOOR_THICKNESS_M / 2)}"/>'
    patch = box_size(GOAL_SIZE_M, GOAL_SIZE_M, GOAL_THICKNESS_M)
    out += [
        "",
        '  <Compound name="mazeFloor">',
        f"    <BoxAppearance {slab}>",
        f'      <Surface ref="{FLOOR_SURFACE_NAME}"/>',
        slab_z,
        "    </BoxAppearance>",
        f'    <BoxGeometry {slab} color="{floor_color}">',
        slab_z,
        "    </BoxGeometry>",
        f"    <BoxAppearance {patch}>",
        f'      <Surface ref="{GOAL_SURFACE_NAME}"/>',
        f'      <Translation z="{metres(GOAL_THICKNESS_M / 2)}"/>',
        "    </BoxAppearance>",
        "  </Compound>",
    ]
    x, y, heading = start_pose(lines)
    out += [
        "",
        '  <Compound name="mazeRobots">',
        '    <Body ref="Nao" name="robot1">',
        '      <Set name="NaoColor" value="black"/>',
        f'      <Translation x="{metres(x)}" y="{metres(y)}" z="{ROBOT_HEIGHT}"/>',
        f'      <Rotation z="{heading}degree"/>',
        "    </Body>",
        "  </Compound>",
        "",
        "</Simulation>",
    ]
    return "\n".join(out) + "\n"


def _write_temp(directory, name, data):
    """Writes `data` (bytes) to a new temp file in `directory` and returns its path."""
    fd, tmp = tempfile.mkstemp(dir=directory, prefix=f".{name}.", suffix=".tmp")
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
    except BaseException:
        Path(tmp).unlink(missing_ok=True)
        raise
    return Path(tmp)


def write_pair(scene_path, scene, maze_bytes):
    """Writes the scene and the maze copy next to it so that they never disagree.

    Both are written completely to temp files in the target directory first, so a full disk or a
    permission error leaves the old pair untouched. Then the maze copy is replaced, and the scene
    last (the scene is what the user opens, the copy follows it); if that second replace fails,
    the previous maze copy is put back. Temp files are always removed. Raises OSError on failure.
    """
    scene_path = Path(scene_path)
    maze_copy = scene_path.with_suffix(".maze")
    scene_path.parent.mkdir(parents=True, exist_ok=True)
    temps = []
    try:
        tmp_maze = _write_temp(scene_path.parent, maze_copy.name, maze_bytes)
        temps.append(tmp_maze)
        tmp_scene = _write_temp(scene_path.parent, scene_path.name, scene.encode("utf-8"))
        temps.append(tmp_scene)
        previous = maze_copy.read_bytes() if maze_copy.exists() else None
        os.replace(tmp_maze, maze_copy)
        try:
            os.replace(tmp_scene, scene_path)
        except OSError:
            # Undo the first replace so the pair stays as it was.
            if previous is None:
                maze_copy.unlink(missing_ok=True)
            else:
                tmp_previous = _write_temp(scene_path.parent, maze_copy.name, previous)
                temps.append(tmp_previous)
                os.replace(tmp_previous, maze_copy)
            raise
    finally:
        for tmp in temps:
            tmp.unlink(missing_ok=True)


def main(argv=None):
    p = argparse.ArgumentParser(description="Convert a .maze file into a SimRobot .rsi2 include.")
    p.add_argument("maze", help="input .maze file")
    p.add_argument("--out", help="output file (default: stdout); a byte-exact copy of the maze is written next to it with the .maze suffix")
    p.add_argument("--tag-layout", default=str(TAG_LAYOUT_PATH),
                   help="AprilTag layout (default: Config/Scenarios/Maze/mazeTagLayout.cfg, the file the oracle reads)")
    p.add_argument("--same-colour-floor", action="store_true",
                   help="paint the floor in the wall colour (the goal patch keeps its colour): only the lighting tells them apart")
    args = p.parse_args(argv)
    try:
        layout = parse_tag_layout(Path(args.tag_layout).read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"cannot read {args.tag_layout}: {exc.strerror}", file=sys.stderr)
        return 1
    except ValueError as exc:
        print(f"{args.tag_layout}: {exc}", file=sys.stderr)
        return 1
    try:
        text = Path(args.maze).read_text(encoding="utf-8")
    except OSError as exc:
        print(f"cannot read {args.maze}: {exc.strerror}", file=sys.stderr)
        return 1
    except UnicodeDecodeError:
        print(f"cannot read {args.maze}: not valid UTF-8 text", file=sys.stderr)
        return 1
    try:
        scene = convert(text, args.maze, layout, args.same_colour_floor)
    except ValueError as exc:
        print(f"invalid maze {args.maze}: {exc}", file=sys.stderr)
        return 1
    if args.out:
        out = Path(args.out)
        # The oracle percepts (OracledMazePerceptsProvider) read the maze from next to the scene, so
        # one command switches both. Written only here, after a successful conversion.
        maze_copy = out.with_suffix(".maze")
        try:
            if maze_copy == out:
                out.parent.mkdir(parents=True, exist_ok=True)
                out.write_text(scene)
            else:
                write_pair(out, scene, Path(args.maze).read_bytes())
        except OSError as exc:
            print(f"cannot write {out} and its maze copy: {exc.strerror or exc}; "
                  f"the previous files are unchanged", file=sys.stderr)
            return 1
    else:
        sys.stdout.write(scene)
    return 0


if __name__ == "__main__":
    sys.exit(main())
