#!/usr/bin/env python3
"""Write a tiny Fallout 3 format plugin and the one mesh it uses, so the engine can be started on a Fallout cell
without any game data.

The plugin has the records of a plain room: a static that is a cube (used as a floor and as a pillar), the marker
static that Fallout cells use for their entry point, an interior cell "OFTestCell" with the lighting of a Fallout 3
cell, a lighting template that the cell takes its fog colour and far fog distance from, and three references that put
them there. It also has a worldspace "OFTestWorld" with a flat exterior cell, a weather and the climate of the
worldspace that says the weather is the one to have. It holds no Bethesda data. The mesh is an OpenSceneGraph text
file, a cube, which the engine can load beside the NIF files of the games.

    scripts/openfallout/synthetic_fallout_plugin.py --out some/data/folder

writes `OFTest.esm` and `meshes/openfallout/cube.osgt` into the folder.
"""
import argparse
import struct
from pathlib import Path

CUBE_ID = 0x800
CELL_ID = 0x801
FLOOR_ID = 0x802
PILLAR_ID = 0x803
MARKER_ID = 0x804
MARKER_REF_ID = 0x805
TEMPLATE_ID = 0x806
WORLD_ID = 0x807
EXTERIOR_CELL_ID = 0x808
EXTERIOR_LAND_ID = 0x809
EXTERIOR_PILLAR_ID = 0x80A
EXTERIOR_MARKER_REF_ID = 0x80B
WEATHER_ID = 0x80C
CLIMATE_ID = 0x80D
CELL_NAME = "OFTestCell"
WORLD_NAME = "OFTestWorld"
WEATHER_NAME = "OFTestWeather"
# The size of an exterior cell of Fallout 3 and New Vegas, in game units. The exterior cell 0,0 covers x and y from 0 to
# this. The cell has an entry marker at its centre, which is where the player starts; a cell without a marker is
# entered at its centre too.
EXTERIOR_CELL_SIZE = 4096.0
# Edge of the cube in game units. The floor is the cube scaled up so that its top face is at height 0, where the
# player starts, and the pillar is the cube as it is, standing on the floor PILLAR_DISTANCE units north of the start.
CUBE = 256.0
FLOOR_SCALE = 16.0
PILLAR_DISTANCE = 600.0

# The lighting of the cell, in the order of the XCLL sub-record. The cell inherits the fog colour and the far fog
# distance of its lighting template (inherit flags 0x04 and 0x10), the rest is its own: what the engine should use is
# the cell's ambient and directional colours, the template's fog colour and far distance, and the cell's near distance.
AMBIENT = (90, 90, 100)
DIRECTIONAL = (200, 190, 160)
CELL_FOG = (20, 25, 30)
CELL_FOG_FAR = 6000.0
TEMPLATE_FOG = (120, 110, 100)
TEMPLATE_FOG_FAR = 2500.0
FOG_NEAR = 100.0
INHERIT_FOG_COLOR_AND_FAR = 0x04 | 0x10

# The weather: for each of the ten colour types of NAM0 (sky upper, fog, clouds lower, ambient, sunlight, sun, stars,
# sky lower, horizon, clouds upper) the colours at sunrise, day, sunset and night. Only the types the engine uses are
# distinct, the others are black. It has 4 times of day like the weathers of Fallout 3, those of New Vegas have 6.
WEATHER_SKY = ((200, 120, 60), (30, 80, 200), (210, 90, 40), (0, 0, 40))
WEATHER_FOG = ((190, 150, 110), (200, 190, 170), (180, 120, 90), (10, 10, 20))
WEATHER_AMBIENT = ((90, 70, 60), (70, 80, 90), (80, 60, 60), (10, 10, 30))
WEATHER_SUNLIGHT = ((255, 200, 150), (255, 240, 200), (255, 160, 100), (0, 0, 0))
WEATHER_SUN = ((255, 255, 255), (255, 255, 255), (255, 120, 40), (0, 0, 0))
# Where the fog starts and ends, in game units, by day and by night, then its power by day and by night.
WEATHER_FOG_DAY = (300.0, 12000.0)
WEATHER_FOG_NIGHT = (150.0, 6000.0)
# The wind and the glare of the sun, as bytes (the engine makes a share of 1 of them)
WEATHER_WIND = 51
WEATHER_GLARE = 255
# The chance that the climate gives the weather. It is the only one, so whatever it is the weather is the one chosen.
WEATHER_CHANCE = 7


