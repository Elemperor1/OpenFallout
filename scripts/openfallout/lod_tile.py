#!/usr/bin/env python3
"""Write a NIF file of Fallout 3 format (20.2.0.7, Bethesda version 34) that is a tile of the distant land of a worldspace.

The games ship the distant land of a worldspace as tiles under meshes/landscape/lod/<worldspace>/, named
<worldspace>.level<L>.x<X>.y<Y>.nif: one covers L by L cells from the cell X, Y. The root node of a tile is moved to the
south west corner of it (X and Y times the size of a cell, 4096) and its vertices run from 0 to L times that size. The
tiles have no normals; a skirt hangs from their edge far below the land (z = -14098 in the games), which hides the gaps
between a tile and a neighbour of another level of detail.

This one is a flat square at one height with a skirt down from its south edge.

    scripts/openfallout/lod_tile.py oftestworld.level8.x8.y0.nif
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from havok_wall import nif, ref_list  # noqa: E402

NONE = -1
IDENTITY = (1, 0, 0, 0, 1, 0, 0, 0, 1)
CELL_SIZE = 4096.0
SKIRT_FLOOR = -14098.0


def node(name, translation, children):
    """An NiNode with a translation, no rotation, no properties, no extra data and no effects."""
    data = struct.pack("<i", name) + ref_list([]) + struct.pack("<i", NONE)  # name, extra data, controller
    data += struct.pack("<I", 0)  # flags
    data += struct.pack("<3f", *translation) + struct.pack("<9f", *IDENTITY) + struct.pack("<f", 1)
    data += ref_list([]) + struct.pack("<i", NONE)  # properties, collision
    return data + ref_list(children) + ref_list([])


def shape(name, data_ref):
    """An NiTriShape with its data and nothing else."""
    out = struct.pack("<i", name) + ref_list([]) + struct.pack("<i", NONE)
    out += struct.pack("<I", 0)  # flags
    out += struct.pack("<3f", 0, 0, 0) + struct.pack("<9f", *IDENTITY) + struct.pack("<f", 1)
    out += ref_list([]) + struct.pack("<i", NONE)  # properties, collision
    out += struct.pack("<ii", data_ref, NONE)
    return out + struct.pack("<IiB", 0, NONE, 0)  # no material names, no active material, no update needed


def shape_data(points, triangles, size):
    """An NiTriShapeData of the points and triangles, with no normals, colours or texture coordinates."""
    out = struct.pack("<iH", 0, len(points))  # group, number of vertices
    out += struct.pack("<BB", 0, 0)  # keep and compress flags
    out += struct.pack("<B", 1) + b"".join(struct.pack("<3f", *p) for p in points)
    out += struct.pack("<H", 0)  # flags: no texture coordinates, no tangents
    out += struct.pack("<B", 0)  # no normals
    out += struct.pack("<3f", size / 2, size / 2, 0) + struct.pack("<f", size)  # bounding sphere
    out += struct.pack("<B", 0)  # no vertex colours
    out += struct.pack("<Hi", 0, NONE)  # consistency, additional data
    out += struct.pack("<H", len(triangles))
    out += struct.pack("<IB", 3 * len(triangles), 1) + b"".join(struct.pack("<3H", *t) for t in triangles)
    return out + struct.pack("<H", 0)  # no match groups


def tile(level, x, y, height):
    """A file for the tile of L by L cells from the cell x, y: a square at the height, and below its south edge the skirt."""
    size = level * CELL_SIZE
    points = [(0, 0, height), (size, 0, height), (size, size, height), (0, size, height),
              (0, 0, SKIRT_FLOOR), (size, 0, SKIRT_FLOOR)]
    triangles = [(0, 1, 2), (0, 2, 3), (0, 5, 1), (0, 4, 5)]
    blocks = [
        ("NiNode", node(0, (x * CELL_SIZE, y * CELL_SIZE, 0), [1])),
        ("NiTriShape", shape(1, 2)),
        ("NiTriShapeData", shape_data(points, triangles, size)),
    ]
    return nif(blocks, ["OFTestTile", "OFTestTileShape"], [0])


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(tile(8, 8, 0, -200.0))
    print(path)
