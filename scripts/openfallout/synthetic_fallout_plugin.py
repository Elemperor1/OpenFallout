#!/usr/bin/env python3
"""Write a tiny Fallout 3 format plugin and the one mesh it uses, so the engine can be started on a Fallout cell
without any game data.

The plugin has the records of a plain room: a static that is a cube (used as a floor and as a pillar), the marker
static that Fallout cells use for their entry point, an interior cell "OFTestCell" with the lighting of a Fallout 3
cell, a lighting template that the cell takes its fog colour and far fog distance from, and three references that put
them there. It also has a worldspace "OFTestWorld" with a flat exterior cell, a weather and the climate of the
worldspace that says the weather is the one to have (it lists a second weather too, which needs a global that is 0),
and two more exterior cells that are flagged for water: one has water of its own and the other the largest float as
its height, which the games write for a cell without water of its own. The worldspace names a kind of water (WATR),
black and opaque, with no reflection. Both cells have a person (a race and a character with a skeleton) standing
east of the end of the player's walk. The person has the parts of a body: the race names an upper body, two hands and a
head (boxes of different sizes), the person a hair and a suit that covers the upper body and the right hand, so that the
body of the race is there where the suit is not.
It holds no Bethesda data. The mesh is an OpenSceneGraph text file, a cube, which the engine can load beside the NIF
files of the games.

    scripts/openfallout/synthetic_fallout_plugin.py --out some/data/folder

writes `OFTest.esm`, `meshes/openfallout/cube.osgt`, the meshes of the person (`meshes/openfallout/person_*.osgt`),
`meshes/characters/_male/skeleton.nif` and `textures/sky/oftestclouds.dds` into the folder.
"""
import argparse
import struct
from pathlib import Path

