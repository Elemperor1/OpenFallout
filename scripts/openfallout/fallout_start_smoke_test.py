#!/usr/bin/env python3
"""Start the engine in an interior cell and in an exterior cell of a synthetic Fallout 3 format plugin, walk the
player into a wall and check what happened.

A content list made only of Fallout plugins has no player, race, class, skills or settings in the record format of the
engine, so the world has to make placeholders to start. The unit tests cover those records one by one; this runs them
together with the rest of the engine: the plugin and its mesh come from synthetic_fallout_plugin.py, the engine is
started twice with `--skip-menu`, once with `--start OFTestCell` (an interior cell) and once with
`--start OFTestWorld:0,0` (the exterior cell 0,0 of a worldspace), under a virtual display (Xvfb with Mesa software
rendering is enough), and a Lua player script walks north from the start for a few seconds, casts a ray down and
quits. The checks are on the log: the cell is the one asked for, in the interior it is lit with its own ambient and
sun colours and with the fog colour and far distance of its lighting template, in the exterior the weather is the one
that the climate of the worldspace lists, the player stands on the floor the whole time, the camera is at eye height
above the player, the player stops at the pillar that is in the way and, strafing east from there, at the person who
stands in the way (a character with a skeleton in its record, which is solid as an actor is: a ray that looks for actors
finds it) and whose body can be seen (rays at what is drawn find the suit it wears, its head, its hair and the hand that
the suit does not cover, and none the parts of its race that the suit takes the place of) and, strafing west from
there, at a wall that is not drawn (a mesh with only Havok collision: a ray at what is drawn goes through it, the player
and a ray at the world stop at it), nothing logs an error from Lua, and the engine quits by itself. When ImageMagick's `import` is installed,
pixels of the screen are checked too: where nothing is drawn it has the colour of the fog of the cell or of the weather,
and in the exterior, where the player looks up after three seconds, the sky overhead has the sky colour of the weather
with the clouds of the weather over it, and a band of water in a cell north of the start shows where the horizon would
be (it is black, as the kind of water of the worldspace says, so darker than the fog). The water of the cell that has a
height of its own is logged once, with its kind of water, and the cell that holds the largest float as its height has
none. The game has no files of the sky of Morrowind here, and the log must not mention a texture or a mesh of the sky as
missing.

    scripts/openfallout/fallout_start_smoke_test.py --build build

It needs no game data and no data of Bethesda. The engine's configuration and logs go to a temporary home directory.
The exit status is 0 when every check holds. It also checks that files/data/meshes/placeholder_skeleton.nif is what
placeholder_skeleton.py writes.
"""
import argparse
import os
import pty
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import placeholder_skeleton
import synthetic_fallout_plugin as plugin

ROOT = Path(__file__).resolve().parents[2]

# Where the walk ends, in the units of the game, relative to where it starts: the pillar is PILLAR_DISTANCE north of the
# start and a cube wide, the player a box 40 wide, so it comes to a halt about 20 units short of the pillar's south
# face.
PILLAR_FACE = plugin.PILLAR_DISTANCE - plugin.CUBE / 2
STOP_RANGE = (PILLAR_FACE - 60.0, PILLAR_FACE)


class Scenario:
    """One start of the engine: the text after --start, how the engine names the cell in its log, whether the cell is an
    exterior one, where in the world the player starts (the entry marker of the cell), whether the cell has the
    lighting that the plugin gives the interior cell and the pixels of the screen to check. The exterior cell has the
    weather of the climate of its worldspace instead of the lighting.

    Each pixel is (what it shows, when it is read, where on the screen, the colour it should have): the colour of the
    screen where nothing is drawn is the colour of the fog, and the sky overhead has the sky colour of the weather. A
    fifth item changes what is checked. "darker": each channel of the pixel must be at least WATER_DIFFERENCE below the
    one of the colour given (the water is black and opaque, so it shows only the fog in front of it, and that is
    darker than the colour of the fog that the horizon has). "tinted": the pixel must have the hue of the colour given:
    its strongest channel is the same as that of the colour and at least TINT_MINIMUM (so a black pixel does not
    pass), and the others are less than half of it (the ground has a texture of one colour, and what the lighting and
    the fog do to it is not known to the test; the colour that the engine gives a texture that it can not find is
    magenta)."""

    def __init__(self, name, start, loaded, exterior, origin, lit, pixels):
        self.name, self.start, self.loaded, self.exterior, self.origin, self.lit = (
            name, start, loaded, exterior, origin, lit)
        self.pixels = pixels