def sub(code, data=b""):
    return code + struct.pack("<H", len(data)) + data


def zstr(code, text):
    return sub(code, text.encode() + b"\0")


def record(code, form_id, subs, flags=0):
    data = b"".join(subs)
    # Fallout 3 header: size, flags, form ID, revision, form version, version control information (24 bytes).
    return code + struct.pack("<IIIIHH", len(data), flags, form_id, 0, 15, 0) + data


def group(label, group_type, children):
    body = b"".join(children)
    return b"GRUP" + struct.pack("<I", 24 + len(body)) + label + struct.pack("<iHHHH", group_type, 0, 0, 0, 0) + body


def top_group(code, children):
    return group(code, 0, children)


def refr(form_id, base, position, scale=None):
    subs = [sub(b"NAME", struct.pack("<I", base))]
    if scale is not None:
        subs.append(sub(b"XSCL", struct.pack("<f", scale)))
    subs.append(sub(b"DATA", struct.pack("<6f", *position, 0.0, 0.0, 0.0)))
    return record(b"REFR", form_id, subs)


def lighting(ambient, directional, fog, fog_near, fog_far):
    """A 40 byte lighting struct as Fallout 3 writes it, in the XCLL of a cell and in the DATA of a lighting template:
    ambient, directional and fog colours (RGBA bytes), fog near and far, the direction as two integers, the fade and
    clip distance of the fog and its power."""
    data = struct.pack("<12Bffiifff", *ambient, 0, *directional, 0, *fog, 0, fog_near, fog_far, 0, 0, 1.0, 4000.0, 1.0)
    assert len(data) == 40
    return data


def weather():
    """A WTHR record with 4 times of day in NAM0, fog distances, wind and glare."""
    colours = bytearray()
    for kind in (WEATHER_SKY, WEATHER_FOG, ((0, 0, 0),) * 4, WEATHER_AMBIENT, WEATHER_SUNLIGHT, WEATHER_SUN,
                 ((0, 0, 0),) * 4, ((0, 0, 0),) * 4, ((0, 0, 0),) * 4, ((0, 0, 0),) * 4):
        for colour in kind:
            colours += bytes(colour) + b"\0"
    assert len(colours) == 160
    fog = struct.pack("<6f", *WEATHER_FOG_DAY, *WEATHER_FOG_NIGHT, 1.0, 1.0)
    # wind speed, cloud speeds, transition delta, sun glare, sun damage, precipitation and thunder fade, thunder
    # frequency, classification (0: none) and the colour of lightning
    data = struct.pack("<15B", WEATHER_WIND, 0, 0, 4, WEATHER_GLARE, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255)
    return record(b"WTHR", WEATHER_ID, [zstr(b"EDID", WEATHER_NAME), sub(b"NAM0", bytes(colours)), sub(b"FNAM", fog),
                                        sub(b"DATA", data)])


def climate():
    """A CLMT record that lists the weather, with the times of sunrise and sunset."""
    return record(b"CLMT", CLIMATE_ID, [zstr(b"EDID", "OFTestClimate"),
                                        sub(b"WLST", struct.pack("<IiI", WEATHER_ID, WEATHER_CHANCE, 0)),
                                        sub(b"TNAM", struct.pack("<6B", 36, 42, 108, 114, 0, 0))])


