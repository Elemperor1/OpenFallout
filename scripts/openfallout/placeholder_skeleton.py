#!/usr/bin/env python3
"""Write the placeholder actor skeleton: a NIF file of 4.0.0.2 format that holds only empty bones.

The engine builds every actor, the player included, on a base skeleton that Morrowind supplies as `meshes/base_anim.nif`
and a few siblings. A game without them, Fallout 3 and New Vegas, has no such file under that name, and the world
refuses to place the player. This file stands in at those paths from resources/vfs-fallback, which the engine adds
below every archive and data directory: a game that has its own file shadows it. It draws nothing and carries no
animation, so an actor on it is invisible and does not move its limbs. It does have the bounding box node that the
physics reads the size of an actor from, a human 40 units wide and 128 tall.
It is a stop-gap for walking the world, not the Fallout skeleton, which comes with the actor work of a later milestone.

    scripts/openfallout/placeholder_skeleton.py files/data/meshes/placeholder_skeleton.nif

The bones are those the engine looks up by name on a humanoid: the root, the spine, the head, the hands and feet.
They sit at the heights of a rough human, 124 units to the eyes, and the node `Head` is at eye height: the first person
camera follows the node `Camera` or, when there is none, `Head`, and with none of them it stays at the world origin.
"""
import struct
import sys
from pathlib import Path

# Half extents and centre of the box that actors collide as, in game units: the feet are at the origin.
BOX_EXTENTS = (20.0, 20.0, 64.0)
BOX_CENTER = (0.0, 0.0, 64.0)

# (name, parent, offset from the parent) in file order; the first bone is the root. The feet are at the origin, x is
# to the right of the actor, y in front of it and z up. `Head` is the attachment node the camera follows, at eye height.
BONES = [
    ("Bip01", None, (0, 0, 0)),
    ("Bounding Box", "Bip01", (0, 0, 0)),
    ("Bip01 Pelvis", "Bip01", (0, 0, 64)),
    ("Bip01 Spine", "Bip01 Pelvis", (0, 0, 6)),
    ("Bip01 Spine1", "Bip01 Spine", (0, 0, 10)),
    ("Bip01 Spine2", "Bip01 Spine1", (0, 0, 10)),
    ("Bip01 Neck", "Bip01 Spine2", (0, 0, 18)),
    ("Bip01 Head", "Bip01 Neck", (0, 0, 8)),
    ("Head", "Bip01 Head", (0, 0, 8)),
    ("Bip01 L Clavicle", "Bip01 Spine2", (-6, 0, 6)),
    ("Bip01 L UpperArm", "Bip01 L Clavicle", (-10, 0, 0)),
    ("Bip01 L Forearm", "Bip01 L UpperArm", (-12, 0, 0)),
    ("Bip01 L Hand", "Bip01 L Forearm", (-10, 0, 0)),
    ("Bip01 R Clavicle", "Bip01 Spine2", (6, 0, 6)),
    ("Bip01 R UpperArm", "Bip01 R Clavicle", (10, 0, 0)),
    ("Bip01 R Forearm", "Bip01 R UpperArm", (12, 0, 0)),
    ("Bip01 R Hand", "Bip01 R Forearm", (10, 0, 0)),
    ("Bip01 L Thigh", "Bip01 Pelvis", (-8, 0, -4)),
    ("Bip01 L Calf", "Bip01 L Thigh", (0, 0, -26)),
    ("Bip01 L Foot", "Bip01 L Calf", (0, 0, -26)),
    ("Bip01 R Thigh", "Bip01 Pelvis", (8, 0, -4)),
    ("Bip01 R Calf", "Bip01 R Thigh", (0, 0, -26)),
    ("Bip01 R Foot", "Bip01 R Calf", (0, 0, -26)),
]


def sized(text):
    data = text.encode("ascii")
    return struct.pack("<I", len(data)) + data


def node(name, children, box=None, offset=(0, 0, 0)):
    """An NiNode of NIF version 4.0.0.2 at an offset from its parent, with no rotation, scale or properties, and a box
    bound if asked."""
    data = sized(name)
    data += struct.pack("<ii", -1, -1)  # extra data, controller
    data += struct.pack("<H", 0)  # flags
    data += struct.pack("<3f", *offset)  # translation
    data += struct.pack("<9f", 1, 0, 0, 0, 1, 0, 0, 0, 1)  # rotation
    data += struct.pack("<f", 1)  # scale
    data += struct.pack("<3f", 0, 0, 0)  # velocity
    data += struct.pack("<I", 0)  # no properties
    if box is None:
        data += struct.pack("<I", 0)  # no bounding volume
    else:
        center, extents = box
        data += struct.pack("<I", 1)  # has a bounding volume
        data += struct.pack("<I", 1)  # of the box type
        data += struct.pack("<3f", *center) + struct.pack("<9f", 1, 0, 0, 0, 1, 0, 0, 0, 1)
        data += struct.pack("<3f", *extents)
    data += struct.pack("<I", len(children)) + b"".join(struct.pack("<i", c) for c in children)
    data += struct.pack("<I", 0)  # no effects
    return sized("NiNode") + data


def skeleton():
    index = {name: i for i, (name, _, _) in enumerate(BONES)}
    children = {name: [] for name, _, _ in BONES}
    for name, parent, _ in BONES:
        if parent is not None:
            children[parent].append(index[name])
    out = b"NetImmerse File Format, Version 4.0.0.2\n"
    out += struct.pack("<II", 0x04000002, len(BONES))
    out += b"".join(node(name, children[name], (BOX_CENTER, BOX_EXTENTS) if name == "Bounding Box" else None, offset)
                    for name, _, offset in BONES)
    out += struct.pack("<Ii", 1, 0)  # one root, the first record
    return out


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(skeleton())
    print(path)