# Where on the screen to look: the top left of the window of the game in the interior cell, which is the fog colour when
# the player looks north; in the exterior, the horizon on the left of the middle of the window at the start (the sky
# has no colour of its own there, it is the fog colour) and, after the player has looked up, the sky and its clouds a
# little to the left of the middle (the middle is the cross hair). How far from the expected colour a channel may be: the game is at
# about nine in the morning, when the weather is still a little on its way from the colours of sunrise to those of the
# day.
FOG_PIXEL_INTERIOR = (300, 100)
FOG_PIXEL_EXTERIOR = (300, 360)
SKY_PIXEL = (500, 360)
PIXEL_TOLERANCE = 12
# A pixel of the band of water of the cell north of the start, on the right of the pillar, a little below the horizon. It
# is the colour of the fog where there is no water, and about (191,119,164) with the water of Morrowind (white, half
# transparent, over the terrain). The kind of water of the worldspace is black and opaque: how far below the colour of
# the fog each channel of the pixel must be for that water to be there.
WATER_PIXEL = (900, 380)
WATER_DIFFERENCE = 40
# A pixel of the ground a little before the player, below the pillar at the bottom of the window. The texture of the
# ground is red; the default texture of the game is not in the data files of the test.
GROUND_PIXEL = (640, 600)
# The least that the strongest channel of a tinted pixel has, which a black pixel (nothing drawn there) does not
TINT_MINIMUM = 40

# The colour of the sky overhead with the clouds of the weather over it. Their texture is white with one alpha, and they
# are drawn over the sky in the colour of the fog of the weather with a little added (0.13 of the range of a colour).
def clouded_sky(sky, fog, alpha):
    cloud = [min(255.0, f + 0.13 * 255) for f in fog]
    return tuple(round(c * alpha / 255 + s * (1 - alpha / 255)) for c, s in zip(cloud, sky))


# The seconds after which the walk script has logged its position, the first when the player stands and looks at the
# horizon, the second when it has looked up.
HORIZON_TIME = 2.0
ZENITH_TIME = 5.0

SCENARIOS = [
    Scenario("interior", plugin.CELL_NAME, plugin.CELL_NAME, False, (0.0, 0.0), True,
             [("fog", HORIZON_TIME, FOG_PIXEL_INTERIOR, plugin.TEMPLATE_FOG)]),
    Scenario("exterior", f"{plugin.WORLD_NAME}:0,0", f"{plugin.WORLD_NAME}Cell (0, 0)", True,
             (plugin.EXTERIOR_CELL_SIZE / 2, plugin.EXTERIOR_CELL_SIZE / 2), False,
             [("fog", HORIZON_TIME, FOG_PIXEL_EXTERIOR, plugin.WEATHER_FOG[1]),
              ("water", HORIZON_TIME, WATER_PIXEL, plugin.WEATHER_FOG[1], "darker"),
              ("ground", HORIZON_TIME, GROUND_PIXEL, plugin.GROUND_COLOUR, "tinted"),
              ("sky and clouds", ZENITH_TIME, SKY_PIXEL,
               clouded_sky(plugin.WEATHER_SKY[1], plugin.WEATHER_FOG[1], plugin.CLOUD_ALPHA))]),
]

WALK_SECONDS = 17

# The eight cells around the exterior cell 0,0 that the player starts in.
NEIGHBOURS = [(x, y) for x in (-1, 0, 1) for y in (-1, 0, 1) if (x, y) != (0, 0)]

# What the engine logs about the lighting of the cell: its own ambient and sun colours and fog near distance, and the
# fog colour and far distance that it takes from its lighting template.
LIGHTING_LINE = ("Cell lighting: ambient {}, directional {}, fog {}, fog range {:.6f} to {:.6f}".format(
    *(",".join(map(str, colour)) for colour in (plugin.AMBIENT, plugin.DIRECTIONAL, plugin.TEMPLATE_FOG)),
    plugin.FOG_NEAR, plugin.TEMPLATE_FOG_FAR))

# What the engine logs about the weather it chose for the exterior cell, from the climate of the worldspace: the weather
# record with its colours at day (and the sky at night), the texture of its clouds (the fourth layer, the other three are
# blank) and the fog distances.
WEATHER_LINE = ("Weather: {}, sky day {} night {}, fog day {}, ambient day {}, sunlight day {}, clouds {}, "
                "fog range {:.6f} to {:.6f} day, {:.6f} to {:.6f} night".format(
                    plugin.WEATHER_NAME, *(",".join(map(str, colour)) for colour in (
                        plugin.WEATHER_SKY[1], plugin.WEATHER_SKY[3], plugin.WEATHER_FOG[1], plugin.WEATHER_AMBIENT[1],
                        plugin.WEATHER_SUNLIGHT[1])), plugin.CLOUD_TEXTURE, *plugin.WEATHER_FOG_DAY,
                    *plugin.WEATHER_FOG_NIGHT))