import placeholder_skeleton

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
CONDITIONAL_WEATHER_ID = 0x80E
GLOBAL_ID = 0x80F
WATER_CELL_ID = 0x810
WATER_LAND_ID = 0x811
DRY_CELL_ID = 0x812
DRY_LAND_ID = 0x813
WATER_TYPE_ID = 0x814
RACE_ID = 0x815
NPC_ID = 0x816
NPC_REF_ID = 0x817
EXTERIOR_NPC_REF_ID = 0x818
HAIR_ID = 0x819
SUIT_ID = 0x81A
LAST_ID = SUIT_ID # the largest form ID of the plugin, which the next object ID of its header follows
CELL_NAME = "OFTestCell"
NPC_NAME = "OFTestPerson"
RACE_NAME = "OFTestRace"
# The skeleton of the character, as the record names it: Fallout characters have theirs in the NPC_ record, not in the
# race. The file is the placeholder skeleton of the engine, which is the one with the bounding box.
SKELETON = "characters\\_male\\skeleton.nif"
WORLD_NAME = "OFTestWorld"
WEATHER_NAME = "OFTestWeather"
CONDITIONAL_WEATHER_NAME = "OFTestConditionalWeather"
# The size of an exterior cell of Fallout 3 and New Vegas, in game units. The exterior cell 0,0 covers x and y from 0 to
# this. The cell has an entry marker at its centre, which is where the player starts; a cell without a marker is
# entered at its centre too.
EXTERIOR_CELL_SIZE = 4096.0
# The exterior cell north of the start (grid 0,1) is a shallow basin: its flat terrain is at WATER_TERRAIN, below the
# terrain of the start cell, and it has water at WATER_HEIGHT, a little above its terrain and below the eyes of the
# player, who sees it from above as a band just below the horizon when they look north. The cell east of it (grid 1,0)
# is flagged for water as nearly all the cells of New Vegas are, but holds the largest float as its height, so it has
# the default height of the worldspace, far below its terrain: no water to see.
WATER_CELL = (0, 1)
DRY_CELL = (1, 0)
WATER_TERRAIN = -40.0
WATER_HEIGHT = -20.0
# The kind of water of the worldspace (its NAM2), which the cells take as they name none: black and wholly opaque, with
# no reflection, so that the water shows nothing but the fog in front of it, which is far from what the water of
# Morrowind (white, half transparent) shows over the terrain. The colours are red, green and blue of 0 to 255, the
# opacity is a percentage.
WATER_TYPE_NAME = "OFTestWater"
WATER_SHALLOW = (0, 0, 0)
WATER_DEEP = (0, 0, 0)
WATER_OPACITY = 100
WATER_REFLECTIVITY = 0.0
FLOAT_MAX = 3.4028234663852886e+38
# Edge of the cube in game units. The floor is the cube scaled up so that its top face is at height 0, where the
# player starts, and the pillar is the cube as it is, standing on the floor PILLAR_DISTANCE units north of the start.
CUBE = 256.0
FLOOR_SCALE = 16.0
PILLAR_DISTANCE = 600.0
# How far north of the start the person stands: a little short of the south face of the pillar, so that the player
# who has walked into the pillar and strafes east meets them with the middle of their body.
NPC_DEPTH = PILLAR_DISTANCE - CUBE / 2 - 30.0
# and this many units east of the line of the walk, the player is a box 40 wide and the person a cylinder 40 wide
NPC_DISTANCE = 300.0
# The body of the person: boxes of half extents and centre (x east, y north, z up, from the feet) that are the models of
# the parts. The race has an upper body wider than the suit (so that the suit is seen where both are drawn), a hand on
# each side outside of the cylinder, and a head; the person has a hair on top of it and wears a suit that covers the
# upper body and the right hand (the right hand is the one on the east side, the way the person faces).
PERSON_PARTS = {
    "upperbody": ((18.0, 10.0, 30.0), (0.0, 0.0, 60.0)),
    "lefthand": ((3.0, 3.0, 3.0), (-26.0, 0.0, 50.0)),
    "righthand": ((3.0, 3.0, 3.0), (26.0, 0.0, 50.0)),
    "head": ((8.0, 8.0, 8.0), (0.0, 0.0, 118.0)),
    "hair": ((9.0, 9.0, 3.0), (0.0, 0.0, 130.0)),
    "suit": ((12.0, 8.0, 30.0), (0.0, 0.0, 60.0)),
}
# The biped slots of Fallout 3 that the suit covers: the upper body (0x04) and the right hand (0x10)
SUIT_SLOTS = 0x04 | 0x10

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
# The texture of the clouds, as the record names it (a path under textures, the way the games write it), and the image:
# white pixels of one alpha, so that the colour of the clouds on the screen does not depend on where it is looked at
# or how far they have drifted.
CLOUD_TEXTURE = "sky\\OFTestClouds.dds"
CLOUD_ALPHA = 128
# Where the fog starts and ends, in game units, by day and by night, then its power by day and by night.
WEATHER_FOG_DAY = (300.0, 12000.0)
WEATHER_FOG_NIGHT = (150.0, 6000.0)
# The wind and the glare of the sun, as bytes (the engine makes a share of 1 of them)
WEATHER_WIND = 51
WEATHER_GLARE = 255
# The chance that the climate gives the weather, and the one it gives a second weather that needs a global that is 0:
# the second weather can not be chosen, so whatever the chances are the first one is.
WEATHER_CHANCE = 7
CONDITIONAL_WEATHER_CHANCE = 93


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


def weather(form_id=WEATHER_ID, name=WEATHER_NAME):
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
    # Like NVWastelandClear, three layers of clouds are the blank texture and the clouds are in the fourth
    return record(b"WTHR", form_id, [zstr(b"EDID", name), zstr(b"DNAM", "Sky\\Alpha.dds"),
                                     zstr(b"CNAM", "sky\\alpha.dds"), zstr(b"ANAM", "sky/alpha.dds"),
                                     zstr(b"BNAM", CLOUD_TEXTURE), sub(b"NAM0", bytes(colours)),
                                     sub(b"FNAM", fog), sub(b"DATA", data)])


