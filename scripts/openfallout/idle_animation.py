#!/usr/bin/env python3
"""Write an animation file (.kf) of Fallout 3 format (20.2.0.7, Bethesda version 34) that moves one bone.

The animation files of a character of Fallout 3 and New Vegas hold a NiControllerSequence, the name of each node that
it drives with an interpolator that has the keys of the node, and the text keys that name the events of the animation.
This one drives a single bone with two translation keys, so that a test can tell whether the engine plays it: the
foot of the skeleton of scripts/openfallout/placeholder_skeleton.py is a child of the calf and 26 units under it,
and the animation moves it 150 units to the left of the actor and holds it there (a sequence that clamps).

    scripts/openfallout/idle_animation.py mtidle.kf
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from havok_wall import nif  # noqa: E402

LOOP, REVERSE, CLAMP = 0, 1, 2
NONE = -1  # a reference to nothing, an index into the table of strings that is none


def text_keys(keys):
    """A NiTextKeyExtraData: keys are (time, index of the text in the table of strings)."""
    data = struct.pack("<iI", NONE, len(keys))
    return data + b"".join(struct.pack("<fi", time, text) for time, text in keys)


def transform_interpolator(data_ref):
    """The default value of the node (a translation, a rotation as w, x, y, z, a scale) and its keys."""
    return struct.pack("<3f", 0, 0, 0) + struct.pack("<4f", 1, 0, 0, 0) + struct.pack("<f", 1) + struct.pack("<i", data_ref)


def translation_data(keys):
    """A NiTransformData with only linear translation keys, (time, (x, y, z))."""
    data = struct.pack("<I", 0)  # no rotation keys
    data += struct.pack("<II", len(keys), 1)  # translation keys, of the linear kind
    data += b"".join(struct.pack("<f3f", time, *translation) for time, translation in keys)
    data += struct.pack("<I", 0)  # no scale keys
    return data


def sequence(cycle, start, stop):
    """A sequence that drives one node, with the text keys after it. The names are those of the table of strings that
    `idle_animation` writes: the sequence, the node and the kind of controller are its first three."""
    # NiSequence: the name, the number of controlled blocks, the growth of the array, the blocks
    data = struct.pack("<iII", 0, 1, 0)
    # The block: interpolator, controller, priority, the node, the kind of property, the controller, its id, the
    # interpolator's id
    data += struct.pack("<iiB", 2, NONE, 50) + struct.pack("<5i", 1, NONE, 2, NONE, NONE)
    # The sequence: weight, text keys, cycle type, frequency, start, stop, manager, accumulation root, animation notes
    data += struct.pack("<fiIfff", 1.0, 1, cycle, 1.0, start, stop)
    data += struct.pack("<ii", NONE, NONE) + struct.pack("<H", 0)
    return data


def idle_animation(name="MTIdle", node="Bip01 L Foot", cycle=CLAMP, stop=1.0, offset=-150.0, height=-26.0):
    """A sequence of `stop` seconds that moves the node from its place under the calf, at `height`, to `offset` to the
    side of it"""
    strings = [name, node, "NiTransformController", "Start", "End"]
    keys = [(0.0, (0.0, 0.0, height)), (stop, (offset, 0.0, height))]
    blocks = [
        ("NiControllerSequence", sequence(cycle, 0.0, stop)),
        ("NiTextKeyExtraData", text_keys([(0.0, 3), (stop, 4)])),
        ("NiTransformInterpolator", transform_interpolator(3)),
        ("NiTransformData", translation_data(keys)),
    ]
    return nif(blocks, strings, [0])


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(idle_animation())
    print(path)