# What the engine logs about the water of an exterior cell: only the cell that has a height of its own has water to see
# (the other cell that is flagged for water has the default height of the worldspace, below its terrain), and it has
# the kind of water of its worldspace.
WATER_LINE = "Water of cell {}WaterCell ({}, {}) at height {:g}, water type {} (opacity {}%, reflectivity {:g})".format(
    plugin.WORLD_NAME, *plugin.WATER_CELL, plugin.WATER_HEIGHT, plugin.WATER_TYPE_NAME, plugin.WATER_OPACITY,
    plugin.WATER_REFLECTIVITY)

# The middle of the cell with water, where the script casts a ray down at the water (a cell is 4096 units wide)
WATER_CENTRE = ((plugin.WATER_CELL[0] + 0.5) * 4096, (plugin.WATER_CELL[1] + 0.5) * 4096)

# From the middle of the cell with water, 10 units under its surface, to the east across the edge of the cell
WATER_SIDE_RAY = (WATER_CENTRE[0], WATER_CENTRE[1], plugin.WATER_HEIGHT - 10, WATER_CENTRE[0] + 4096,
                  WATER_CENTRE[1], plugin.WATER_HEIGHT - 10)

# Where the strafe starts (the second of the walk script), the radius of the person's body, and where the player ends
# strafing east, relative to where it starts: the person is NPC_DISTANCE away and the player a box 40 wide, so it comes
# to a halt about 20 + 20 short of the middle of them
STRAFE_FROM = 8.0
PERSON_RADIUS = 20.0
PERSON_STOP = (plugin.NPC_DISTANCE - 40.0 - 30.0, plugin.NPC_DISTANCE - 40.0 + 5.0)

# Where the walk west ends: the wall is WALL_DISTANCE west of the start and 2 * WALL_HALF_EXTENTS[0] wide and the player
# a box 40 wide, so it comes to a halt about 20 short of the east face of the wall, WALL_DISTANCE - 20 west of the start
WEST_FROM = 11.0
WALL_FACE = plugin.WALL_DISTANCE - plugin.WALL_HALF_EXTENTS[0]
WALL_STOP = (-WALL_FACE + 10.0, -WALL_FACE + 30.0)

# What the rays at the drawn body of the person must find, each from outside to the middle of the body at the height of the
# ray (the person stands at NPC_DISTANCE east and NPC_DEPTH north of where the player starts): the name of the ray, where
# it starts and ends relative to the person, and the coordinate of the face it hits. The parts are boxes (PERSON_PARTS),
# so the faces are exact. The suit covers the upper body and the right hand and is narrower than the upper body of the race
# and than the right hand is far out, so a ray from the east at the height of the hand hits the suit only when neither of
# them is drawn.
def person_face(name, axis, side):
    (half, centre) = plugin.PERSON_PARTS[name]
    return centre[axis] + side * half[axis]


BODY_RAYS = {
    "east": ((100, 0, 50), (-100, 0, 50), 0, person_face("suit", 0, +1)),
    "west": ((-100, 0, 50), (100, 0, 50), 0, person_face("lefthand", 0, -1)),
    "head": ((100, 0, 118), (-100, 0, 118), 0, person_face("head", 0, +1)),
    "top": ((0, 0, 300), (0, 0, 0), 2, person_face("hair", 2, +1)),
}

# Height of the camera above the feet of the player, in game units: the head node of the placeholder skeleton is at 124.
EYE_HEIGHT = (100.0, 140.0)

BODY_RAY_LUA = "".join("                look('%s', { %s }, { %s })\n" % (name, ", ".join(map(str, start)), ", ".join(map(str, end)))
                       for name, (start, end, _, _) in BODY_RAYS.items())

