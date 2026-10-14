#!/usr/bin/env python3
"""Write the tag36h11 AprilTag textures (ids 0..63) used by the simulated maze walls.

The bit layout is read from ``Util/apriltag/src/tag36h11.c`` and drawn exactly like the library's
``apriltag_to_image``: a ``total_width`` x ``total_width`` module image with a one module white ring, a
black ring (together ``width_at_border`` modules wide: the black outer square of the tag) and the code
bits inside (a set bit is white). Each module becomes ``SCALE`` x ``SCALE`` pixels, so no filtering
blurs the modules. Output is 8 bit RGB, the only PNG layout besides ARGB that SimRobot loads.
Standard library only.
"""

import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "Util/apriltag/src/tag36h11.c"
OUT_DIR = REPO / "Config/Scenes/Textures/AprilTags"
WIDTH_AT_BORDER = 8  # modules of the black outer square; checked against tag36h11.c by load_family
TOTAL_WIDTH = 10  # modules of the whole image, white ring included
TAG_COUNT = 64  # the 8 x 8 cells of the maze
SCALE = 16  # pixels per module


def load_family(path=SOURCE):
    """Return (codes, bit_x, bit_y, width_at_border, total_width) parsed from tag36h11.c."""
    text = Path(path).read_text(encoding="utf-8")
    codes = [int(v, 16) for v in re.findall(r"^\s*(0x[0-9a-fA-F]+)UL,?\s*$", text, re.M)]
    bit_x, bit_y = {}, {}
    for axis, index, value in re.findall(r"tf->bit_([xy])\[(\d+)\] = (\d+);", text):
        (bit_x if axis == "x" else bit_y)[int(index)] = int(value)
    width_at_border = int(re.search(r"tf->width_at_border = (\d+);", text).group(1))
    total_width = int(re.search(r"tf->total_width = (\d+);", text).group(1))
    if "reversed_border = false" not in text or len(codes) != 587 or len(bit_x) != 36 or len(bit_y) != 36:
        raise ValueError(f"unexpected layout in {path}")
    if (width_at_border, total_width) != (WIDTH_AT_BORDER, TOTAL_WIDTH):
        raise ValueError(f"unexpected tag width in {path}")
    return codes, [bit_x[i] for i in range(36)], [bit_y[i] for i in range(36)], width_at_border, total_width


def tag_modules(code, bit_x, bit_y, width_at_border, total_width):
    """Return the module grid (rows of 0 = black, 255 = white) of one tag, like apriltag_to_image."""
    grid = [[0] * total_width for _ in range(total_width)]
    last = total_width - 1  # the white ring is the outermost module ring
    for i in range(total_width):
        grid[0][i] = grid[last][i] = grid[i][0] = grid[i][last] = 255
    start = (total_width - width_at_border) // 2
    nbits = len(bit_x)
    for i in range(nbits):
        if code >> (nbits - i - 1) & 1:
            grid[bit_y[i] + start][bit_x[i] + start] = 255
    return grid


def png_bytes(grid, scale=SCALE):
    """Encode a module grid as an 8 bit RGB PNG with ``scale`` x ``scale`` pixels per module."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    rows = []
    for row in grid:
        line = b"\x00" + b"".join(bytes((v, v, v)) * scale for v in row)  # filter type 0
        rows.extend([line] * scale)
    size = len(grid) * scale
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


def texture_name(tag_id):
    return f"tag36_11_{tag_id:05d}.png"  # the upstream apriltag-imgs naming


def main(argv=None):
    p = argparse.ArgumentParser(description="Write the tag36h11 textures of the maze tags (ids 0..63).")
    p.add_argument("--out", default=str(OUT_DIR), help="output directory (default: Config/Scenes/Textures/AprilTags)")
    args = p.parse_args(argv)
    codes, bit_x, bit_y, width_at_border, total_width = load_family()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for tag_id in range(TAG_COUNT):
        grid = tag_modules(codes[tag_id], bit_x, bit_y, width_at_border, total_width)
        (out / texture_name(tag_id)).write_bytes(png_bytes(grid))
    print(f"wrote {TAG_COUNT} textures to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
