#!/usr/bin/env python3
"""Write a NIF file of Fallout 3 format (20.2.0.7, Bethesda version 34) that is only collision: a wall that is not drawn.

The engine can make the collision of a mesh from its Havok data instead of from the geometry it draws. A mesh with no
geometry at all, which the game uses for a wall that holds the player in, has nothing else to make it from. The file
has a root node, its BSXFlags (2: collision), a bhkCollisionObject, a bhkRigidBody in the layer of static things and
one bhkBoxShape, in the units of Havok, of which one is 7 game units long (so a box 20 units wide is written as 20 / 7).

    scripts/openfallout/havok_wall.py wall.nif
"""
import struct
import sys
from pathlib import Path

HAVOK_SCALE = 7.0
STATIC_LAYER = 1
FILE_VERSION = 0x14020007


def sized(text):
    data = text.encode("ascii")
    return struct.pack("<I", len(data)) + data


def short(text):
    data = text.encode("ascii") + b"\0"
    return struct.pack("<B", len(data)) + data


def ref_list(refs):
    return struct.pack("<I", len(refs)) + b"".join(struct.pack("<i", ref) for ref in refs)


def nif(blocks, strings, roots):
    """A NIF file of Fallout 3 format: blocks are (type name, data), strings the table that the names index."""
    types = []
    for name, _ in blocks:
        if name not in types:
            types.append(name)
    out = b"Gamebryo File Format, Version 20.2.0.7\n"
    out += struct.pack("<IBIII", FILE_VERSION, 1, 11, len(blocks), 34)  # little endian, user version 11, stream 34
    out += short("OpenFallout") + short("") + short("")
    out += struct.pack("<H", len(types)) + b"".join(sized(name) for name in types)
    out += b"".join(struct.pack("<H", types.index(name)) for name, _ in blocks)
    out += b"".join(struct.pack("<I", len(data)) for _, data in blocks)
    out += struct.pack("<II", len(strings), max(len(text) for text in strings))
    out += b"".join(sized(text) for text in strings)
    out += struct.pack("<I", 0)  # no groups
    out += b"".join(data for _, data in blocks)
    return out + ref_list(roots)


def root_node(extra, collision):
    data = struct.pack("<i", 0)  # name: the first string
    data += ref_list([extra]) + struct.pack("<i", -1)  # extra data, no controller
    data += struct.pack("<I", 0)  # flags
    data += struct.pack("<3f", 0, 0, 0) + struct.pack("<9f", 1, 0, 0, 0, 1, 0, 0, 0, 1) + struct.pack("<f", 1)
    data += struct.pack("<I", 0)  # no properties
    data += struct.pack("<i", collision)
    data += ref_list([]) + ref_list([])  # no children, no effects
    return data


def rigid_body(shape, translation, layer=STATIC_LAYER):
    """A body that does not move, its translation in Havok units."""
    data = struct.pack("<i", shape)
    data += struct.pack("<BBH", layer, 0, 0)  # layer, flags, group
    data += struct.pack("<I", 0) + struct.pack("<B", 1) + bytes(3) + struct.pack("<III", 0, 0, 0)  # world object
    data += struct.pack("<BBH", 1, 0, 0)  # entity: response, unused, process contact delay
    # The rigid body: a copy of the filter, the response again, then the transform and the physical properties
    data += bytes(4) + struct.pack("<BBH", layer, 0, 0) + bytes(4)
    data += struct.pack("<BBH", 1, 0, 0) + bytes(4)
    data += struct.pack("<4f", *translation, 0)
    data += struct.pack("<4f", 0, 0, 0, 1)  # rotation as Havok writes a quaternion: x, y, z, w
    data += struct.pack("<4f", 0, 0, 0, 0) * 2  # linear and angular velocity
    data += b"".join(struct.pack("<4f", *row, 0) for row in ((1, 0, 0), (0, 1, 0), (0, 0, 1)))  # inertia tensor
    data += struct.pack("<4f", 0, 0, 0, 0)  # centre of mass
    data += struct.pack("<3f", 0, 0.1, 0.05)  # mass, linear and angular damping
    data += struct.pack("<f", 0.5) + struct.pack("<f", 0.4)  # friction, restitution
    data += struct.pack("<3f", 200, 200, 0.15)  # maximum linear and angular velocity, penetration depth
    data += struct.pack("<BBBB", 7, 1, 1, 1) + bytes(12)  # fixed motion, deactivator, solver deactivation, quality
    data += ref_list([]) + struct.pack("<I", 0)  # no constraints, body flags
    return data


def box_shape(half_extents):
    """A box of the half extents, in Havok units."""
    return struct.pack("<If", 0, 0.1) + bytes(8) + struct.pack("<3f", *half_extents) + bytes(4)


def wall(half_extents, centre):
    """A file with a box of the half extents (in game units) around the centre, relative to the origin of the file, and
    nothing to draw."""
    scale = 1 / HAVOK_SCALE
    blocks = [
        ("NiNode", root_node(1, 2)),
        ("BSXFlags", struct.pack("<iI", 1, 2)),
        ("bhkCollisionObject", struct.pack("<iHi", 0, 1, 3)),
        ("bhkRigidBody", rigid_body(4, [c * scale for c in centre])),
        ("bhkBoxShape", box_shape([h * scale for h in half_extents])),
    ]
    return nif(blocks, ["OFTestWall", "BSX"], [0])


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(wall((20, 100, 100), (0, 0, 100)))
    print(path)
