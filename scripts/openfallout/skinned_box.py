#!/usr/bin/env python3
"""Write a NIF file of Fallout 3 format (20.2.0.7, Bethesda version 34) with a box that is skinned to one bone.

The body of a character of Fallout 3 and New Vegas is made of meshes that are skinned to the bones of its skeleton: each
vertex follows the bones it names, so a body part moves when an animation moves the bones. This is the smallest such
mesh, for tests of that: a box, all of whose vertices follow one bone completely. The file has a root node, the bone as
a node of its own (the skin names the nodes it follows, and they have to be in the file), the shape with its data and
the skin instance and data.

The box is given where it is when the skeleton is at rest, in the space of the skeleton (the feet at the origin), and the
bone where its node is at rest; the file keeps the inverse of that as the transform of the bone, as the games do.

    scripts/openfallout/skinned_box.py foot.nif
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from havok_wall import nif, ref_list  # noqa: E402

NONE = -1
IDENTITY = (1, 0, 0, 0, 1, 0, 0, 0, 1)
# The triangles of a box, over the eight corners of vertices(): the four of the bottom, then the four of the top
CUBE_TRIANGLES = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (2, 3, 7), (2, 7, 6), (1, 2, 6),
                  (1, 6, 5), (3, 0, 4), (3, 4, 7)]


def node(name, translation, children):
    """An NiNode with a translation, no rotation, no properties, no extra data and no effects."""
    data = struct.pack("<i", name) + ref_list([]) + struct.pack("<i", NONE)  # name, extra data, controller
    data += struct.pack("<I", 0)  # flags
    data += struct.pack("<3f", *translation) + struct.pack("<9f", *IDENTITY) + struct.pack("<f", 1)
    data += ref_list([]) + struct.pack("<i", NONE)  # properties, collision
    return data + ref_list(children) + ref_list([])


def shape(name, data_ref, skin_ref):
    """An NiTriShape in the space of the file, with its data and skin, and no materials or properties."""
    out = struct.pack("<i", name) + ref_list([]) + struct.pack("<i", NONE)
    out += struct.pack("<I", 0)  # flags
    out += struct.pack("<3f", 0, 0, 0) + struct.pack("<9f", *IDENTITY) + struct.pack("<f", 1)
    out += ref_list([]) + struct.pack("<i", NONE)  # properties, collision
    out += struct.pack("<ii", data_ref, skin_ref)
    return out + struct.pack("<IiB", 0, NONE, 0)  # no material names, no active material, no update needed


def vertices(centre, half):
    return [(centre[0] + x * half[0], centre[1] + y * half[1], centre[2] + z * half[2])
            for z in (-1, 1) for (x, y) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]


def shape_data(points, half):
    """An NiTriShapeData of the points and the triangles of a box, with no normals, colours or texture coordinates."""
    out = struct.pack("<iH", 0, len(points))  # group, number of vertices
    out += struct.pack("<BB", 0, 0)  # keep and compress flags
    out += struct.pack("<B", 1) + b"".join(struct.pack("<3f", *p) for p in points)
    out += struct.pack("<H", 0)  # flags: no texture coordinates, no tangents
    out += struct.pack("<B", 0)  # no normals
    centre = [sum(p[axis] for p in points) / len(points) for axis in range(3)]
    out += struct.pack("<3f", *centre) + struct.pack("<f", 2 * max(half))  # bounding sphere
    out += struct.pack("<B", 0)  # no vertex colours
    out += struct.pack("<Hi", 0, NONE)  # consistency, additional data
    out += struct.pack("<H", len(CUBE_TRIANGLES))
    out += struct.pack("<IB", 3 * len(CUBE_TRIANGLES), 1) + b"".join(struct.pack("<3H", *t) for t in CUBE_TRIANGLES)
    return out + struct.pack("<H", 0)  # no match groups


def skin_instance(data_ref, root, bones):
    return struct.pack("<ii", data_ref, NONE) + struct.pack("<i", root) + ref_list(bones)


def skin_data(bone_position, points):
    """The skin data: the transform of the skin itself is the identity, and the one bone has all the vertices with the
    weight 1, its transform from the space of the skin to the space of the bone is the move from the bone to the origin."""
    out = struct.pack("<9f", *IDENTITY) + struct.pack("<3f", 0, 0, 0) + struct.pack("<f", 1)
    out += struct.pack("<IB", 1, 1)  # one bone, with weights for the vertices
    out += struct.pack("<9f", *IDENTITY) + struct.pack("<3f", *[-c for c in bone_position]) + struct.pack("<f", 1)
    out += struct.pack("<4f", 0, 0, 0, 100)  # bounding sphere of the bone
    out += struct.pack("<H", len(points))
    return out + b"".join(struct.pack("<Hf", i, 1.0) for i in range(len(points)))


def skinned_box(bone, bone_position, centre, half):
    """A file with a box of the half extents around the centre, skinned to the bone, which is at bone_position when the
    skeleton is at rest (both in the space of the skeleton)."""
    points = vertices(centre, half)
    blocks = [
        ("NiNode", node(0, (0, 0, 0), [1, 2])),
        ("NiTriShape", shape(1, 3, 4)),
        ("NiNode", node(2, bone_position, [])),
        ("NiTriShapeData", shape_data(points, half)),
        ("NiSkinInstance", skin_instance(5, 0, [2])),
        ("NiSkinData", skin_data(bone_position, points)),
    ]
    return nif(blocks, ["OFSkinnedBox", "OFSkinnedBoxShape", bone], [0])


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(skinned_box("Bip01 L Foot", (-8, 0, 8), (-8, 0, 8), (3, 3, 3)))
    print(path)