def land():
    """A LAND record: a flat terrain at height 0 over the whole cell, 33 by 33 vertices with normals pointing up and no
    textures (the engine uses the default one of the game)."""
    flags = 0x1 | 0x2  # has normals and heights
    normals = bytes((0, 0, 127)) * (33 * 33)
    # The height of the first vertex, then for each vertex the difference to the one before it (a signed byte, in steps
    # of 8 units), then 3 bytes of padding.
    heights = struct.pack("<f", 0.0) + bytes(33 * 33) + bytes(3)
    return record(b"LAND", EXTERIOR_LAND_ID, [sub(b"DATA", struct.pack("<I", flags)), sub(b"VNML", normals),
                                              sub(b"VHGT", heights)])


def worldspace():
    """The WRLD record of OFTestWorld followed by its children: the exterior cell 0,0 in an exterior block and sub-block
    (both labelled with the grid 0,0), with its references in the temporary children group of the cell."""
    land_level, water_level = -2700.0, -14000.0
    wrld = record(b"WRLD", WORLD_ID, [zstr(b"EDID", WORLD_NAME), zstr(b"FULL", "OpenFallout Test World"),
                                      sub(b"NAM0", struct.pack("<ff", -4096.0, -4096.0)),
                                      sub(b"NAM9", struct.pack("<ff", 8192.0, 8192.0)),
                                      sub(b"CNAM", struct.pack("<I", CLIMATE_ID)),
                                      sub(b"DATA", b"\x00"), sub(b"DNAM", struct.pack("<ff", land_level, water_level))])
    # DATA of an exterior cell is its flags (0: not an interior, no water), XCLC the grid and the flags of the land.
    cell = record(b"CELL", EXTERIOR_CELL_ID, [zstr(b"EDID", WORLD_NAME + "Cell"), sub(b"DATA", b"\x00"),
                                              sub(b"XCLC", struct.pack("<iiI", 0, 0, 0))])
    half = EXTERIOR_CELL_SIZE / 2
    refs = group(struct.pack("<I", EXTERIOR_CELL_ID), 9, [
        land(),
        refr(EXTERIOR_PILLAR_ID, CUBE_ID, (half, half + PILLAR_DISTANCE, CUBE / 2)),
        refr(EXTERIOR_MARKER_REF_ID, MARKER_ID, (half, half, 0.0)),
    ])
    children = group(struct.pack("<I", EXTERIOR_CELL_ID), 6, [refs])
    grid = struct.pack("<hh", 0, 0)
    sub_block = group(grid, 5, [cell, children])
    block = group(grid, 4, [sub_block])
    return [wrld, group(struct.pack("<I", WORLD_ID), 1, [block])]


