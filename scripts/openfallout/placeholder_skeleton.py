#!/usr/bin/env python3
"""Write the placeholder actor skeleton: a NIF file of 4.0.0.2 format that holds only empty bones.

The engine builds every actor, the player included, on a base skeleton that Morrowind supplies as `meshes/base_anim.nif`
and a few siblings. A game without them, Fallout 3 and New Vegas, has no such file under that name, and the world
refuses to place the player. This file stands in at those paths, one level below any game data: a game that has its own
file shadows it. It draws nothing and carries no animation, so an actor on it is invisible and does not move its
limbs. It does have the bounding box node that the physics reads the size of an actor from, a human 40 units wide and
128 tall.
It is a stop-gap for walking the world, not the Fallout skeleton, which comes with the actor work of a later milestone.

    scripts/openfallout/placeholder_skeleton.py files/data/meshes/placeholder_skeleton.nif

The bones are those the engine looks up by name on a humanoid: the root, the spine, the head, the hands and feet.
"""
import struct
import sys
from pathlib import Path

# Half extents and centre of the box that actors collide as, in game units: the feet are at the origin.
BOX_EXTENTS = (20.0, 20.0, 64.0)
BOX_CENTER = (0.0, 0.0, 64.0)

# (name, parent) in file order; the first bone is the root.
BONES = [
    ("Bip01", None),
    ("Bounding Box", "Bip01"),
    ("Bip01 Pelvis", "Bip01"),
    ("Bip01 Spine", "Bip01 Pelvis"),
    ("Bip01 Spine1", "Bip01 Spine"),
    ("Bip01 Spine2", "Bip01 Spine1"),
    ("Bip01 Neck", "Bip01 Spine2"),
    ("Bip01 Head", "Bip01 Neck"),
    ("Bip01 L Clavicle", "Bip01 Spine2"),
    ("Bip01 L UpperArm", "Bip01 L Clavicle"),
    ("Bip01 L Forearm", "Bip01 L UpperArm"),
    ("Bip01 L Hand", "Bip01 L Forearm"),
    ("Bip01 R Clavicle", "Bip01 Spine2"),
    ("Bip01 R UpperArm", "Bip01 R Clavicle"),
    ("Bip01 R Forearm", "Bip01 R UpperArm"),
    ("Bip01 R Hand", "Bip01 R Forearm"),
    ("Bip01 L Thigh", "Bip01 Pelvis"),
    ("Bip01 L Calf", "Bip01 L Thigh"),
    ("Bip01 L Foot", "Bip01 L Calf"),
    ("Bip01 R Thigh", "Bip01 Pelvis"),
    ("Bip01 R Calf", "Bip01 R Thigh"),
    ("Bip01 R Foot", "Bip01 R Calf"),
]


def sized(text):
    data = text.encode("ascii")
    return struct.pack("<I", len(data)) + data


def node(name, children, box=None):
    """An NiNode of NIF version 4.0.0.2 with an identity transform and no properties, and a box bound if asked."""
    data = sized(name)
    data += struct.pack("<ii", -1, -1)  # extra data, controller
    data += struct.pack("<H", 0)  # flags
    data += struct.pack("<3f", 0, 0, 0)  # translation
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
    index = {name: i for i, (name, _) in enumerate(BONES)}
    children = {name: [] for name, _ in BONES}
    for name, parent in BONES:
        if parent is not None:
            children[parent].append(index[name])
    out = b"NetImmerse File Format, Version 4.0.0.2\n"
    out += struct.pack("<II", 0x04000002, len(BONES))
    out += b"".join(node(name, children[name], (BOX_CENTER, BOX_EXTENTS) if name == "Bounding Box" else None)
                    for name, _ in BONES)
    out += struct.pack("<Ii", 1, 0)  # one root, the first record
    return out


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(skeleton())
    print(path)
