#!/usr/bin/env python3
"""Write a NIF file of Fallout 3 format (20.2.0.7, Bethesda version 34) that is only the marker of a piece of furniture.

The model of a chair or a couch in the games has a BSFurnitureMarker in its root: the places where a character stands to use
it. Each entry is an offset in the space of the model (at the level of the floor), an orientation (about a thousandth of a
radian) and the kind of use (the reference class: 1 to 6 sleep, 10 to 20 sit). The file has a root node with that one extra
data and nothing to draw.

    scripts/openfallout/furniture_marker.py chair.nif
"""
import struct
import sys
from pathlib import Path

import havok_wall


def marker_block(entries):
    """A BSFurnitureMarker of the entries (offset, orientation, kind), the way Fallout 3 writes them."""
    data = struct.pack("<i", 1)  # name: the second string
    data += struct.pack("<I", len(entries))
    for offset, orientation, kind in entries:
        data += struct.pack("<3fHBB", *offset, orientation, kind, kind)
    return data


def chair(entries):
    """A file with the markers of a chair and nothing to draw."""
    blocks = [
        ("NiNode", havok_wall.root_node(1, -1)),
        ("BSFurnitureMarker", marker_block(entries)),
    ]
    return havok_wall.nif(blocks, ["OFTestChair", "FRN"], [0])


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(chair([((40.0, 25.0, 0.0), 1500, 14)]))
    print(path)