def plugin():
    hedr = struct.pack("<fiI", 0.94, 16, CLIMATE_ID + 1)
    header = record(b"TES4", 0, [sub(b"HEDR", hedr), zstr(b"CNAM", "OpenFallout"),
                                 zstr(b"SNAM", "synthetic test plugin")], flags=1)
    half = int(CUBE / 2)
    cube = record(b"STAT", CUBE_ID, [zstr(b"EDID", "OFTestCube"),
                                     sub(b"OBND", struct.pack("<6h", -half, -half, -half, half, half, half)),
                                     zstr(b"MODL", "openfallout\\cube.osgt")])
    # The static that real cells use to say where to enter them, with no model.
    marker = record(b"STAT", MARKER_ID, [zstr(b"EDID", "COCMarkerHeading")])
    template = record(b"LGTM", TEMPLATE_ID, [
        zstr(b"EDID", "OFTestLighting"),
        sub(b"DATA", lighting((1, 2, 3), (4, 5, 6), TEMPLATE_FOG, 1.0, TEMPLATE_FOG_FAR))])
    xcll = lighting(AMBIENT, DIRECTIONAL, CELL_FOG, FOG_NEAR, CELL_FOG_FAR)
    cell = record(b"CELL", CELL_ID, [zstr(b"EDID", CELL_NAME), zstr(b"FULL", "OpenFallout Test Cell"),
                                     sub(b"DATA", b"\x01"), sub(b"XCLL", xcll),
                                     sub(b"LTMP", struct.pack("<I", TEMPLATE_ID)),
                                     sub(b"LNAM", struct.pack("<I", INHERIT_FOG_COLOR_AND_FAR))])
    refs = group(struct.pack("<I", CELL_ID), 9, [
        refr(FLOOR_ID, CUBE_ID, (0.0, 0.0, -CUBE * FLOOR_SCALE / 2), FLOOR_SCALE),
        refr(PILLAR_ID, CUBE_ID, (0.0, PILLAR_DISTANCE, CUBE / 2)),
        refr(MARKER_REF_ID, MARKER_ID, (0.0, 0.0, 0.0)),
    ])
    children = group(struct.pack("<I", CELL_ID), 6, [refs])
    sub_block = group(struct.pack("<i", CELL_ID // 10 % 10), 3, [cell, children])
    block = group(struct.pack("<i", CELL_ID % 10), 2, [sub_block])
    return (header + top_group(b"STAT", [cube, marker]) + top_group(b"LGTM", [template])
            + top_group(b"WTHR", [weather()]) + top_group(b"CLMT", [climate()])
            + top_group(b"CELL", [block]) + top_group(b"WRLD", worldspace()))


def cube_mesh():
    """An OpenSceneGraph text file with one cube of edge CUBE, centred on its origin."""
    x = y = z = CUBE / 2
    corners = [(-x, -y, -z), (x, -y, -z), (x, y, -z), (-x, y, -z), (-x, -y, z), (x, -y, z), (x, y, z), (-x, y, z)]
    # Corner indices of each face, counter-clockwise seen from outside, with the normal of that face.
    faces = [((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)), ((0, 1, 5, 4), (0, -1, 0)),
             ((2, 3, 7, 6), (0, 1, 0)), ((1, 2, 6, 5), (1, 0, 0)), ((3, 0, 4, 7), (-1, 0, 0))]
    vertices, normals, indices = [], [], []
    for face, normal in faces:
        base = len(vertices)
        vertices.extend(corners[i] for i in face)
        normals.extend([normal] * 4)
        indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))

    def array(name, array_id, rows):
        return (["      %s TRUE {" % name, "        osg::Vec3Array {", "          UniqueID %d" % array_id,
                 "          Binding BIND_PER_VERTEX", "          vector %d {" % len(rows)]
                + ["            %g %g %g" % row for row in rows] + ["          }", "        }", "      }"])

    lines = ["#Ascii Scene", "#Version 161", "#Generator OpenFallout synthetic_fallout_plugin.py", "",
             "osg::Geode {", "  UniqueID 1", "  Drawables 1 {", "    osg::Geometry {", "      UniqueID 2",
             "      PrimitiveSetList 1 {", "        osg::DrawElementsUShort {", "          UniqueID 3",
             "          Mode TRIANGLES", "          vector %d {" % len(indices)]
    lines += ["            " + " ".join(str(i) for i in indices[n:n + 6]) for n in range(0, len(indices), 6)]
    lines += ["          }", "        }", "      }"]
    lines += array("VertexArray", 4, vertices) + array("NormalArray", 5, normals)
    lines += ["    }", "  }", "}", ""]
    return "\n".join(lines)


def write(out):
    out = Path(out)
    mesh = out / "meshes" / "openfallout" / "cube.osgt"
    mesh.parent.mkdir(parents=True, exist_ok=True)
    mesh.write_text(cube_mesh(), encoding="ascii")
    (out / "OFTest.esm").write_bytes(plugin())
    return out / "OFTest.esm"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, type=Path, help="folder to write into (created if missing)")
    print(write(parser.parse_args().out))