# Waits for the player to settle, then walks north (the player faces north at the start) into the pillar, from the
# eighth second strafes east into the person that stands there, and logs once a second. At 7.5 s, when it has stopped at
# the pillar, a ray goes east at the height of the middle of its body and looks for an actor.
WALK_SCRIPT = """\
local self = require('openfallout.self')
local nearby = require('openfallout.nearby')
local util = require('openfallout.util')
local core = require('openfallout.core')
local I = require('openfallout.interfaces')
local camera = require('openfallout.camera')
local async = require('openfallout.async')

local npcPos = nil
local bodyRaysDone = false
local elapsed = 0
local nextLog = 0
local rayDone = false
local personRayDone = false
local wallRayDone = false
local function log(...) print('OFTEST', ...) end
local function fmt(v) return string.format('%%.1f,%%.1f,%%.1f', v.x, v.y, v.z) end

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if elapsed >= 3 then
                I.Controls.overrideMovementControls(true)
                self.controls.movement = elapsed < 8 and 1 or 0
                self.controls.sideMovement = elapsed < 8 and 0 or (elapsed < %d and 1 or -1)
                self.controls.run = true
                self.controls.yawChange = 0
                -- looks up for a second, the pitch stops at straight up
                self.controls.pitchChange = elapsed < 4 and -2 * dt or 0
            end
            if elapsed >= nextLog then
                nextLog = nextLog + 1
                log(string.format('t=%%.1f exterior=%%s name=%%s pos=%%s cam=%%s', elapsed,
                    tostring(self.cell.isExterior), tostring(self.cell.name), fmt(self.position),
                    fmt(camera.getPosition())))
            end
            if elapsed >= 4 and not rayDone then
                rayDone = true
                local from = self.position + util.vector3(0, 0, 200)
                local down = nearby.castRay(from, from - util.vector3(0, 0, 1000),
                    { collisionType = nearby.COLLISION_TYPE.World + nearby.COLLISION_TYPE.HeightMap })
                log('ray down hit=', tostring(down.hit), down.hit and fmt(down.hitPos) or '')
                local water = nearby.castRay(util.vector3(%.1f, %.1f, 500), util.vector3(%.1f, %.1f, -500),
                    { collisionType = nearby.COLLISION_TYPE.Water })
                log('ray water hit=', tostring(water.hit), water.hit and fmt(water.hitPos) or '')
                -- just under the surface and along it, across the edge of the cell: only the surface is there to hit
                local side = nearby.castRay(util.vector3(%.1f, %.1f, %.1f), util.vector3(%.1f, %.1f, %.1f),
                    { collisionType = nearby.COLLISION_TYPE.Water })
                log('ray water side hit=', tostring(side.hit), side.hit and fmt(side.hitPos) or '')
            end
            if elapsed >= 7.5 and not personRayDone then
                personRayDone = true
                local from = self.position + util.vector3(0, 0, 64)
                local person = nearby.castRay(from, from + util.vector3(600, 0, 0),
                    { collisionType = nearby.COLLISION_TYPE.Actor, ignore = self.object })
                log('ray person hit=', tostring(person.hit), person.hit and fmt(person.hitPos) or '',
                    person.hitObject and tostring(person.hitObject.recordId) or '')
                npcPos = person.hitObject and person.hitObject.position
            end
            -- rays at what is drawn of the person, from the outside to the middle of the body
            if elapsed >= 9 and not bodyRaysDone and npcPos then
                bodyRaysDone = true
                local function look(name, from, to)
                    nearby.asyncCastRenderingRay(async:callback(function(res)
                        log('render ray ' .. name .. ' hit=', tostring(res.hit), res.hit and fmt(res.hitPos) or '')
                    end), npcPos + util.vector3(table.unpack(from)), npcPos + util.vector3(table.unpack(to)))
                end
%s            end
            -- at the wall that is not drawn: a ray at the world finds it, a ray at what is drawn goes through
            if elapsed >= %d and not wallRayDone then
                wallRayDone = true
                local from = self.position + util.vector3(0, 0, 64)
                local wall = nearby.castRay(from, from - util.vector3(300, 0, 0),
                    { collisionType = nearby.COLLISION_TYPE.World, ignore = self.object })
                log('ray wall hit=', tostring(wall.hit), wall.hit and fmt(wall.hitPos) or '')
                nearby.asyncCastRenderingRay(async:callback(function(res)
                    log('render ray wall hit=', tostring(res.hit), res.hit and fmt(res.hitPos) or '')
                end), from, from - util.vector3(300, 0, 0))
            end
            if elapsed >= %d then core.quit() end
        end,
    },
}
""" % (int(WEST_FROM), *WATER_CENTRE, *WATER_CENTRE, *WATER_SIDE_RAY, BODY_RAY_LUA, int(WEST_FROM) + 4, WALK_SECONDS)