def cloud_texture(size=4):
    """The image of the clouds, a DDS file of uncompressed 32 bit pixels, white with CLOUD_ALPHA as alpha."""
    pixels = bytes((255, 255, 255, CLOUD_ALPHA)) * (size * size)
    # magic, header size, flags (caps, height, width, pitch, pixel format), height, width, pitch, depth, mip maps, 11
    # reserved, then the pixel format: size, flags (alpha and rgb), four character code, bits, the masks of red, green,
    # blue and alpha, then the caps (texture) and 4 more words
    header = struct.pack("<4sIIIIIII11I", b"DDS ", 124, 0x100F, size, size, size * 4, 0, 0, *(0,) * 11)
    pixel_format = struct.pack("<IIIIIIII", 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    return header + pixel_format + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + pixels


def water_type():
    """A WATR record with its settings in a DNAM of 196 bytes: the sun power, the reflectivity and the fresnel amount
    after 16 bytes, then the colours (red, green, blue and an unused byte) of shallow water, deep water and the
    reflection, the rest are zeros."""
    visual = (bytes(16) + struct.pack("<fff", 826.0, WATER_REFLECTIVITY, 0.75) + bytes(4) + struct.pack("<ff", 0.0, 0.0)
              + bytes(WATER_SHALLOW) + b"\xff" + bytes(WATER_DEEP) + b"\xff" + bytes((10, 10, 10)) + b"\xff")
    visual += bytes(196 - len(visual))
    return record(b"WATR", WATER_TYPE_ID, [zstr(b"EDID", WATER_TYPE_NAME), sub(b"ANAM", bytes((WATER_OPACITY,))),
                                           sub(b"FNAM", b"\x02"), sub(b"DATA", bytes(2)), sub(b"DNAM", visual)])


def global_variable():
    """A GLOB record, a float that is 0."""
    return record(b"GLOB", GLOBAL_ID, [zstr(b"EDID", "OFTestWeatherGlobal"), sub(b"FNAM", b"f"),
                                       sub(b"FLTV", struct.pack("<f", 0.0))])


def climate():
    """A CLMT record that lists the weather and one that needs a global that is 0, with the times of sunrise and
    sunset."""
    weathers = (struct.pack("<IiI", WEATHER_ID, WEATHER_CHANCE, 0)
                + struct.pack("<IiI", CONDITIONAL_WEATHER_ID, CONDITIONAL_WEATHER_CHANCE, GLOBAL_ID))
    return record(b"CLMT", CLIMATE_ID, [zstr(b"EDID", "OFTestClimate"), sub(b"WLST", weathers),
                                        sub(b"TNAM", struct.pack("<6B", 36, 42, 108, 114, 0, 0))])


def land(form_id=EXTERIOR_LAND_ID, height=0.0):
    """A LAND record: a flat terrain at the height over the whole cell, 33 by 33 vertices with normals pointing up and no
    textures (the engine uses the default one of the game)."""
    flags = 0x1 | 0x2  # has normals and heights
    normals = bytes((0, 0, 127)) * (33 * 33)
    # The height of the first vertex in steps of 8 units, then for each vertex the difference to the one before it (a
    # signed byte, in steps of 8 units), then 3 bytes of padding.
    heights = struct.pack("<f", height / 8) + bytes(33 * 33) + bytes(3)
    return record(b"LAND", form_id, [sub(b"DATA", struct.pack("<I", flags)), sub(b"VNML", normals),
                                     sub(b"VHGT", heights)])


def part_path(name):
    """The path of the mesh of a part of the person, as a record names it (under meshes)."""
    return "openfallout\\person_%s.osgt" % name


def part(index, name):
    """The INDX and MODL of a part of the body or head of a race."""
    return [sub(b"INDX", struct.pack("<I", index)), zstr(b"MODL", part_path(name))]


def race():
    """A race with the data that the loader needs of one and the parts of a body that Fallout 3 gives it: the head (part
    0 of the 8 of the head data) and the upper body and the two hands (parts 0 to 2 of the 4 of the body data), for men.
    The skeleton is in the NPC_ record."""
    skills = struct.pack("<16B", *([0] * 16))
    data = skills + struct.pack("<4fI", 1.0, 1.0, 1.0, 1.0, 1)
    return record(b"RACE", RACE_ID, [zstr(b"EDID", RACE_NAME), sub(b"DATA", data),
                                     sub(b"NAM0"), sub(b"MNAM"), *part(0, "head"), sub(b"FNAM"),
                                     sub(b"NAM1"), sub(b"MNAM"), *part(0, "upperbody"), *part(1, "lefthand"),
                                     *part(2, "righthand"), sub(b"FNAM")])


def hair():
    """A HAIR record with a model."""
    return record(b"HAIR", HAIR_ID, [zstr(b"EDID", "OFTestHair"), zstr(b"FULL", "OpenFallout Test Hair"),
                                     zstr(b"MODL", part_path("hair")), zstr(b"ICON", "openfallout\\hair.dds"),
                                     sub(b"DATA", b"\x00")])


def suit():
    """An ARMO record of Fallout 3: the model of a man in MODL, the biped slots and the general flags in BMDT, then the
    value, health and weight."""
    return record(b"ARMO", SUIT_ID, [zstr(b"EDID", "OFTestSuit"), zstr(b"FULL", "OpenFallout Test Suit"),
                                     zstr(b"MODL", part_path("suit")), sub(b"BMDT", struct.pack("<II", SUIT_SLOTS, 0)),
                                     sub(b"DATA", struct.pack("<IIf", 1, 1, 1.0))])


def person():
    """A character of that race, with the skeleton of the placeholder, as a Fallout record names it (MODL)."""
    # ACBS of Fallout 3 and New Vegas: flags, fatigue, barter gold, level, calc min, calc max, speed multiplier, karma,
    # disposition base, template flags (24 bytes); none of the flags is set, so the record has its own traits.
    acbs = struct.pack("<IHHhHHHfhH", 0, 0, 0, 1, 1, 1, 100, 0.0, 0, 0)
    return record(b"NPC_", NPC_ID, [zstr(b"EDID", NPC_NAME), zstr(b"FULL", "OpenFallout Test Person"),
                                    zstr(b"MODL", SKELETON), sub(b"ACBS", acbs),
                                    sub(b"CNTO", struct.pack("<II", SUIT_ID, 1)),
                                    sub(b"RNAM", struct.pack("<I", RACE_ID)),
                                    sub(b"HNAM", struct.pack("<I", HAIR_ID))])


def achr(form_id, position):
    """A placed character: the base record and where it stands."""
    return record(b"ACHR", form_id, [sub(b"NAME", struct.pack("<I", NPC_ID)),
                                     sub(b"DATA", struct.pack("<6f", *position, 0.0, 0.0, 0.0))])


def worldspace():
    """The WRLD record of OFTestWorld followed by its children: the exterior cell 0,0 in an exterior block and sub-block
    (both labelled with the grid 0,0), with its references in the temporary children group of the cell."""
    land_level, water_level = -2700.0, -14000.0
    wrld = record(b"WRLD", WORLD_ID, [zstr(b"EDID", WORLD_NAME), zstr(b"FULL", "OpenFallout Test World"),
                                      sub(b"NAM0", struct.pack("<ff", -4096.0, -4096.0)),
                                      sub(b"NAM9", struct.pack("<ff", 8192.0, 8192.0)),
                                      sub(b"CNAM", struct.pack("<I", CLIMATE_ID)),
                                      sub(b"NAM2", struct.pack("<I", WATER_TYPE_ID)), sub(b"DATA", b"\x00"),
                                      sub(b"DNAM", struct.pack("<ff", land_level, water_level))])
    # DATA of an exterior cell is its flags (0: not an interior, no water), XCLC the grid and the flags of the land.
    cell = record(b"CELL", EXTERIOR_CELL_ID, [zstr(b"EDID", WORLD_NAME + "Cell"), sub(b"DATA", b"\x00"),
                                              sub(b"XCLC", struct.pack("<iiI", 0, 0, 0))])
    # Two more cells with flat terrain and water flagged: one with a height of its own, one with the largest float.
    water_cell = record(b"CELL", WATER_CELL_ID, [zstr(b"EDID", WORLD_NAME + "WaterCell"), sub(b"DATA", b"\x02"),
                                                 sub(b"XCLC", struct.pack("<iiI", *WATER_CELL, 0)),
                                                 sub(b"XCLW", struct.pack("<f", WATER_HEIGHT))])
    dry_cell = record(b"CELL", DRY_CELL_ID, [zstr(b"EDID", WORLD_NAME + "DryCell"), sub(b"DATA", b"\x02"),
                                             sub(b"XCLC", struct.pack("<iiI", *DRY_CELL, 0)),
                                             sub(b"XCLW", struct.pack("<f", FLOAT_MAX))])
    half = EXTERIOR_CELL_SIZE / 2
    refs = group(struct.pack("<I", EXTERIOR_CELL_ID), 9, [
        land(),
        refr(EXTERIOR_PILLAR_ID, CUBE_ID, (half, half + PILLAR_DISTANCE, CUBE / 2)),
        refr(EXTERIOR_MARKER_REF_ID, MARKER_ID, (half, half, 0.0)),
        achr(EXTERIOR_NPC_REF_ID, (half + NPC_DISTANCE, half + NPC_DEPTH, 0.0)),
    ])
    children = group(struct.pack("<I", EXTERIOR_CELL_ID), 6, [refs])
    water_children = group(struct.pack("<I", WATER_CELL_ID), 6, [
        group(struct.pack("<I", WATER_CELL_ID), 9, [land(WATER_LAND_ID, WATER_TERRAIN)])])
    dry_children = group(struct.pack("<I", DRY_CELL_ID), 6, [
        group(struct.pack("<I", DRY_CELL_ID), 9, [land(DRY_LAND_ID)])])
    grid = struct.pack("<hh", 0, 0)
    sub_block = group(grid, 5, [cell, children, water_cell, water_children, dry_cell, dry_children])
    block = group(grid, 4, [sub_block])
    return [wrld, group(struct.pack("<I", WORLD_ID), 1, [block])]


def plugin():
    hedr = struct.pack("<fiI", 0.94, 18, LAST_ID + 1)
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
        achr(NPC_REF_ID, (NPC_DISTANCE, NPC_DEPTH, 0.0)),
    ])
    children = group(struct.pack("<I", CELL_ID), 6, [refs])
    sub_block = group(struct.pack("<i", CELL_ID // 10 % 10), 3, [cell, children])
    block = group(struct.pack("<i", CELL_ID % 10), 2, [sub_block])
    return (header + top_group(b"STAT", [cube, marker]) + top_group(b"LGTM", [template])
            + top_group(b"GLOB", [global_variable()])
            + top_group(b"WTHR", [weather(), weather(CONDITIONAL_WEATHER_ID, CONDITIONAL_WEATHER_NAME)])
            + top_group(b"CLMT", [climate()]) + top_group(b"WATR", [water_type()])
            + top_group(b"RACE", [race()]) + top_group(b"HAIR", [hair()]) + top_group(b"ARMO", [suit()])
            + top_group(b"NPC_", [person()])
            + top_group(b"CELL", [block]) + top_group(b"WRLD", worldspace()))


def box_mesh(half_extents=(CUBE / 2,) * 3, center=(0.0, 0.0, 0.0)):
    """An OpenSceneGraph text file with one box of the half extents around the centre; with the defaults, a cube of edge
    CUBE centred on its origin."""
    x, y, z = half_extents
    cx, cy, cz = center
    corners = [(-x, -y, -z), (x, -y, -z), (x, y, -z), (-x, y, -z), (-x, -y, z), (x, -y, z), (x, y, z), (-x, y, z)]
    corners = [(a + cx, b + cy, c + cz) for a, b, c in corners]
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
    mesh.write_text(box_mesh(), encoding="ascii")
    for name, (half_extents, center) in PERSON_PARTS.items():
        (out / "meshes" / Path(part_path(name).replace("\\", "/"))).write_text(box_mesh(half_extents, center),
                                                                           encoding="ascii")
    texture = out / "textures" / "sky" / "oftestclouds.dds"
    texture.parent.mkdir(parents=True, exist_ok=True)
    texture.write_bytes(cloud_texture())
    skeleton = out / "meshes" / Path(SKELETON.replace("\\", "/"))
    skeleton.parent.mkdir(parents=True, exist_ok=True)
    skeleton.write_bytes(placeholder_skeleton.skeleton())
    (out / "OFTest.esm").write_bytes(plugin())
    return out / "OFTest.esm"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, type=Path, help="folder to write into (created if missing)")
    print(write(parser.parse_args().out))
