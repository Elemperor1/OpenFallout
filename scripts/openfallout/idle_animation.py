#!/usr/bin/env python3
"""Write an animation file (.kf) of Fallout 3 format (20.2.0.7, Bethesda version 34) that moves one bone.

The animation files of a character of Fallout 3 and New Vegas hold a NiControllerSequence, the name of each node that
it drives with an interpolator that has the keys of the node, and the text keys that name the events of the animation.
This one drives a single bone with two translation keys, so that a test can tell whether the engine plays it: the
foot of the skeleton of scripts/openfallout/placeholder_skeleton.py is a child of the calf and 26 units under it,
and the animation moves it 150 units to the left of the actor and holds it there (a sequence that clamps).

The files of the games drive most of their bones with the control points of a B-spline instead of keys (a
NiBSplineCompTransformInterpolator, whose control points are shorts that the offset and half range of the channel
scale): `--spline` writes the same movement that way, with four control points on a line.

    scripts/openfallout/idle_animation.py [--spline] mtidle.kf
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


def spline_interpolator(start, stop, data_ref, basis_ref, offset, half_range):
    """A NiBSplineCompTransformInterpolator that has control points for the translation only: the interval, the data
    and the basis, the default value of the node, the handles of the three channels (the points of the translation
    start at the first short; 0xFFFF is the handle of a channel without points) and the offset and half range of each."""
    data = struct.pack("<2f2i", start, stop, data_ref, basis_ref)
    data += struct.pack("<3f", 0, 0, 0) + struct.pack("<4f", 1, 0, 0, 0) + struct.pack("<f", 1)
    data += struct.pack("<3I", 0, 0xFFFF, 0xFFFF)
    data += struct.pack("<6f", offset, half_range, 0, 1, 0, 1)
    return data


def spline_data(points):
    """A NiBSplineData with only compact control points, the shorts of the points one after the other"""
    return struct.pack("<II", 0, len(points)) + struct.pack(f"<{len(points)}h", *points)


def compact_translation(points):
    """The offset, half range and shorts that make the translation points (x, y, z) of a channel: a value is the offset
    plus the half range times the short over 32767"""
    values = [value for point in points for value in point]
    low, high = min(values), max(values)
    offset = (low + high) / 2
    half_range = (high - low) / 2 or 1.0
    shorts = [round((value - offset) / half_range * 32767) for value in values]
    return offset, half_range, shorts


def sequence(cycle, start, stop, blocks):
    """A sequence that drives nodes, with the text keys after it. The names are those of the table of strings that
    `idle_animation` writes: the sequence, the node and the kind of controller are its first three. The blocks are
    (the interpolator, the index of the name of the node in the table of strings), in the order of the records."""
    # NiSequence: the name, the number of controlled blocks, the growth of the array, the blocks
    data = struct.pack("<iII", 0, len(blocks), 0)
    for interpolator, node in blocks:
        # The block: interpolator, controller, priority, the node, the kind of property, the controller, its id, the
        # interpolator's id
        data += struct.pack("<iiB", interpolator, NONE, 50) + struct.pack("<5i", node, NONE, 2, NONE, NONE)
    # The sequence: weight, text keys, cycle type, frequency, start, stop, manager, accumulation root, animation notes
    data += struct.pack("<fiIfff", 1.0, 1, cycle, 1.0, start, stop)
    data += struct.pack("<ii", NONE, NONE) + struct.pack("<H", 0)
    return data


def idle_animation(
    name="MTIdle", node="Bip01 L Foot", cycle=CLAMP, stop=1.0, offset=-150.0, height=-26.0, spline=False, root_speed=0.0
):
    """A sequence of `stop` seconds that moves the node from its place under the calf, at `height`, to `offset` to the
    side of it, with two keys or, when `spline` is true, with the four control points of a B-spline (the points of a
    line are the ends of it and the places a third and two thirds of the way). When `root_speed` is more than 0 the
    sequence moves the root of the skeleton, Bip01, forward (along y) at that many units a second, as the animations of
    walking and running do: the engine takes the speed at which an animation travels from it."""
    strings = [name, node, "NiTransformController", "Start", "End"]
    keys = [(0.0, (0.0, 0.0, height)), (stop, (offset, 0.0, height))]
    if spline:
        points = [(offset * fraction, 0.0, height) for fraction in (0.0, 1 / 3, 2 / 3, 1.0)]
        bias, half_range, shorts = compact_translation(points)
        records = [
            ("NiBSplineCompTransformInterpolator", spline_interpolator(0.0, stop, 3, 4, bias, half_range)),
            ("NiBSplineData", spline_data(shorts)),
            ("NiBSplineBasisData", struct.pack("<I", len(points))),
        ]
    else:
        records = [("NiTransformInterpolator", transform_interpolator(3)), ("NiTransformData", translation_data(keys))]
    # the records come after the sequence and its text keys, and the blocks of the sequence refer to them by index
    blocks = [(2, 1)]
    if root_speed > 0:
        strings.append("Bip01")
        root_keys = [(0.0, (0.0, 0.0, 0.0)), (stop, (0.0, root_speed * stop, 0.0))]
        blocks.append((2 + len(records), len(strings) - 1))
        records += [
            ("NiTransformInterpolator", transform_interpolator(2 + len(records) + 1)),
            ("NiTransformData", translation_data(root_keys)),
        ]
    return nif([("NiControllerSequence", sequence(cycle, 0.0, stop, blocks)),
                ("NiTextKeyExtraData", text_keys([(0.0, 3), (stop, 4)]))] + records, strings, [0])


if __name__ == "__main__":
    arguments = [argument for argument in sys.argv[1:] if argument != "--spline"]
    if len(arguments) != 1 or len(sys.argv) - len(arguments) > 2:
        sys.exit(__doc__)
    path = Path(arguments[0])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(idle_animation(spline="--spline" in sys.argv[1:]))
    print(path)