NUMBER = r"(-?[\d.]+)"
VECTOR = ",".join([NUMBER] * 3)
SAMPLE = re.compile(r"OFTEST\tt=([\d.]+) exterior=(\w+) name=(.*) pos=" + VECTOR + " cam=" + VECTOR)
RAY = re.compile(r"OFTEST\tray down hit=\t(\w+)\t(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)")
RENDER_RAY = re.compile(r"OFTEST\trender ray (\w+) hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?")
WATER_SIDE_RAY_LINE = re.compile(r"OFTEST\tray water side hit=\t(\w+)")
PERSON_RAY = re.compile(r"OFTEST\tray person hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?\t(.*)")
WALL_RAY = re.compile(r"OFTEST\tray wall hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?")
WALL_RENDER_RAY = re.compile(r"OFTEST\trender ray wall hit=\t(\w+)")
WATER_RAY = re.compile(r"OFTEST\tray water hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?")


def start_xvfb():
    if os.environ.get("DISPLAY"):
        return None
    xvfb = shutil.which("Xvfb")
    if xvfb is None:
        sys.exit("DISPLAY is not set and Xvfb is not installed")
    display = ":%d" % (90 + os.getpid() % 100)
    process = subprocess.Popen([xvfb, display, "-screen", "0", "1280x720x24", "+extension", "GLX"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.environ["DISPLAY"] = display
    time.sleep(2)
    return process


def wait_for_pixels(process, log, seconds, pixels):
    """The colours of the pixels of the screen, as {what it shows: (red, green, blue)}, each read once the walk script
    has logged its first position after the time of the pixel. A colour is None when it cannot be read (ImageMagick's
    `import` is not installed, or the engine quit before)."""
    importer = shutil.which("import")
    colours = {pixel[0]: None for pixel in pixels}
    pending = list(pixels)
    deadline = time.monotonic() + seconds
    while importer is not None and pending and process.poll() is None and time.monotonic() < deadline:
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        for pixel in list(pending):
            name, when, (x, y) = pixel[:3]
            if re.search(rf"OFTEST\tt={int(when)}\.\d ", text):
                shot = subprocess.run([importer, "-window", "root", "-crop", "1x1+%d+%d" % (x, y), "+repage", "txt:-"],
                                      capture_output=True, text=True)
                found = re.search(r"srgba?\((\d+),(\d+),(\d+)", shot.stdout)
                grey = re.search(r"gray\((\d+)\)", shot.stdout)
                colours[name] = (tuple(int(found.group(i)) for i in (1, 2, 3)) if found
                                 else (int(grey.group(1)),) * 3 if grey else None)
                pending.remove(pixel)
        time.sleep(0.1)
    return colours


def run_engine(program, resources, data, home, runtime, seconds, start, pixels):
    env = dict(os.environ, HOME=str(home), XDG_RUNTIME_DIR=str(runtime), LIBGL_ALWAYS_SOFTWARE="1")
    for name in ("XDG_CONFIG_HOME", "XDG_DATA_HOME"):
        env.pop(name, None)
    command = [str(program), "--resources", str(resources), "--data", str(data), "--content", "OFTest.esm",
               "--content", "walktest.omwscripts", "--skip-menu", "--start", start, "--no-grab"]
    # The engine shows a dialog instead of logging a fatal error when stdin is not a terminal, so give it one.
    master, slave = pty.openpty()
    process = subprocess.Popen(command, stdin=slave, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    os.close(slave)
    colours = {}
    try:
        colours = wait_for_pixels(process, home / ".config" / "openfallout" / "openfallout.log", seconds, pixels)
        process.wait(timeout=seconds)
    except subprocess.TimeoutExpired:
        print(f"the engine was still running after {seconds} seconds, stopping it")
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    os.close(master)
    return process.returncode, colours


def check(text, scenario, colours):
    """The problems the log shows, as a list of sentences; empty when the run did what it should."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Can't start|Failed to start|Lua error|Fatal error|Failed to load Lua", line, re.I):
            problems.append(line)
    # a person's body is no obstacle for the navigator (which can not read a cylinder), and its record has its data
    for line in text.splitlines():
        if re.search(r"Unsupported shape type|Traits are not found|Base data is not found|NPC traits not found", line):
            problems.append(line)
    if "using placeholder records" not in text:
        problems.append("the engine did not make placeholder records, so the content had a player record")
    if f"Loading cell {scenario.loaded}" not in text:
        problems.append(f"the engine did not load the cell {scenario.loaded}")
    if scenario.exterior:
        # The worldspace has one cell record, so the engine makes the cells around it. Each must be loaded once, under
        # its own coordinates: two made cells that shared an id would load one of them twice and the other never.
        # (the cell with water and the one flagged for it have records of their own, and the names of those in the log)
        loaded = re.findall(rf"Loading cell {plugin.WORLD_NAME}\w* \((-?\d+), (-?\d+)\)", text)
        for dx, dy in NEIGHBOURS:
            if loaded.count((str(dx), str(dy))) != 1:
                problems.append(f"the cell {plugin.WORLD_NAME} ({dx}, {dy}) next to the start was loaded "
                                f"{loaded.count((str(dx), str(dy)))} times, expected once")
    if scenario.lit and LIGHTING_LINE not in text:
        lines = [line.split("]", 1)[-1].strip() for line in text.splitlines() if "Cell lighting" in line]
        problems.append(f"the log has no line '{LIGHTING_LINE}', the lighting of the cell is: {lines or 'not logged'}")
    weather_lines = [line.split("]", 1)[-1].strip() for line in text.splitlines() if "] Weather: " in line]
    if scenario.exterior and WEATHER_LINE not in weather_lines:
        problems.append(f"the log has no line '{WEATHER_LINE}', the weather of the cell is: "
                        f"{weather_lines or 'not logged'}")
    # the texture of the ground is where its texture set says, not where the icon of its record says
    for line in text.splitlines():
        if re.search(r"Landscape texture .* is not in the data files", line):
            problems.append(line)
    if any(plugin.CONDITIONAL_WEATHER_NAME in line for line in weather_lines):
        problems.append("the engine chose the weather that needs a global that is 0")
    if not scenario.exterior and weather_lines:
        problems.append(f"the engine chose a weather in an interior cell: {weather_lines}")
    water_lines = [line.split("]", 1)[-1].strip() for line in text.splitlines() if "] Water of cell " in line]
    if scenario.exterior and water_lines != [WATER_LINE]:
        problems.append(f"the log has the lines {water_lines or 'none'} about water, expected only '{WATER_LINE}'")
    if not scenario.exterior and water_lines:
        problems.append(f"the engine made water in an interior cell: {water_lines}")
    for name, _, position, want, *mode in scenario.pixels:
        have = colours.get(name)
        print(f"pixel {position} shows the {name}: {have}, expected {mode[0] + ' ' if mode else ''}{want}")
        if have is None:
            print(f"no screenshot, the colour of the {name} on the screen is not checked (needs ImageMagick's import)")
        elif mode == ["tinted"]:
            strongest = have.index(max(have))
            if (strongest != want.index(max(want)) or have[strongest] < TINT_MINIMUM
                    or any(2 * channel > have[strongest] for i, channel in enumerate(have) if i != strongest)):
                problems.append(f"the pixel at {position} that should show the {name} is {have}, which should have "
                                f"the hue of {want} (its strongest channel the same and at least {TINT_MINIMUM}, "
                                f"the others less than half of it)")
        elif mode:
            if any(h > w - WATER_DIFFERENCE for h, w in zip(have, want)):
                problems.append(f"the pixel at {position} that should show the {name} is {have}, which should be at "
                                f"least {WATER_DIFFERENCE} below {want} in each channel")
        elif any(abs(h - w) > PIXEL_TOLERANCE for h, w in zip(have, want)):
            problems.append(f"the pixel at {position} that should show the {name} is {have}, the {name} colour of the "
                            f"cell is {want}")
    missing = re.findall(r"Failed to (?:load|open) (?:image|'[^']*sky[^']*')[^\n]*(?:tx_sun|tx_moon|tx_masser|tx_secunda"
                         r"|tx_sky|sky_[a-z_0-9]*\.nif)[^\n]*", text)
    if missing:
        problems.append(f"the log reports files of the sky of Morrowind as missing: {missing[:3]}")
    samples = [(float(m.group(1)), m.group(2), m.group(3), tuple(float(m.group(i)) for i in (4, 5, 6)),
                tuple(float(m.group(i)) for i in (7, 8, 9))) for m in map(SAMPLE.search, text.splitlines()) if m]
    if len(samples) < WALK_SECONDS:
        problems.append(f"the walk script logged {len(samples)} positions, expected at least {WALK_SECONDS}")
        return problems
    for _, exterior, _, _, _ in samples:
        if exterior != str(scenario.exterior).lower():
            problems.append(f"the player is {'not ' if scenario.exterior else ''}in an exterior cell")
            break
    for time_, _, _, (x, y, z), _ in samples:
        if not 0.0 <= z <= 2.0:
            problems.append(f"at {time_:.0f} s the player is {z:.1f} units high, not standing on the floor at 0")
            break
    # the walk north ends at the pillar, and from the eighth second the player strafes east into the person
    x, y, z = [position for time_, _, _, position, _ in samples if time_ < STRAFE_FROM][-1]
    origin_x, origin_y = scenario.origin
    if not origin_y + STOP_RANGE[0] <= y <= origin_y + STOP_RANGE[1]:
        problems.append(f"the player ended at y={y:.1f}, expected {origin_y + STOP_RANGE[0]:.0f} to "
                        f"{origin_y + STOP_RANGE[1]:.0f} (stopped by the pillar)")
    if abs(x - origin_x) > 5.0:
        problems.append(f"the player drifted sideways to x={x:.1f} before the strafe")
    x, y, z = [position for time_, _, _, position, _ in samples if time_ <= WEST_FROM][-1]
    if not origin_x + PERSON_STOP[0] <= x <= origin_x + PERSON_STOP[1]:
        problems.append(f"the player strafing east ended at x={x:.1f}, expected {origin_x + PERSON_STOP[0]:.0f} to "
                        f"{origin_x + PERSON_STOP[1]:.0f} (stopped by the person at {origin_x + plugin.NPC_DISTANCE:.0f})")
    # and then west, across the start, into the wall that is not drawn
    x, y, z = samples[-1][3]
    if not origin_x + WALL_STOP[0] <= x <= origin_x + WALL_STOP[1]:
        problems.append(f"the player strafing west ended at x={x:.1f}, expected {origin_x + WALL_STOP[0]:.0f} to "
                        f"{origin_x + WALL_STOP[1]:.0f} (stopped by the wall, which has its Havok collision, at "
                        f"{origin_x - plugin.WALL_DISTANCE:.0f})")
    # The first person camera follows a node of the skeleton, and with none it stays at the world origin: it has to be
    # above the player, at about the eye height of a human, from the second second on.
    for time_, _, _, (px, py, pz), (cx, cy, cz) in samples:
        above = EYE_HEIGHT[0] <= cz - pz <= EYE_HEIGHT[1]
        if time_ >= 1.0 and not (abs(cx - px) < 40.0 and abs(cy - py) < 40.0 and above):
            problems.append(f"at {time_:.0f} s the camera is at {cx:.0f},{cy:.0f},{cz:.0f} and the player at "
                            f"{px:.0f},{py:.0f},{pz:.0f}, not at eye height above the player")
            break
    rays = [RAY.search(line) for line in text.splitlines()]
    rays = [m for m in rays if m]
    if not rays or rays[0].group(1) != "true" or abs(float(rays[0].group(4))) > 1.0:
        problems.append("the ray cast down from the player did not hit the ground at height 0")
    # The water of a cell of Fallout is a body to collide with (to stand on with water walking, to be hit by a bullet)
    water_rays = [m for m in map(WATER_RAY.search, text.splitlines()) if m]
    if not water_rays:
        problems.append("the script did not log the ray cast at the water")
    elif scenario.exterior and (water_rays[0].group(1) != "true"
                                or abs(float(water_rays[0].group(4)) - plugin.WATER_HEIGHT) > 1.0):
        problems.append(f"the ray cast down at the water of the cell {plugin.WATER_CELL} did not hit it at height "
                        f"{plugin.WATER_HEIGHT:g}: {water_rays[0].group(0)}")
    elif not scenario.exterior and water_rays[0].group(1) != "false":
        problems.append(f"the ray cast at the water hit something in an interior cell: {water_rays[0].group(0)}")
    # The person is a body to collide with, as an actor does: the ray at the actors finds it, at the near edge
    person_rays = [m for m in map(PERSON_RAY.search, text.splitlines()) if m]
    if not person_rays:
        problems.append("the script did not log the ray cast at the person")
    else:
        hit, hit_x, hit_object = person_rays[0].group(1), person_rays[0].group(2), person_rays[0].group(5)
        want_x = scenario.origin[0] + plugin.NPC_DISTANCE - PERSON_RADIUS
        # (a ray at a cylinder ends a few units off, the tests of Bullet for convex shapes are not exact)
        form_id = re.search(r"0x([0-9a-f]+)", hit_object, re.I)
        if (hit != "true" or abs(float(hit_x) - want_x) > 5.0 or not form_id
                or int(form_id.group(1), 16) & 0xFFFFFF != plugin.NPC_ID):
            problems.append(f"the ray cast east at the actors did not find the person (form {plugin.NPC_ID:#x}) at "
                            f"x={want_x:.0f}: {person_rays[0].group(0)}")
    # What is drawn of the person: the suit where it covers the body, the hand that it does not cover, the head, the hair.
    # The ray goes along an axis, so the coordinate of that axis is the one of the face that it hits
    person = (scenario.origin[0] + plugin.NPC_DISTANCE, scenario.origin[1] + plugin.NPC_DEPTH, 0.0)
    found = {m.group(1): m for m in map(RENDER_RAY.search, text.splitlines()) if m}
    for name, (_, _, axis, want) in BODY_RAYS.items():
        ray = found.get(name)
        if ray is None:
            problems.append(f"the script did not log the ray at what is drawn of the person ('{name}')")
        elif ray.group(2) != "true" or abs(float(ray.group(3 + axis)) - (person[axis] + want)) > 1.5:
            problems.append(f"the ray '{name}' at what is drawn of the person should hit a face at {person[axis] + want:.1f}"
                            f" on its axis: {ray.group(0)}")
    for line in text.splitlines():
        if re.search(r" [EW]\] .*(?:person_|Hair not found|Head part not found)", line):
            problems.append(line)
    # The wall has nothing to draw, so a ray at what is drawn goes through it, and its Havok box is hit by a ray at the
    # world: the face of its box that the player faces, WALL_FACE west of the start
    wall_rays = [m for m in map(WALL_RAY.search, text.splitlines()) if m]
    wall_render_rays = [m for m in map(WALL_RENDER_RAY.search, text.splitlines()) if m]
    if not wall_rays or not wall_render_rays:
        problems.append("the script did not log the rays cast at the wall that is not drawn")
    else:
        want_x = scenario.origin[0] - WALL_FACE
        if wall_rays[0].group(1) != "true" or abs(float(wall_rays[0].group(2)) - want_x) > 2.0:
            problems.append(f"the ray cast west at the world did not hit the Havok box of the wall at x={want_x:.0f}: "
                            f"{wall_rays[0].group(0)}")
        if wall_render_rays[0].group(1) != "false":
            problems.append(f"the ray cast west at what is drawn hit something where only the wall is: "
                            f"{wall_render_rays[0].group(0)}")
    side_rays = [m for m in map(WATER_SIDE_RAY_LINE.search, text.splitlines()) if m]
    if not side_rays or side_rays[0].group(1) != "false":
        problems.append("the ray cast along just under the water and across the edge of its cell hit something, "
                        "the water should have no sides")
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, type=Path, help="build directory with the openfallout program")
    parser.add_argument("--seconds", type=float, default=90, help="how long to give the engine before stopping it")
    parser.add_argument("--keep", action="store_true", help="keep the temporary directory and print its path")
    args = parser.parse_args()

    build = args.build.resolve()
    program = build / "openfallout"
    if not program.exists():
        sys.exit(f"{program} does not exist")
    problems = []

    shipped = ROOT / "files" / "data" / "meshes" / "placeholder_skeleton.nif"
    if not shipped.exists() or shipped.read_bytes() != placeholder_skeleton.skeleton():
        problems.append(f"{shipped} is not what placeholder_skeleton.py writes, run it again")

    work = Path(tempfile.mkdtemp(prefix="openfallout-fallout-start-"))
    data = work / "data"
    plugin.write(data)
    (data / "scripts").mkdir()
    (data / "scripts" / "walktest.lua").write_text(WALK_SCRIPT, encoding="ascii")
    (data / "walktest.omwscripts").write_text("PLAYER: scripts/walktest.lua\n", encoding="ascii")

    xvfb = start_xvfb()
    for scenario in SCENARIOS:
        home = work / scenario.name / "home"
        home.mkdir(parents=True)
        runtime = work / scenario.name / "runtime"
        runtime.mkdir(mode=0o700)
        print(f"== {scenario.name}: --start {scenario.start}")
        status, colours = run_engine(program, build / "resources", data, home, runtime, args.seconds, scenario.start,
                                     scenario.pixels)

        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check(text, scenario, colours)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print(f"PROBLEM ({scenario.name}):", problem)
        problems += found
    if xvfb is not None:
        xvfb.terminate()

    if args.keep:
        print(f"kept {work}")
    else:
        shutil.rmtree(work, ignore_errors=True)
    print("FAIL" if problems else "PASS")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
