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
the suit does not cover, and none the parts of its race that the suit takes the place of; its race has a small box
skinned to the left foot and the folder of the skeleton has an animation file that moves that foot, which a ray at the
box finds where the animation put it: the file moves the foot with the control points of a B-spline, as most bones of
the files of the games are driven, or with two keys under `--key-animation`; `--no-animation` leaves the file out, which
must leave the foot where the skeleton has it)
and, from there, three creatures in view east of the person (one with a skeleton, body models and an idle animation of its own, one that takes its model from a levelled
list of creatures that is its template, and a levelled list of creatures placed as it is, which the cell must turn into the creature that
the list gives): a ray at what is drawn finds the body of each, and the box skinned to the foot of each where the
animation put it, a ray at the actors finds the solid body that the box of the record makes, and the one that the list
gives has the record of its entry; and, strafing west from
there, at a wall that is not drawn (a mesh with only Havok collision: a ray at what is drawn goes through it, the player
and a ray at the world stop at it), nothing logs an error from Lua, and the engine quits by itself. When ImageMagick's `import` is installed,
pixels of the screen are checked too: where nothing is drawn it has the colour of the fog of the cell or of the weather,
and in the exterior, where the player looks up after three seconds, the sky overhead has the sky colour of the weather
with the clouds of the weather over it, and a band of water in a cell north of the start shows where the horizon would
be (it is green, as the kind of water of the worldspace says, so greener than the fog and not tinted by the
colour of a texture that the engine can not find). The water of the cell that has a
height of its own is logged once, with its kind of water, and the cell that holds the largest float as its height has
none. The game has no files of the sky of Morrowind here, and the log must not mention a texture or a mesh of the sky as
missing. Three more characters stand south and west of the start, where the player does not walk, and a Lua player
script (PACKAGE_SCRIPT) logs where each is once a second: one that has two AI packages (the first is for the night,
which it must ignore at the time of day the game starts at, the second sends it to a marker 300 units north of it) must
walk there (to the edge of the radius that the package names), turn to the way the marker faces and stay; one that has no package must stay where it was put; one that has
a package that sends it about the place where it stands must stay within the radius of the package and move. Two
more have packages that depend on scripts: one has a package whose condition never holds and one whose condition holds
from the stage of quest C that a Lua global script sets (GATE_SCRIPT), so it stays until then and goes to the marker of
the second; the other has no package until the script of the scripted object gives it one, and goes to its marker. A
character with no animation to walk with (`--no-animation`) is not moved at all.

    scripts/openfallout/fallout_start_smoke_test.py --build build

It needs no game data and no data of Bethesda. The engine's configuration and logs go to a temporary home directory.
The exit status is 0 when every check holds. It also checks that files/data/meshes/placeholder_skeleton.nif is what
placeholder_skeleton.py writes.
"""
import argparse
import ctypes
import math
import os
import pty
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

import placeholder_skeleton
import synthetic_fallout_plugin as plugin

ROOT = Path(__file__).resolve().parents[2]
AUTOPLAY = ROOT / "scripts" / "openfallout" / "autoplay"

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
    fifth item changes what is checked. "greener": the green channel of the pixel must be at least WATER_GREEN more than
    the red one and than the blue one (the water is green and opaque, and the fog that the horizon has is not green;
    the colour that the engine gives a texture that it can not find is magenta, which has no green at all, so water
    that took such a texture would not pass). "tinted": the pixel must have the hue of the colour given:
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
# transparent, over the terrain). The kind of water of the worldspace is green and opaque: how much greener than red
# and blue the pixel must be for that water to be there.
WATER_PIXEL = (900, 380)
WATER_GREEN = 25
# A pixel of the ground a little before the player, on the left at the bottom of the window (the messages that the
# scripts show are in the middle of the bottom, and dark behind their text). The texture of the ground is red; the
# default texture of the game is not in the data files of the test.
GROUND_PIXEL = (240, 600)
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
              ("water", HORIZON_TIME, WATER_PIXEL, plugin.WATER_SHALLOW, "greener"),
              ("ground", HORIZON_TIME, GROUND_PIXEL, plugin.GROUND_COLOUR, "tinted"),
              ("sky and clouds", ZENITH_TIME, SKY_PIXEL,
               clouded_sky(plugin.WEATHER_SKY[1], plugin.WEATHER_FOG[1], plugin.CLOUD_ALPHA))]),
]

WALK_SECONDS = 24
# The scripted object moves the player to a marker in the other kind of cell after the walk, and the run goes on for as
# long as it takes the player to get there and be looked at
MOVE_SECONDS = 6
PLAYER_MOVES = "OFTEST\tplayer moves now"
MOVE_FROM = WALK_SECONDS + 1
MOVE_LOOK = WALK_SECONDS + 3.5
# The player stands still from the fifteenth second, where the walk west ends, and turns to face east, to the person,
# for half a second, so that the person is seen: the body of a model that is skinned is only brought up to date when it
# is seen, and a ray at what is drawn finds it where it was when it was last seen. A second later a ray looks at its foot.
TURN_FROM = 15
FOOT_RAY_FROM = TURN_FROM + 1.5

# Then the person is moved east by a script of the world (PERSON_SCRIPT), which starts when the player script sends it
# the word at PERSON_MOVE_FROM: at the speed of walking for WALK_FOR seconds and then, faster, at the speed of running
# for RUN_FOR seconds, and stands still again. A character has no mechanics of its own yet, so the engine takes its speed
# from how fast whatever moves it changes its place. A ray at the foot looks WALK_RAY_AFTER seconds after the person
# starts to walk, one RUN_RAY_AFTER seconds after it starts, and one STAND_RAY_AFTER seconds after.
PERSON_MOVE_FROM = 18.5
WALK_FOR = 1.5
RUN_FOR = 0.9
WALK_RAY_AFTER = 1.0
RUN_RAY_AFTER = WALK_FOR + 0.7
STAND_RAY_AFTER = WALK_FOR + RUN_FOR + 1.5

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


# The small box of the race is skinned to the left foot of the skeleton, which hangs from the calf under the thigh
# (BONES in placeholder_skeleton.py). With no animation it is where the skeleton has the foot; with the idle animation,
# which moves the foot FOOT_MOVE to the side of the calf, the ray from the west finds the box that far from it. The ray
# runs at the height of the foot.
FOOT_BONES = dict((name, offset) for name, _, offset in placeholder_skeleton.BONES)
FOOT_CHAIN = [FOOT_BONES[name] for name in ("Bip01 Pelvis", "Bip01 L Thigh", "Bip01 L Calf", "Bip01 L Foot")]
FOOT_HEIGHT = sum(offset[2] for offset in FOOT_CHAIN)
FOOT_REST_FACE = sum(offset[0] for offset in FOOT_CHAIN) - plugin.FOOT_MARKER[0]
FOOT_PLAYED_FACE = FOOT_REST_FACE + plugin.FOOT_MOVE


BODY_RAYS = {
    "east": ((100, 0, 50), (-100, 0, 50), 0, person_face("suit", 0, +1)),
    "west": ((-100, 0, 50), (100, 0, 50), 0, person_face("lefthand", 0, -1)),
    "head": ((100, 0, 118), (-100, 0, 118), 0, person_face("head", 0, +1) + plugin.FACE_SHIFT),
    "top": ((0, 0, 300), (0, 0, 0), 2, person_face("hair", 2, +1)),
}
FOOT_RAY = ((-300, 0, FOOT_HEIGHT), (0, 0, FOOT_HEIGHT), 0, FOOT_PLAYED_FACE)
# Where the box is, from the person, as it walks, runs and stands again. The person moves while the ray is cast, so the
# coordinate is not exact: the poses are more than twice this apart.
MOVING_FOOT_FACES = {
    "footwalk": FOOT_REST_FACE + plugin.WALK_MOVE,
    "footrun": FOOT_REST_FACE + plugin.RUN_MOVE,
    "footstand": FOOT_PLAYED_FACE,
}
MOVING_FOOT_TOLERANCE = 25.0

# The creatures stand at CREATURE_POSITIONS from where the player starts. A ray from above finds the top of the body model
# (a box, the first of the NIFZ list), a ray from the west at the height of the foot the box that is skinned to the foot
# (moved by the animation like the one of the person), and a ray at the actors the solid body made of the box of the
# record, a cylinder as wide as the smaller side of the box: from the east at half the height of the box it is hit
# CREATURE_RADIUS short of the middle. The record of what it hits: the creature itself, the one that takes its model from
# a template, and for the list the creature that the list gives.
CREATURE_TOP = plugin.CREATURE_BODY_BOX[1][2] + plugin.CREATURE_BODY_BOX[0][2]
_BOUNDS = plugin.CREATURE_BOUNDS
CREATURE_RADIUS = min(_BOUNDS[3] - _BOUNDS[0], _BOUNDS[4] - _BOUNDS[1]) / 2.0
CREATURE_BODY_HEIGHT = (_BOUNDS[2] + _BOUNDS[5]) / 2.0
CREATURE_RECORDS = {"beast": plugin.CREATURE_ID, "user": plugin.CREATURE_USER_ID, "list": plugin.CREATURE_ID}
CREATURE_ROW = ", ".join("{ '%s', %g, %g }" % (name, x, y) for name, (x, y) in plugin.CREATURE_POSITIONS.items())
CREATURE_RAYS_FROM = 9.0

# Height of the camera above the feet of the player, in game units: the head node of the placeholder skeleton is at 124.
EYE_HEIGHT = (100.0, 140.0)

def ray_lua(name, start, end):
    return "look('%s', { %s }, { %s })" % (name, ", ".join(map(str, start)), ", ".join(map(str, end)))


BODY_RAY_LUA = "".join("                %s\n" % ray_lua(name, start, end) for name, (start, end, _, _) in BODY_RAYS.items())
FOOT_RAY_LUA = ray_lua("foot", FOOT_RAY[0], FOOT_RAY[1])

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

local startPos = nil
local creatureRaysDone = false
local creatureFootRaysDone = false
local creatures = { %s }
local npcPos = nil
local npcObject = nil
local personMoving = false
local footRaysDone = {}
local bodyRaysDone = false
local elapsed = 0
local nextLog = 0
local rayDone = false
local personRayDone = false
local wallRayDone = false
local footRayDone = false
local function log(...) print('OFTEST', ...) end
local function fmt(v) return string.format('%%.1f,%%.1f,%%.1f', v.x, v.y, v.z) end
-- a ray at what is drawn of the person, from and to points relative to its position
local function look(name, from, to)
    nearby.asyncCastRenderingRay(async:callback(function(res)
        log('render ray ' .. name .. ' hit=', tostring(res.hit), res.hit and fmt(res.hitPos) or '')
    end), npcPos + util.vector3(table.unpack(from)), npcPos + util.vector3(table.unpack(to)))
end

-- a ray at what is drawn, from and to points relative to where the player started
local function lookAt(name, from, to)
    nearby.asyncCastRenderingRay(async:callback(function(res)
        log('render ray ' .. name .. ' hit=', tostring(res.hit), res.hit and fmt(res.hitPos) or '')
    end), startPos + util.vector3(table.unpack(from)), startPos + util.vector3(table.unpack(to)))
end

-- a ray at the foot of the person where it is now (it is moved), which logs where it is when the ray has found its foot
local function lookFoot(name)
    local pos = npcObject.position
    nearby.asyncCastRenderingRay(async:callback(function(res)
        log('render ray ' .. name .. ' hit=', tostring(res.hit), res.hit and fmt(res.hitPos) or '',
            'person', fmt(npcObject.position))
    end), pos + util.vector3(%s), pos + util.vector3(%s))
end

-- Frames the screen of the Pip-Boy or the level up has been on
local screenFrames = 0

return {
    engineHandlers = {
        -- A script of the test rewards experience, and the screen of the level up stops the game until the player is done
        -- with it: the test closes it after some frames (frames go on while the game is stopped)
        onFrame = function()
            if I.UI.getMode() ~= 'Interface' then
                screenFrames = 0
            else
                screenFrames = screenFrames + 1
                if screenFrames > 10 then
                    I.UI.removeMode('Interface')
                    screenFrames = 0
                end
            end
        end,
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if startPos == nil then startPos = self.position end
            if elapsed >= 3 then
                I.Controls.overrideMovementControls(true)
                self.controls.movement = elapsed < 8 and 1 or 0
                self.controls.sideMovement = elapsed < 8 and 0 or (elapsed < %d and 1 or (elapsed < %d and -1 or 0))
                self.controls.run = true
                self.controls.yawChange = (elapsed >= %d and elapsed < %d + 0.5) and (math.pi / 2) / 0.5 * dt or 0
                -- looks up for a second, the pitch stops at straight up, and looks level again when it turns
                self.controls.pitchChange = elapsed < 4 and -2 * dt
                    or ((elapsed >= %d and elapsed < %d + 0.5) and (math.pi / 2) / 0.5 * dt or 0)
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
                npcObject = person.hitObject
            end
            -- rays at what is drawn of the person, from the outside to the middle of the body
            if elapsed >= 9 and not bodyRaysDone and npcPos then
                bodyRaysDone = true
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
            -- the foot of the person, which the animation has moved (see TURN_FROM)
            if elapsed >= %.1f and not footRayDone and npcPos then
                footRayDone = true
                log('view', string.format('yaw=%%.2f pitch=%%.2f', camera.getYaw(), camera.getPitch()))
                %s
            end
            -- the person is moved from here, and its foot looked at while it walks, runs and stands again
            if elapsed >= %.1f and npcObject and not personMoving then
                personMoving = true
                core.sendGlobalEvent('OFTestMovePerson', npcObject)
            end
            for _, ray in ipairs({ { 'footwalk', %.1f }, { 'footrun', %.1f }, { 'footstand', %.1f } }) do
                if personMoving and elapsed >= ray[2] and not footRaysDone[ray[1]] then
                    footRaysDone[ray[1]] = true
                    lookFoot(ray[1])
                end
            end
            -- the creatures, which are far from where the player walks: rays from above and at the actors, and, when the
            -- player faces them, rays at the box that is skinned to the foot, which the animation has moved
            if elapsed >= %.1f and not creatureRaysDone then
                creatureRaysDone = true
                for _, c in ipairs(creatures) do
                    lookAt('creaturetop_' .. c[1], { c[2], c[3], 300 }, { c[2], c[3], 0 })
                    local from = startPos + util.vector3(c[2] + 300, c[3], %g)
                    local hit = nearby.castRay(from, from - util.vector3(600, 0, 0),
                        { collisionType = nearby.COLLISION_TYPE.Actor, ignore = self.object })
                    log('ray creature ' .. c[1] .. ' hit=', tostring(hit.hit), hit.hit and fmt(hit.hitPos) or '',
                        hit.hitObject and tostring(hit.hitObject.recordId) or '')
                end
            end
            if elapsed >= %.1f and not creatureFootRaysDone then
                creatureFootRaysDone = true
                for _, c in ipairs(creatures) do
                    lookAt('creaturefoot_' .. c[1], { c[2] - 300, c[3], %g }, { c[2], c[3], %g })
                end
            end
            if elapsed >= %d then core.quit() end
        end,
    },
}
""" % (CREATURE_ROW, ", ".join(map(str, FOOT_RAY[0])), ", ".join(map(str, FOOT_RAY[1])), int(WEST_FROM), TURN_FROM, TURN_FROM, TURN_FROM,
       TURN_FROM, TURN_FROM, *WATER_CENTRE, *WATER_CENTRE, *WATER_SIDE_RAY, BODY_RAY_LUA, int(WEST_FROM) + 4,
       FOOT_RAY_FROM, FOOT_RAY_LUA, PERSON_MOVE_FROM, PERSON_MOVE_FROM + WALK_RAY_AFTER,
       PERSON_MOVE_FROM + RUN_RAY_AFTER, PERSON_MOVE_FROM + STAND_RAY_AFTER, CREATURE_RAYS_FROM,
       CREATURE_BODY_HEIGHT, FOOT_RAY_FROM, FOOT_HEIGHT, FOOT_HEIGHT, WALK_SECONDS + MOVE_SECONDS)

# Moves the person that the player script names, east, in steps of a frame: teleporting it to where it would be at the
# speed of walking and then of running (the speeds of the animation files, which the engine compares the speed of the
# character with), then leaves it where it ends.
PERSON_SCRIPT = """\
local util = require('openfallout.util')

local person, from, elapsed = nil, nil, 0
local WALK = %g
local RUN = %g
local WALK_FOR = %g
local RUN_FOR = %g

return {
    eventHandlers = {
        OFTestMovePerson = function(object)
            person, from, elapsed = object, object.position, 0
        end,
    },
    engineHandlers = {
        onUpdate = function(dt)
            if person == nil then return end
            elapsed = elapsed + dt
            local distance
            if elapsed < WALK_FOR then
                distance = WALK * elapsed
            elseif elapsed < WALK_FOR + RUN_FOR then
                distance = WALK * WALK_FOR + RUN * (elapsed - WALK_FOR)
            else
                distance = WALK * WALK_FOR + RUN * RUN_FOR
            end
            person:teleport(person.cell, from + util.vector3(distance, 0, 0))
            if elapsed >= WALK_FOR + RUN_FOR then person = nil end
        end,
    },
}
""" % (plugin.WALK_VELOCITY, plugin.RUN_VELOCITY, WALK_FOR, RUN_FOR)

# Logs where the characters that follow packages are and which way they face, once a second. They are looked up by the form
# ID of their reference (the interior and the exterior cell each have their own).
PACKAGE_SCRIPT = """\
local nearby = require('openfallout.nearby')
local core = require('openfallout.core')

local actors = { @ACTORS@ }
local elapsed, nextLog = 0, 0

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            -- the walk ends at @UNTIL@ seconds, and the player is somewhere else after it
            if elapsed < nextLog or elapsed > @UNTIL@ then return end
            nextLog = nextLog + 1
            for _, actor in ipairs(actors) do
                for _, id in ipairs(actor[2]) do
                    local ok, line = pcall(function()
                        local object = nearby.getObjectByFormId(core.getFormId('OFTest.esm', id))
                        local p = object.position
                        return string.format('actor %s t=%.1f pos=%.1f,%.1f,%.1f yaw=%.3f', actor[1], elapsed,
                            p.x, p.y, p.z, object.rotation:getYaw())
                    end)
                    if ok then print('OFTEST', line) break end
                end
            end
        end,
    },
}
""".replace("@UNTIL@", str(WALK_SECONDS)).replace("@ACTORS@", ", ".join("{ '%s', { %d, %d } }" % (name, *plugin.PACKAGE_REF_IDS[name])
                                  for name in ("walker", "idler", "roamer")) + ", " + ", ".join(
                                      "{ '%s', { %d, %d } }" % (name, *plugin.FOLLOW_REF_IDS[name])
                                      for name in ("follower", "companion")) + ", " + ", ".join(
                                      "{ '%s', { %d, %d } }" % (name, *plugin.GATE_REF_IDS[name])
                                      for name in ("conditional", "scripted")))

# Opens the gate of the packages of the conditional character: sets the stage of quest C that the second of its packages
# needs (a global script, at the game time that the plugin names). Late in the walk it tells what the packages told the
# scripts: the globals that the scripts of the events of the packages and of the walker count in.
GATE_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')
local types = require('openfallout.types')

local elapsed, done, reported = 0, false, false

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if not done and elapsed >= @FROM@ then
                done = true
                local quest = world.quests.find('@QUEST@')
                print('OFTEST', 'gate opened', tostring(quest ~= nil and quest:setStage(@STAGE@)))
            end
            if not reported and elapsed >= @EVENTS_FROM@ then
                reported = true
                local g = world.globals
                print('OFTEST', 'package events', @EVENTS@)
                -- the talking activator is an object of the world, of its own type
                local found, valid, typed = pcall(function()
                    local intercom = world.getObjectByFormId(core.getFormId('OFTest.esm', @INTERCOM@))
                    return intercom:isValid(), intercom.type == types.ESM4TalkingActivator
                end)
                print('OFTEST', 'intercom', tostring(found and valid), tostring(found and typed))
            end
        end,
    },
}
""".replace("@FROM@", str(plugin.GATE_FROM)).replace("@QUEST@", plugin.QUEST_C_NAME).replace(
    "@STAGE@", str(plugin.QUEST_C_STAGE)).replace("@INTERCOM@", str(plugin.INTERCOM_REF_ID)).replace(
    "@EVENTS_FROM@", str(WALK_SECONDS - 2)).replace(
    "@EVENTS@", ", ".join(f"tostring(g.{name})" for _, name in plugin.PACKAGE_EVENT_GLOBALS.values()))

# Tells the companion, a character with no package, to follow the player (a global script, from the game time that the
# plugin names). The reference of each kind of cell is tried, the one that is loaded takes it.
FOLLOW_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')

local ids = { @IDS@ }
local elapsed, done = 0, false

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if done or elapsed < @FROM@ then return end
            for _, id in ipairs(ids) do
                local ok, actor = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', id))
                if ok and actor:isValid() then
                    actor:startFollowing(world.players[1], @DISTANCE@)
                    print('OFTEST', 'companion told to follow')
                    done = true
                end
            end
        end,
    },
}
""".replace("@IDS@", ", ".join(map(str, plugin.FOLLOW_REF_IDS["companion"]))).replace(
    "@FROM@", str(plugin.COMPANION_FROM)).replace("@DISTANCE@", str(plugin.FOLLOW_DISTANCES["companion"]))

# Reads the quests of the plugin through world.quests and world.globals once the quest script has had time to run (its
# delay is plugin.QUEST_DELAY and it sets the first stage on its plugin.QUEST_TICKS-th run), and logs what it finds. It
# also asks for a stage again that was reached (the quests do not allow that), for one the quest does not have, and
# sets the global from Lua.
QUEST_SCRIPT = """\
local world = require('openfallout.world')

local elapsed, done = 0, false

local function describe(quest)
    if not quest then return 'none' end
    local journal = {}
    for _, entry in ipairs(quest.journal) do
        table.insert(journal, entry.stage .. '=' .. entry.text)
    end
    return string.format('%s id=%s name=%s stage=%d running=%s completed=%s failed=%s journal=[%s]',
        quest.editorId, quest.id, quest.name, quest.stage, tostring(quest.running), tostring(quest.completed),
        tostring(quest.failed), table.concat(journal, ';'))
end

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if done or elapsed < @FROM@ then return end
            done = true
            local a = world.quests.find('@A@')
            local b = world.quests.find(string.lower('@B@'))
            print('OFTEST', 'quest a', describe(a))
            print('OFTEST', 'quest b', describe(b))
            if a and b then
                print('OFTEST', 'quest objective', tostring(b:objectiveDisplayed(@OBJECTIVE@)),
                    tostring(b:objectiveCompleted(@OBJECTIVE@)), tostring(b:objectiveDisplayed(@OBJECTIVE@ + 1)))
                -- the id as core.getFormId writes it ("FormId:0x" and digits), and the same without "FormId:"
                print('OFTEST', 'quest by id', tostring(world.quests.find(a.id) == a),
                    tostring(world.quests.find(string.sub(a.id, 8)) == a))
                print('OFTEST', 'quest again', tostring(a:setStage(@A_STAGE@)), tostring(a:setStage(99)),
                    tostring(a:stageDone(@A_STAGE@)), tostring(a:stageDone(99)))
            end
            print('OFTEST', 'quest missing', tostring(world.quests.find('NoSuchQuest')))
            print('OFTEST', 'quest counter', tostring(world.globals.@COUNTER@),
                tostring(world.globals.NoSuchGlobal))
            world.globals.@COUNTER@ = 100.9
            print('OFTEST', 'quest counter set', tostring(world.globals.@COUNTER@),
                tostring(pcall(function() world.globals.NoSuchGlobal = 1 end)))
        end,
    },
}
"""
QUEST_SCRIPT = (QUEST_SCRIPT.replace("@FROM@", str(plugin.QUEST_DELAY * (plugin.QUEST_TICKS + 5)))
                .replace("@A@", plugin.QUEST_A_NAME).replace("@B@", plugin.QUEST_B_NAME)
                .replace("@OBJECTIVE@", str(plugin.QUEST_OBJECTIVE))
                .replace("@A_STAGE@", str(plugin.QUEST_A_STAGE)).replace("@COUNTER@", plugin.COUNTER_NAME))

# A mod, named before the plugin it has as its master (--content OFTestMod.esp --content OFTest.esm): the engine puts the
# master first, as the games do, and the global of the master that the mod changes and the global that the mod adds are
# read by a script of the world.
MODS_SCRIPT = """\
local core = require('openfallout.core')
local world = require('openfallout.world')

local elapsed, done = 0, false

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if done or elapsed < 2 then return end
            done = true
            local names = {}
            for _, name in ipairs(core.contentFiles.list) do table.insert(names, name) end
            print('OFTEST', 'mods order', table.concat(names, ','))
            print('OFTEST', 'mods base', tostring(world.globals.@BASE@), tostring(world.globals.@ADDED@))
            print('OFTEST', 'mods id', tostring(core.getFormId('@MOD@', 0x800)))

            -- what the mod did to the cubes it changes: where each is (the cell it is listed in, none when the cell
            -- does not have it or it is deleted), whether it is enabled, and where it stands
            local interior = world.getCellById(core.getFormId('OFTest.esm', @CELL@))
            local exterior = world.getCellById(core.getFormId('OFTest.esm', @EXTERIOR@))
            local cells = { interior = interior:getAll(), exterior = exterior:getAll() }
            local wanted = { @REFS@ }
            for _, item in ipairs(wanted) do
                local found, object = 'none', nil
                for _, where in ipairs({ 'interior', 'exterior' }) do
                    for _, other in ipairs(cells[where]) do
                        if tostring(other.id) == tostring(core.getFormId(item[2], item[3])):gsub('^FormId:', '') then
                            found, object = where, other
                        end
                    end
                end
                if object then
                    local at = object.position
                    print('OFTEST', 'mods ref', item[1], found, tostring(object.enabled),
                        string.format('%d,%d,%d', at.x, at.y, at.z))
                else
                    print('OFTEST', 'mods ref', item[1], found)
                end
            end
            core.quit()
        end,
    },
}
"""
MODS_SCRIPT = (MODS_SCRIPT.replace("@BASE@", plugin.MOD_BASE_GLOBAL_NAME).replace("@ADDED@", plugin.MOD_ADDED_GLOBAL_NAME)
               .replace("@MOD@", plugin.MOD_ADDON).replace("@CELL@", str(plugin.CELL_ID))
               .replace("@EXTERIOR@", str(plugin.EXTERIOR_CELL_ID))
               .replace("@REFS@", ", ".join("{ '%s', 'OFTest.esm', %d }" % (key, form_id)
                                           for key, form_id in plugin.MOD_REFS.items())
                        + ", { 'new', '%s', 0x801 }" % plugin.MOD_ADDON))


# Archives and loose files. A global script reads four files through the VFS: one that is loose and in an archive (the
# games' bInvalidateOlderFiles decides which one counts), one that is only in an archive and that ArchiveInvalidation.txt
# names, one that is only in an archive, and one that is in the archive that is named after the mod (found without being
# asked for).
ARCHIVES_SCRIPT = """\
local vfs = require('openfallout.vfs')

local elapsed, done = 0, false

local function content(name)
    if not vfs.fileExists(name) then return 'missing' end
    local file = vfs.open(name)
    local text = file:read('*a')
    file:close()
    return (text:gsub('%s+$', ''))
end

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if done or elapsed < 2 then return end
            done = true
            print('OFTEST', 'archives', 'shared', content('oftest/shared.txt'))
            print('OFTEST', 'archives', 'hidden', content('oftest/hidden.txt'))
            print('OFTEST', 'archives', 'only', content('oftest/archiveonly.txt'))
            print('OFTEST', 'archives', 'mod', content('oftest/modarchive.txt'))
            require('openfallout.core').quit()
        end,
    },
}
"""


# The conversation, in an engine of its own because it pauses the game, which the walk must not be. A script of the world
# activates the talker (the player does, in the game) and a script of the player looks at the conversation with
# core.conversation: the greeting that the voice type and the sex of the talker pick (the one for another voice must not be
# said), its begin script, the window of the built-in script that shows it (the mode Dialogue) and moves on when the
# response has been read, the options (the topic at the top of the list, the goodbye) and the response to the first of
# them, which adds a topic and runs a script when it ends, then the topic it added, whose response ends the conversation.
DIALOGUE_FROM = 3.0
DIALOGUE_TIMEOUT = 15.0
# How long after the conversation the message box that its last response shows may wait for the harness to pick a button
DIALOGUE_BUTTON_TIMEOUT = 20.0
DIALOGUE_SCRIPT = """\
local core = require('openfallout.core')
local I = require('openfallout.interfaces')

local conversation = core.conversation
local elapsed, step, since, poll = 0, 0, 0, 0
local ask, added, buttonValue

local function log(...) print('OFTEST', 'dialogue', ...) end

local function describe(view)
    local lines, options = {}, {}
    for _, line in ipairs(view.lines) do table.insert(lines, line.text) end
    for _, option in ipairs(view.options) do table.insert(options, option.text) end
    return string.format('active=%s speaking=%s ended=%s speaker=%s lines=[%s] options=[%s] mode=%s',
        tostring(view.active), tostring(view.speaking), tostring(view.ended), view.speakerName,
        table.concat(lines, '|'), table.concat(options, '|'), tostring(I.UI.getMode()))
end

local function go(next)
    step, since = next, 0
end

return {
    engineHandlers = {
        -- The game pauses with the conversation, so the time is the real one
        onFrame = function()
            local dt = core.getRealFrameDuration()
            elapsed = elapsed + dt
            since = since + dt
            local view = conversation.get()
            if step == 0 then
                if elapsed >= @FROM@ then
                    log('available', tostring(conversation.available()))
                    core.sendGlobalEvent('OFTestTalk', {})
                    go(1)
                end
            elseif step == 1 and view.active then
                log('greeting', describe(view))
                core.sendGlobalEvent('OFTestReport', 'greeting')
                go(2)
            elseif step == 2 and view.active and not view.speaking then
                -- The window read the greeting and went on
                log('options', describe(view))
                core.sendGlobalEvent('OFTestReport', 'options')
                for _, option in ipairs(view.options) do
                    if option.text == '@ASK_PROMPT@' then ask = option.topic end
                end
                if ask then
                    local after = conversation.choose(ask)
                    log('asked', describe(after))
                end
                go(3)
            elseif step == 3 and view.active and not view.speaking then
                log('after ask', describe(view))
                core.sendGlobalEvent('OFTestReport', 'asked')
                for _, option in ipairs(view.options) do
                    if option.text == '@ADDED@' then added = option.topic end
                end
                if added then
                    log('added', describe(conversation.choose(added)))
                end
                go(4)
            elseif step == 4 and (view.ended or not view.active) then
                log('over', describe(view))
                go(5)
            elseif step == 5 and since >= 1 then
                -- The window went with the end of the conversation
                log('closed', describe(conversation.get()))
                core.sendGlobalEvent('OFTestReport', 'closed')
                go(6)
            elseif step == 6 then
                -- The last response showed a message box with buttons; the harness picks one with the keyboard, and the
                -- script of the talker reads which (the script of the world tells what it read)
                poll = poll + dt
                if poll >= 0.5 then
                    poll = 0
                    core.sendGlobalEvent('OFTestReport', 'button')
                end
                if buttonValue and buttonValue > 0 then
                    log('button', tostring(buttonValue))
                    go(7)
                elseif since >= @BUTTON_TIMEOUT@ then
                    log('button timeout')
                    go(7)
                end
            elseif step == 7 and since >= 0.5 then
                -- A second conversation that the script ends without the window: the window goes with it, and the game
                -- is not left paused
                core.sendGlobalEvent('OFTestTalk', {})
                go(8)
            elseif step == 8 and view.active then
                conversation.close()
                go(9)
            elseif step == 9 and since >= 1 then
                log('dropped', describe(conversation.get()))
                go(10)
            elseif step == 10 then
                core.quit()
            end
            if step ~= 0 and (step < 6 or step == 8) and since >= @TIMEOUT@ then
                log('timeout at step', tostring(step), describe(view))
                core.quit()
            end
        end,
    },
    eventHandlers = {
        OFTestButtonValue = function(value) buttonValue = value end,
    },
}
""".replace("@FROM@", str(DIALOGUE_FROM)).replace("@TIMEOUT@", str(DIALOGUE_TIMEOUT)).replace(
    "@BUTTON_TIMEOUT@", str(DIALOGUE_BUTTON_TIMEOUT)).replace(
    "@ASK_PROMPT@", plugin.ASK_PROMPT).replace("@ADDED@", plugin.ADDED_TOPIC_NAME)

DIALOGUE_HOST_SCRIPT = """\
local core = require('openfallout.core')
local world = require('openfallout.world')

return {
    eventHandlers = {
        OFTestTalk = function()
            local ok, talker = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', @TALKER_REF@))
            if ok and talker:isValid() then
                print('OFTEST', 'dialogue talker found')
                talker:activateBy(world.players[1])
            else
                print('OFTEST', 'dialogue talker missing')
            end
        end,
        OFTestReport = function(tag)
            print('OFTEST', 'dialogue globals', tag, tostring(world.globals.@BEGIN@), tostring(world.globals.@END@))
            if tag == 'button' then
                world.players[1]:sendEvent('OFTestButtonValue', world.globals.@BUTTON@)
                print('OFTEST', 'dialogue said', tostring(world.globals.@SAID@))
                print('OFTEST', 'dialogue ordered', tostring(world.globals.@ORDERED@))
                print('OFTEST', 'dialogue empty', tostring(world.globals.@EMPTY@))
                print('OFTEST', 'dialogue blocks', tostring(world.globals.@WRONG@), tostring(world.globals.@TALK@),
                    tostring(world.globals.@BOX@), tostring(world.globals.@ELSEWHERE@))
            end
        end,
    },
}
""".replace("@TALKER_REF@", str(plugin.TALKER_REF_ID)).replace("@BEGIN@", plugin.DIALOGUE_GLOBALS["begin"][1]).replace(
    "@END@", plugin.DIALOGUE_GLOBALS["end"][1]).replace("@BUTTON@", plugin.DIALOGUE_GLOBALS["button"][1]).replace(
    "@SAID@", plugin.DIALOGUE_GLOBALS["said"][1]).replace("@WRONG@", plugin.DIALOGUE_GLOBALS["wrong"][1]).replace(
    "@TALK@", plugin.DIALOGUE_GLOBALS["talk"][1]).replace("@BOX@", plugin.DIALOGUE_GLOBALS["box"][1]).replace(
    "@ELSEWHERE@", plugin.DIALOGUE_GLOBALS["elsewhere"][1]).replace(
    "@ORDERED@", plugin.DIALOGUE_GLOBALS["ordered"][1]).replace("@EMPTY@", plugin.DIALOGUE_GLOBALS["empty"][1])

# The making of the character, in an engine of its own because its screens pause the game. A script of the world sets the
# global that the script of a quest waits for, the script opens the menus (the race, then the name, SPECIAL with the
# tagged skills, the traits), and a script of the player looks at each screen as it opens (core.chargen) and does what a player
# does: the name is typed on the keyboard by the harness, the rest is picked with the functions the buttons of the
# screens call. The script of the quest reads what was made into globals once the last screen is done.
CHARGEN_FROM = 3.0
CHARGEN_TIMEOUT = 15.0
CHARGEN_SCRIPT = """\
local core = require('openfallout.core')
local I = require('openfallout.interfaces')

local chargen = core.chargen
local elapsed, step, since = 0, 0, 0

local function log(...) print('OFTEST', 'chargen', ...) end

local function describe(state)
    local attributes, tags, traits = {}, {}, {}
    for _, attribute in ipairs(state.attributes) do table.insert(attributes, attribute.value) end
    for _, skill in ipairs(state.skills) do
        if skill.tagged then table.insert(tags, skill.number) end
    end
    for _, trait in ipairs(state.traits) do
        table.insert(traits, trait.name .. (trait.picked and '*' or ''))
    end
    return string.format('name=%s female=%s points=%d attributes=[%s] tags=[%s] traits=[%s] races=%d skills=%d mode=%s',
        state.name, tostring(state.female), state.pointsLeft, table.concat(attributes, ','), table.concat(tags, ','),
        table.concat(traits, '|'), #state.races, #state.skills, tostring(I.UI.getMode()))
end

local function go(next)
    step, since = next, 0
end

local function raceTwo(state)
    for _, race in ipairs(state.races) do
        if race.name == '@RACE_TWO@' then return race.id end
    end
end

return {
    engineHandlers = {
        -- The game pauses with the screens, so the time is the real one
        onFrame = function()
            local dt = core.getRealFrameDuration()
            elapsed = elapsed + dt
            since = since + dt
            local page = chargen.page()
            if step == 0 then
                if elapsed >= @FROM@ then
                    core.sendGlobalEvent('OFTestChargenGo', {})
                    go(1)
                end
            elseif step == 1 and page and page.page == 'race' then
                local state = chargen.state()
                log('screen race', describe(state))
                for _, attribute in ipairs(state.attributes) do
                    log('attribute', attribute.number, attribute.name, attribute.description)
                end
                chargen.setFemale(true)
                chargen.setRace(raceTwo(state))
                log('race picked', tostring(chargen.state().race == raceTwo(state)), tostring(chargen.state().female))
                chargen.finish()
                go(2)
            elseif step == 2 and page and page.page == 'name' then
                -- the harness types the name on the keyboard and presses Return
                log('screen name', describe(chargen.state()))
                go(3)
            elseif step == 3 and page and page.page == 'special' then
                log('screen special', describe(chargen.state()))
                log('adjust', tostring(chargen.adjust(5, 3)), tostring(chargen.adjust(11, -2)),
                    tostring(chargen.adjust(10, 4)), tostring(chargen.adjust(6, 1)), tostring(chargen.adjust(11, -9)))
                log('special done', describe(chargen.state()))
                chargen.finish()
                go(4)
            elseif step == 4 and page and page.page == 'skills' then
                log('screen skills', describe(chargen.state()))
                log('tag', tostring(chargen.toggleTag(32)), tostring(chargen.toggleTag(41)),
                    tostring(chargen.toggleTag(44)), tostring(chargen.toggleTag(45)), tostring(chargen.toggleTag(33)))
                log('skills done', describe(chargen.state()))
                chargen.finish()
                go(5)
            elseif step == 5 and page and page.page == 'traits' then
                local state = chargen.state()
                log('screen traits', describe(state))
                local picks = {}
                for _, trait in ipairs(state.traits) do table.insert(picks, tostring(chargen.toggleTrait(trait.id))) end
                log('picks', table.concat(picks, ','))
                log('traits done', describe(chargen.state()))
                chargen.finish()
                go(6)
            elseif step == 6 and not page then
                log('closed', describe(chargen.state()))
                go(7)
            elseif step == 8 and since >= 0.5 then
                core.quit()
            end
            if step > 0 and step < 7 and since >= @TIMEOUT@ then
                log('timeout at step', tostring(step), tostring(page and page.page))
                core.quit()
            elseif step == 7 and since >= @TIMEOUT@ then
                log('timeout waiting for the script of the quest')
                core.quit()
            end
        end,
    },
    eventHandlers = {
        OFTestChargenDone = function()
            log('quest read')
            go(8)
        end,
    },
}
""".replace("@FROM@", str(CHARGEN_FROM)).replace("@TIMEOUT@", str(CHARGEN_TIMEOUT)).replace(
    "@RACE_TWO@", plugin.RACE_TWO_NAME)

CHARGEN_HOST_SCRIPT = """\
local world = require('openfallout.world')

local reported = false
local names = { @NAMES@ }

return {
    engineHandlers = {
        onUpdate = function()
            if reported or world.globals.@STATE@ ~= 2 then return end
            reported = true
            local parts = {}
            for _, name in ipairs(names) do
                table.insert(parts, name[1] .. '=' .. tostring(world.globals[name[2]]))
            end
            print('OFTEST', 'chargen', 'globals', table.concat(parts, ' '))
            world.players[1]:sendEvent('OFTestChargenDone')
        end,
    },
    eventHandlers = {
        OFTestChargenGo = function()
            world.globals.@GO@ = 1
        end,
    },
}
""".replace("@NAMES@", ", ".join("{ '%s', '%s' }" % (name, global_name)
                               for name, (_, global_name) in plugin.CHARGEN_GLOBALS.items())).replace(
    "@STATE@", plugin.CHARGEN_GLOBALS["state"][1]).replace("@GO@", plugin.CHARGEN_GLOBALS["go"][1])

# The new game, in an engine of its own: started with `--new-game` and no `--start`, the engine does what the games do for
# New Game. It finds the quest CG00 of the plugin, starts it and runs its MenuMode block, which sets stage 0, and the result
# script of the stage takes the controls of the player, moves the player to a marker of the interior cell (which faces a way)
# and sets stage 5. The script of the quest shows a message box with two buttons; autoplay.lua (the tool in
# scripts/openfallout/autoplay, which is what runs the intro on real data) picks the first, which sets stage 10, which gives
# the controls back and opens the screen for the name, which autoplay.lua finishes. A script of the player logs the box and
# the controls while it is open (and that the box takes no button that is not there); this script of the world logs the
# quest, the controls and the player once the game has run for NEW_GAME_FROM seconds, then quits.
NEW_GAME_FROM = 6.0 # game seconds; the box stops the game for a moment, and the two gates of autoplay are tried one second apart
CONTROL_KEYS = "'playercontrols', 'playerfighting', 'playerlooking', 'playerviewswitch'"
NEW_GAME_PLAYER_SCRIPT = """\
local core = require('openfallout.core')
local input = require('openfallout.input')
local self = require('openfallout.self')
local types = require('openfallout.types')

local seen, drawn, reported = nil, false, false

return {
    engineHandlers = {
        onFrame = function()
            -- the player draws a weapon while the box is open; the quest takes fighting away on its own at the end,
            -- which has to put the weapon away
            if drawn and not reported and input.getControlSwitch('playercontrols') then
                reported = true
                print('OFTEST', 'newgame stance', 'after', tostring(types.Actor.getStance(self) == types.Actor.STANCE.Nothing))
            end
            local box = core.messageBox.current()
            if not box or box.serial == seen then return end
            seen = box.serial
            types.Actor.setStance(self, types.Actor.STANCE.Weapon)
            drawn = true
            print('OFTEST', 'newgame stance', 'drawn', tostring(types.Actor.getStance(self) == types.Actor.STANCE.Weapon))
            local row = {}
            for _, key in ipairs({ @KEYS@ }) do table.insert(row, key .. '=' .. tostring(input.getControlSwitch(key))) end
            print('OFTEST', 'newgame box', string.format('buttons=%d first=%s second=%s', #box.buttons, box.buttons[1],
                box.buttons[2]), table.concat(row, ' '), string.format('yaw=%.3f', self.rotation:getYaw()),
                string.format('at=%.0f,%.0f', self.position.x, self.position.y),
                'bounds=' .. tostring(core.messageBox.press(0)) .. ',' .. tostring(core.messageBox.press(#box.buttons + 1)))
        end,
    },
}
""".replace("@KEYS@", CONTROL_KEYS)
NEW_GAME_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')
local types = require('openfallout.types')

local elapsed, done = 0, false

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if done or elapsed < @FROM@ then return end
            done = true
            local quest = world.quests.find('@QUEST@')
            if quest then
                print('OFTEST', 'newgame quest', string.format('stage=%d running=%s', quest.stage, tostring(quest.running)))
            else
                print('OFTEST', 'newgame quest', 'none')
            end
            local player = world.players[1]
            local row = {}
            for _, key in ipairs({ @KEYS@ }) do
                table.insert(row, key .. '=' .. tostring(types.Player.getControlSwitch(player, key)))
            end
            print('OFTEST', 'newgame controls', table.concat(row, ' '))
            -- what the gates of autoplay did: the player came into the trigger and activated the scripted object
            print('OFTEST', 'newgame gates', string.format('enters=%d any=%d activated=%d', world.globals.@ENTERS@,
                world.globals.@ANY@, world.globals.@ACTIVATED@))
            print('OFTEST', 'newgame player', tostring(player.cell.isExterior), tostring(player.cell.name),
                string.format('%.1f,%.1f,%.1f', player.position.x, player.position.y, player.position.z))
            core.quit()
        end,
    },
}
"""
NEW_GAME_START_ELSEWHERE = f"{plugin.WORLD_NAME}:0,0" # the exterior cell the engine logs as OFTestWorldCell (0, 0)
NEW_GAME_SCRIPT = (NEW_GAME_SCRIPT.replace("@FROM@", str(NEW_GAME_FROM)).replace("@QUEST@", plugin.NEW_GAME_QUEST_NAME)
                   .replace("@KEYS@", CONTROL_KEYS).replace("@ENTERS@", plugin.TRIGGER_GLOBALS["enters"][1])
                   .replace("@ANY@", plugin.TRIGGER_GLOBALS["any"][1])
                   .replace("@ACTIVATED@", plugin.OBJECT_GLOBALS["activated"][1]))

# The table of the gates of autoplay (what the intro waits for: a trigger volume to walk into, an object to activate), for the
# plugin of the test: when the quest stands at its last stage for a second, autoplayworld.lua puts the player in the trigger
# volume and makes the player activate the scripted object, once each
NEW_GAME_GATES = f"""\
return {{
    settle = 1,
    tries = 1,
    boxes = {{
        {{ file = 'OFTest.esm', ref = {plugin.NEW_GAME_MESSAGE_ID}, button = -1 }},
    }},
    gates = {{
        {{ quest = '{plugin.NEW_GAME_QUEST_NAME}', stage = {plugin.NEW_GAME_LAST_STAGE}, file = 'OFTest.esm', ref = {plugin.TRIGGER_REF_ID}, how = 'walk' }},
        {{ quest = '{plugin.NEW_GAME_QUEST_NAME}', stage = {plugin.NEW_GAME_LAST_STAGE}, file = 'OFTest.esm', ref = {plugin.SCRIPTED_REF_IDS["interior"]}, how = 'activate' }},
    }},
}}
"""

# The gates of autoplay that shoot, take the player to another cell by its form ID and activate several things at once, on the
# arena: once the quest of the arena has given the player the rifle (stage 10) autoplay shoots three times at the activator whose
# script counts the blows, shoots at the dummy until it is dead, goes to the room (which has three switches), activates the three
# switches and goes back to the arena. Each gate waits for the one before it (`gap`, real seconds: of the gates that are ready the
# first of the table goes first, so the gaps are the same).
AUTOPLAY_GATE = "quest = '{quest}', stage = {stage}, file = 'OFTest.esm'".format(
    quest=plugin.ARENA_QUEST_NAME, stage=plugin.ARENA_SETUP_STAGE)
AUTOPLAY_GATES = f"""\
return {{
    settle = 1,
    tries = 1,
    gates = {{
        {{ {AUTOPLAY_GATE}, ref = {plugin.AUTOPLAY_TARGET_REF_ID}, how = 'fire', shots = 3 }},
        {{ {AUTOPLAY_GATE}, ref = {plugin.ARENA_DUMMY_REF_ID}, how = 'fire', shots = 20, untilDead = true, gap = 4 }},
        {{ {AUTOPLAY_GATE}, cell = {plugin.AUTOPLAY_ROOM_CELL_ID}, at = {{ {", ".join(map(str, plugin.AUTOPLAY_ROOM_AT))} }}, how = 'teleport', gap = 4 }},
        {{ {AUTOPLAY_GATE}, refs = {{ {", ".join(map(str, plugin.AUTOPLAY_SWITCH_REF_IDS))} }}, how = 'activate', gap = 4 }},
        {{ {AUTOPLAY_GATE}, cell = {plugin.ARENA_CELL_ID}, at = {{ 0.0, 0.0, 0.0 }}, how = 'teleport', gap = 4 }},
    }},
}}
"""

# The world script of that test: puts the quest of the arena at the stage that gives the player the rifle, and says every
# few seconds what the scripts of the activators and of the dummy counted and where the player is
AUTOPLAY_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')

local elapsed, nextReport, started = 0, 0, false
local function report()
    local g, player = world.globals, world.players[1]
    print('OFTEST', 'autoplay', 'state', string.format('t=%.0f targetHits=%.0f dummyHits=%.0f deaths=%.0f switches=%.0f cell=%s',
        elapsed, g.@TARGET_HITS@, g.@HITS@, g.@DEATHS@, g.@SWITCHES@, tostring(player.cell.name)))
end

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if not started and elapsed >= 1 then
                started = true
                local quest = world.quests.find('@QUEST@')
                print('OFTEST', 'autoplay', 'stage', tostring(quest ~= nil and quest:setStage(@SETUP@)))
            end
            if elapsed >= nextReport then
                nextReport = nextReport + 2
                report()
            end
            if elapsed >= @END@ then
                report()
                core.quit()
            end
        end,
    },
}
"""
AUTOPLAY_END = 30
AUTOPLAY_SCRIPT = (AUTOPLAY_SCRIPT.replace("@QUEST@", plugin.ARENA_QUEST_NAME).replace("@SETUP@", str(plugin.ARENA_SETUP_STAGE))
                   .replace("@END@", str(AUTOPLAY_END)).replace("@TARGET_HITS@", plugin.AUTOPLAY_GLOBALS["target_hits"][1])
                   .replace("@SWITCHES@", plugin.AUTOPLAY_GLOBALS["switches"][1])
                   .replace("@HITS@", plugin.ARENA_GLOBALS["hits"][1]).replace("@DEATHS@", plugin.ARENA_GLOBALS["deaths"][1]))

# Walks the player into the trigger of the plugin and out of it again, and logs what the script of the trigger counted.
# The box is turned a quarter about z, so that its long side runs along y: a point 100 to the side along y is inside it,
# and one 100 along x is not (the box is 40 across).
TRIGGER_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')
local util = require('openfallout.util')

local x, y = @X@, @Y@
local elapsed, step = 0, 0
local function put(dx, dy)
    local player = world.players[1]
    player:teleport(player.cell, util.vector3(x + dx, y + dy, 0))
end
local function report(tag)
    local g = world.globals
    print('OFTEST', 'trigger', tag, string.format('enters=%.0f any=%.0f stays=%.0f leaves=%.0f', g.@ENTERS@, g.@ANY@,
        g.@STAYS@, g.@LEAVES@))
end

-- After the walk the player activates the chair in the box (from outside the box): it is put at the chair, and the script of
-- the trigger, which asks the game whether the player uses the chair every frame, sees it; it is moved away from there and
-- does not use the chair any more
local function chair()
    local ok, object = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', @CHAIR@))
    return ok and object:isValid() and object or nil
end
local function sitter(tag)
    local ok, object = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', @SITTER@))
    local seat = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', @SITTER_CHAIR@)) and
        world.getObjectByFormId(core.getFormId('OFTest.esm', @SITTER_CHAIR@))
    print('OFTEST', 'sitter', tag, tostring(world.globals.@SITTER_GLOBAL@),
        ok and seat and string.format('%.0f', (object.position - seat.position):length()) or 'no sitter')
end
-- Where the player is from the chair (across, along: the chair is not turned), and the way it faces
local function sits(tag)
    local object, player = chair(), world.players[1]
    local offset = object and player.position - object.position
    print('OFTEST', 'furniture', tag, tostring(world.globals.@SITS@),
        offset and string.format('%.0f\t%.0f\t%.0f\t%.1f', offset.x, offset.y, offset.z, player.rotation:getYaw())
            or 'no chair')
end

-- The sitter's chair has one entry and the sitter is on it: the player who activates that chair is not put there
local function occupied(tag)
    local ok, seat = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', @SITTER_CHAIR@))
    print('OFTEST', 'occupied', tag, ok and string.format('%.0f', (world.players[1].position - seat.position):length())
        or 'no chair')
end
local function takeSitterChair()
    local ok, seat = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', @SITTER_CHAIR@))
    if ok then seat:activateBy(world.players[1]) end
end

local steps = {
    { 1.5, function() report('before') put(0, 100) end },
    { 3.0, function() report('inside') put(100, 0) end },
    { 4.5, function() report('outside') put(0, -100) end },
    { 6.0, function() report('again') put(100, 0) end },
    { 7.0, function() sits('before') local object = chair() if object then object:activateBy(world.players[1]) end end },
    { 8.0, function() sits('sat') put(300, 0) end },
    { 9.0, function() sits('away') end },
    { 12.5, function() occupied('before') takeSitterChair() end },
    { 13.5, function() occupied('after') end },
    { 15.0, function() sitter('later') core.quit() end },
}

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            local next = steps[step + 1]
            if next and elapsed >= next[1] then
                step = step + 1
                next[2]()
            end
        end,
    },
}
"""
TRIGGER_SCRIPT = (TRIGGER_SCRIPT.replace("@X@", str(plugin.TRIGGER_PLACE[0])).replace("@Y@", str(plugin.TRIGGER_PLACE[1]))
                  .replace("@ENTERS@", plugin.TRIGGER_GLOBALS["enters"][1]).replace("@ANY@", plugin.TRIGGER_GLOBALS["any"][1])
                  .replace("@STAYS@", plugin.TRIGGER_GLOBALS["stays"][1])
                  .replace("@LEAVES@", plugin.TRIGGER_GLOBALS["leaves"][1])
                  .replace("@SITS@", plugin.TRIGGER_GLOBALS["sits"][1]).replace("@CHAIR@", str(plugin.FURNITURE_REF_ID))
                  .replace("@SITTER_GLOBAL@", plugin.SITTER_GLOBAL[1]).replace("@SITTER_CHAIR@", str(plugin.SITTER_CHAIR_REF_ID))
                  .replace("@SITTER@", str(plugin.SITTER_REF_ID)))

# Saves: a script of the world waits for the quests of the plugin to run, sets a global, asks the script of the menu for a
# save, changes the state (the global, and quest C, which has not been started) and asks the menu for the game to be loaded
# again: what the script then reads must be the state of the save, and the quests that the game ran before the save must
# not run again (the global is not given the seven that stage 10 of quest A adds once more). A second engine started with
# `--load-savegame` on the save must read the same.
SAVES_FROM = 2.0
SAVES_COUNTER = 123
SAVES_CHANGED_COUNTER = 999
SAVES_SETTLE = 0.5
SAVES_WORLD_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')
local types = require('openfallout.types')
local util = require('openfallout.util')

-- What runs when: a list of functions and the second they are due. A game that is loaded starts over with the script, so
-- what comes after a load is scheduled by onLoad and the rest by the first update of a game that was not loaded.
local pending, clock, loaded, started = {}, 0, false, false
local function after(seconds, fn) pending[#pending + 1] = { clock + seconds, fn } end

-- The cubes of the plugin for the saves, the record the cube that is made has, and where they are put
local REFS = { moved = @MOVED@, hidden = @HIDDEN@, grown = @GROWN@, gone = @GONE@, doomed = @DOOMED@, stacked = @STACKED@ }
local ORDER = { 'moved', 'hidden', 'grown', 'gone', 'doomed', 'stacked' }
local CUBE = core.getFormId('OFTest.esm', @CUBE@)
local MOVED_TO = util.vector3(@MOVED_TO@)
local MADE_AT = util.vector3(@MADE_AT@)

local function find(id)
    local ok, object = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', id))
    if ok and object:isValid() and object.count > 0 then return object end
end

-- The person of the first cell gets a local script (saveslocal.lua, attached by this script as it is a custom one) that
-- counts the events it is sent and keeps the count in its saved data
local PERSON = @PERSON@
local function bump(times)
    local person = find(PERSON)
    if person == nil then return end
    for _ = 1, times do person:sendEvent('OFTestBump', {}) end
end

local function cubesAt(cell, place)
    local found = {}
    for _, other in ipairs(cell:getAll()) do
        if other.recordId == CUBE and (other.position - place):length() < 5 then found[#found + 1] = other end
    end
    return found
end

local function describe(tag)
    local a = world.quests.find('@A@')
    local b = world.quests.find('@B@')
    local c = world.quests.find('@C@')
    print('OFTEST', 'saves', tag, string.format(
        'a_stage=%d a_done=%s b_stage=%d b_completed=%s b_shown=%s b_finished=%s c_stage=%d c_done=%s counter=%s',
        a.stage, tostring(a:stageDone(@A_STAGE@)), b.stage, tostring(b.completed),
        tostring(b:objectiveDisplayed(@OBJECTIVE@)), tostring(b:objectiveCompleted(@OBJECTIVE@)), c.stage,
        tostring(c:stageDone(@C_STAGE@)), tostring(world.globals.@COUNTER@)))
    local fields = {}
    for _, name in ipairs(ORDER) do
        local object = find(REFS[name])
        if object == nil then
            fields[#fields + 1] = name .. '=gone'
        else
            local at = object.position
            fields[#fields + 1] = string.format('%s=%.0f,%.0f,%.0f,%s,%.1f,%d', name, at.x, at.y, at.z,
                tostring(object.enabled), object.scale, object.count)
        end
    end
    local made, madeId = 0, 'none'
    for _, other in ipairs(cubesAt(world.players[1].cell, MADE_AT)) do
        if other.enabled then
            made = made + 1
            madeId = other.id
        end
    end
    fields[#fields + 1] = 'made=' .. made
    print('OFTEST', 'saves refs', tag, table.concat(fields, ' '))
    -- The number that the world made up for the cube is the one Lua knew it by
    print('OFTEST', 'saves made', tag, madeId)
    local person = find(PERSON)
    if person ~= nil then
        person:sendEvent('OFTestAsk', { tag = tag })
    else
        print('OFTEST', 'saves local', tag, 'count=none')
    end
    local g = world.globals
    print('OFTEST', 'saves characters', tag, string.format('items=%d dead=%d luck=%d karma=%d',
        g.@G_ITEMS@, g.@G_DEAD@, g.@G_LUCK@, g.@G_KARMA@))
end

-- Sets the stage that writes what the game knows into the globals, and reads them a moment after
local function report(tag, stage)
    world.quests.find('@DQUEST@'):setStage(stage)
    after(@SETTLE@, function() describe(tag) end)
end

local function askMenu(event)
    types.Player.sendMenuEvent(world.players[1], event, {})
end

return {
    engineHandlers = {
        onSave = function() return { saved = true } end,
        -- A game that is loaded, in this engine or in another, goes straight to reading the state
        onLoad = function()
            loaded = true
            after(0.2, function() report('loaded', @S_LOADED@) end)
            after(3, function() report('later', @S_LATER@) end)
            after(3 + 2 * @SETTLE@, function() core.quit() end)
        end,
        onUpdate = function(dt)
            clock = clock + dt
            if not started then
                started = true
                if not loaded then
                    after(@FROM@, function()
                        world.globals.@COUNTER@ = @VALUE@
                        -- What happens before the game is saved: one cube is moved, one is disabled, one is made bigger,
                        -- one is deleted, a cube that is no reference of the plugin is made, and the quest of the
                        -- characters gives the player rounds, luck and karma and kills the person
                        local cell = world.players[1].cell
                        find(REFS.moved):teleport(cell, MOVED_TO)
                        find(REFS.hidden).enabled = false
                        find(REFS.grown):setScale(@SCALE@)
                        find(REFS.gone):remove()
                        find(REFS.stacked):remove(@TAKEN@)
                        find(PERSON):addScript('scripts/saveslocal.lua')
                        world.createObject(CUBE, 1):teleport(cell, MADE_AT)
                        world.quests.find('@DQUEST@'):setStage(@S_GIVE@)
                        -- The frame after the calls is when the world does them
                        after(@SETTLE@, function()
                            bump(@BUMPS@)
                            report('before', @S_BEFORE@)
                            after(2 * @SETTLE@, function() askMenu('OFTestSave') end)
                        end)
                    end)
                end
            end
            local due = {}
            for i = #pending, 1, -1 do
                if pending[i][1] <= clock then due[#due + 1] = table.remove(pending, i) end
            end
            for i = #due, 1, -1 do due[i][2]() end
        end,
    },
    eventHandlers = {
        OFTestCount = function(event)
            print('OFTEST', 'saves local', event.tag, string.format('count=%d', event.count))
        end,
        OFTestSaved = function()
            world.quests.find('@C@'):setStage(@C_STAGE@)
            world.globals.@COUNTER@ = @CHANGED@
            -- And after it: the cubes are put back, and one more is deleted, with the cube that was made; the quest takes
            -- back what it gave
            local cell = world.players[1].cell
            find(REFS.moved):teleport(cell, util.vector3(@PLACE@))
            find(REFS.hidden).enabled = true
            find(REFS.grown):setScale(1)
            find(REFS.doomed):remove()
            find(REFS.stacked):remove(@TAKEN_AFTER@)
            bump(@BUMPS_AFTER@)
            for _, other in ipairs(cubesAt(cell, MADE_AT)) do other:remove() end
            world.quests.find('@DQUEST@'):setStage(@S_CHANGE@)
            after(@SETTLE@, function()
                report('changed', @S_CHANGED@)
                after(2 * @SETTLE@, function() askMenu('OFTestLoad') end)
            end)
        end,
    },
}
"""
SAVES_WORLD_SCRIPT = (SAVES_WORLD_SCRIPT.replace("@FROM@", str(SAVES_FROM)).replace("@A@", plugin.QUEST_A_NAME)
                      .replace("@B@", plugin.QUEST_B_NAME).replace("@C@", plugin.QUEST_C_NAME)
                      .replace("@A_STAGE@", str(plugin.QUEST_A_STAGE)).replace("@C_STAGE@", str(plugin.QUEST_C_STAGE))
                      .replace("@OBJECTIVE@", str(plugin.QUEST_OBJECTIVE)).replace("@COUNTER@", plugin.COUNTER_NAME)
                      .replace("@VALUE@", str(SAVES_COUNTER)).replace("@CHANGED@", str(SAVES_CHANGED_COUNTER))
                      .replace("@SETTLE@", str(SAVES_SETTLE)).replace("@CUBE@", str(plugin.CUBE_ID))
                      .replace("@SCALE@", str(plugin.SAVED_SCALE))
                      .replace("@MOVED_TO@", ", ".join(map(str, plugin.SAVED_MOVED_TO)))
                      .replace("@MADE_AT@", ", ".join(map(str, plugin.SAVED_MADE_AT)))
                      .replace("@PLACE@", ", ".join(map(str, (*plugin.SAVED_PLACE, 128.0))))
                      .replace("@MOVED@", str(plugin.SAVED_REF_IDS["moved"]))
                      .replace("@HIDDEN@", str(plugin.SAVED_REF_IDS["hidden"]))
                      .replace("@GROWN@", str(plugin.SAVED_REF_IDS["grown"]))
                      .replace("@GONE@", str(plugin.SAVED_REF_IDS["gone"]))
                      .replace("@DOOMED@", str(plugin.SAVED_REF_IDS["doomed"]))
                      .replace("@STACKED@", str(plugin.SAVED_REF_IDS["stacked"]))
                      .replace("@TAKEN@", str(plugin.SAVED_TAKEN)).replace("@TAKEN_AFTER@", str(plugin.SAVED_TAKEN_AFTER))
                      .replace("@PERSON@", str(plugin.NPC_REF_ID)).replace("@BUMPS@", str(plugin.SAVED_BUMPS))
                      .replace("@BUMPS_AFTER@", str(plugin.SAVED_BUMPS_AFTER))
                      .replace("@DQUEST@", plugin.SAVED_QUEST_NAME)
                      .replace("@S_GIVE@", str(plugin.SAVED_STAGES["give"]))
                      .replace("@S_CHANGE@", str(plugin.SAVED_STAGES["change"]))
                      .replace("@S_BEFORE@", str(plugin.SAVED_STAGES["before"]))
                      .replace("@S_CHANGED@", str(plugin.SAVED_STAGES["changed"]))
                      .replace("@S_LOADED@", str(plugin.SAVED_STAGES["loaded"]))
                      .replace("@S_LATER@", str(plugin.SAVED_STAGES["later"]))
                      .replace("@G_ITEMS@", plugin.SAVED_GLOBALS["items"][1])
                      .replace("@G_DEAD@", plugin.SAVED_GLOBALS["dead"][1])
                      .replace("@G_LUCK@", plugin.SAVED_GLOBALS["luck"][1])
                      .replace("@G_KARMA@", plugin.SAVED_GLOBALS["karma"][1]))
SAVES_LOCAL_SCRIPT = """\
local core = require('openfallout.core')

local count = 0

return {
    engineHandlers = {
        onSave = function() return { count = count } end,
        onLoad = function(data) count = data.count end,
    },
    eventHandlers = {
        OFTestBump = function() count = count + 1 end,
        OFTestAsk = function(event) core.sendGlobalEvent('OFTestCount', { tag = event.tag, count = count }) end,
    },
}
"""
SAVES_MENU_SCRIPT = """\
local menu = require('openfallout.menu')
local core = require('openfallout.core')

return {
    eventHandlers = {
        OFTestSave = function()
            menu.saveGame('oftest')
            print('OFTEST', 'saves', 'saved', tostring(menu.getCurrentSaveDir()))
            core.sendGlobalEvent('OFTestSaved', {})
        end,
        OFTestLoad = function()
            local dir = menu.getCurrentSaveDir()
            for slot, info in pairs(menu.getSaves(dir)) do
                if info.description == 'oftest' then
                    print('OFTEST', 'saves', 'loading', dir, slot)
                    menu.loadGame(dir, slot)
                    return
                end
            end
            print('OFTEST', 'saves', 'no slot', tostring(dir))
        end,
    },
}
"""

# The probe of the saves in the tool that plays the intros (scripts/openfallout/autoplay/autoplayprobe.lua and
# autoplaysaves.lua): it saves the game when quest A has stood at its stage, loads it back when quest C has stood at its stage
# (once), and a game that was loaded says whether the quests are those of the save and saves again. The script of the test
# only moves the two quests, a second after the start and four seconds later; a second engine is started on the first save.
SAVES_PROBE_GATES = f"""\
return {{
    settle = 1000,
    tries = 0,
    gates = {{}},
    saves = {{
        after = 1,
        resaveAfter = 1,
        save = {{ {{ quest = '{plugin.QUEST_A_NAME}', stage = {plugin.QUEST_A_STAGE} }} }},
        load = {{ {{ quest = '{plugin.QUEST_C_NAME}', stage = {plugin.QUEST_C_STAGE} }} }},
    }},
}}
"""
SAVES_PROBE_SCRIPT = f"""\
local core = require('openfallout.core')
local world = require('openfallout.world')

local clock, step = 0, 0
return {{
    engineHandlers = {{
        onSave = function() return {{ saved = true }} end,
        onUpdate = function(dt)
            clock = clock + dt
            if step == 0 and clock >= 1 then
                step = 1
                world.quests.find('{plugin.QUEST_A_NAME}'):setStage({plugin.QUEST_A_STAGE})
            elseif step == 1 and clock >= 5 then
                step = 2
                world.quests.find('{plugin.QUEST_C_NAME}'):setStage({plugin.QUEST_C_STAGE})
            elseif step == 2 and clock >= 14 then
                step = 3
                core.quit()
            end
        end,
    }},
}}
"""

# Reads the scripted object of the plugin, whose script (a compiled script of an object) runs in the engine: it
# disables a cube on load, counts frames, measures its distance to the player and counts activations by the player. The
# script looks the objects up by the form ID of the reference of the kind of cell that is loaded. After the time the
# scripts have had to run it logs what the globals say, activates the object by the player twice, enables the cube again
# from Lua and, a little later, logs what the script saw.
OBJECT_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')
local util = require('openfallout.util')

local scripted_ids, victim_ids, mover_ids, far_ids = { @SCRIPTED@ }, { @VICTIM@ }, { @MOVER@ }, { @FAR@ }
local sleeper_ids = { @SLEEPER@ }
local setter_ids = { @SETTER@ }
local traveller_ids = { @TRAVELLER@ }
local crosser_ids = { @CROSSER@ }
local traveller_object, setter_object = nil, nil
local doomed_ids = { @DOOMED@ }
local follower_ids, inverse_ids = { @FOLLOWER@ }, { @INVERSE@ }
local locker_ids = { @LOCKER@ }
local elapsed, step = 0, 0
local scripted_object, mover_object, far_object, far_seen, defaults = nil, nil, nil, 0, 0
local by_object = 0

local function find(ids)
    for _, id in ipairs(ids) do
        local ok, object = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', id))
        if ok and object:isValid() then return object end
    end
end

return {
    engineHandlers = {
        -- The activations of the scripted object that reach the handlers of Lua: its OnActivate block takes the first
        -- one and passes the second on with Activate
        onActivate = function(object, actor)
            if scripted_object ~= nil and object == scripted_object then defaults = defaults + 1 end
            -- An activator that is no actor: the third activation has the scripted object activate the mover
            if scripted_object ~= nil and actor == scripted_object then by_object = by_object + 1 end
        end,
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if step == 0 and elapsed >= @FROM@ then
                step = 1
                local scripted, victim = find(scripted_ids), find(victim_ids)
                scripted_object = scripted
                local player = world.players[1]
                if not (scripted and victim) then
                    print('OFTEST', 'object missing', tostring(scripted ~= nil), tostring(victim ~= nil))
                    step = 3
                    return
                end
                local g = world.globals
                print('OFTEST', 'object state', tostring(victim.enabled), tostring(g.@LOADS@ == 1),
                    tostring(g.@FRAMES@ > 10), tostring(g.@VICTIM_DISABLED@), tostring(g.@ACTIVATED@),
                    string.format('%d %d', math.floor(g.@DISTANCE@),
                        math.floor((scripted.position - player.position):length())))
                -- The cubes that have the victim for their enable parent: the script disabled the victim, so the one that
                -- is enabled with it is disabled and the one that has the flag that inverses it is enabled
                print('OFTEST', 'object followers', tostring(find(follower_ids).enabled), tostring(find(inverse_ids).enabled))
                scripted:activateBy(player)
                local once = g.@ACTIVATED@
                scripted:activateBy(player)
                print('OFTEST', 'object activated', tostring(once), tostring(g.@ACTIVATED@))
                scripted:activateBy(player)
                victim.enabled = true
                -- An object made while the game runs has no reference number of a plugin, and the scripts of Fallout
                -- must let it be put in the cell and activated like the others
                local ok, err = pcall(function()
                    local made = world.createObject(core.getFormId('OFTest.esm', @CUBE@), 1)
                    made:teleport(scripted.cell, scripted.position + util.vector3(0, -200, 0))
                    made:activateBy(player)
                end)
                print('OFTEST', 'object made', tostring(ok), ok and '' or tostring(err))
            elseif step == 1 and elapsed >= @FROM@ + 1 then
                step = 3
                local victim = find(victim_ids)
                print('OFTEST', 'object victim again', tostring(victim.enabled),
                    tostring(world.globals.@VICTIM_DISABLED@))
                print('OFTEST', 'object defaults', tostring(defaults))
                print('OFTEST', 'object by object', tostring(by_object))
                local g = world.globals
                print('OFTEST', 'object idler', tostring(g.@ITEMS@), tostring(g.@RANK@), tostring(g.@JOINED@),
                    tostring(g.@DEAD@))
                print('OFTEST', 'object player', tostring(g.@PLAYER_RANK@))
                print('OFTEST', 'object listed', tostring(g.@LISTED@))
                -- The locks the script saw and changed, the cube it destroyed and deleted, and the list it added to
                print('OFTEST', 'object locks', tostring(g.@LOCKED@), tostring(g.@LOCK_LEVEL@), tostring(g.@UNLOCKED@),
                    tostring(g.@RELOCKED@), tostring(g.@RELOCK_LEVEL@))
                print('OFTEST', 'object doomed', tostring(g.@DESTROYED@), tostring(g.@DOOMED_DISABLED@),
                    tostring(find(doomed_ids).enabled))
                print('OFTEST', 'object forms', tostring(g.@IN_LIST@), tostring(g.@LIST_BEFORE@),
                    tostring(g.@LIST_AFTER@))
                -- The perk the script added twice and removed, the note it added, and the base form of a cube
                print('OFTEST', 'object progress', tostring(g.@PERK_BEFORE@), tostring(g.@PERK_AFTER@),
                    tostring(g.@PERK_TEAMMATES@), tostring(g.@PERK_REMOVED@), tostring(g.@NOTE_BEFORE@),
                    tostring(g.@NOTE_AFTER@), tostring(g.@IS_ID@), tostring(g.@IS_NOT_ID@))
                -- The link of the record, the cell and worldspace of the cube, and the loads of the script of the beast
                print('OFTEST', 'object world', tostring(g.@LINKED@), tostring(g.@LINKED_NONE@),
                    tostring(g.@SAME_CELL@), tostring(g.@IN_WORLDSPACE@), tostring(g.@TEMPLATE_LOADS@))
                -- IsPlayerInRegion for the region the cell lists and the player stands in, the one it lists and the player
                -- is not in, the one it does not list and the one of another worldspace
                print('OFTEST', 'object regions', tostring(g.@REGION_IN@), tostring(g.@REGION_OUT@),
                    tostring(g.@REGION_UNLISTED@), tostring(g.@REGION_OTHER_WORLD@))
                -- The clock: GetCurrentTime, the global GameHour and GetGlobalValue of it
                print('OFTEST', 'object clock', string.format('%.4f', g.@HOUR_COMMAND@),
                    string.format('%.4f', g.@HOUR_GLOBAL@), string.format('%.4f', g.@GLOBAL_VALUE@))
                -- The actor values of the locker cube (set, changed and hurt), its health share, whether it is dead, the
                -- karma of the player as an actor value and the health of a weapon
                print('OFTEST', 'object values', string.format('%.4f', g.@AV_VARIABLE@),
                    string.format('%.4f', g.@AV_SHARE@), string.format('%.4f', g.@AV_DEAD@),
                    string.format('%.4f', g.@AV_KARMA@), string.format('%.4f', g.@AV_WEAPON@),
                    string.format('%.4f', g.@AV_STRENGTH@), string.format('%.4f', g.@AV_LUCK@),
                    string.format('%.4f', g.@AV_SPEED@), string.format('%.4f', g.@AV_PERSON_KARMA@),
                    string.format('%.4f', g.@AV_DLC_THERE@), string.format('%.4f', g.@AV_DLC_MISSING@),
                    string.format('%.4f', g.@AV_PERSON_LEVEL@), string.format('%.4f', g.@AV_DAY_OF_WEEK@),
                    string.format('%.4f', g.@AV_CURRENT_PACKAGE@), string.format('%.4f', g.@AV_USER_LEVEL@))
                -- The cubes that PlaceAtMe made in front of the locker cube: what the call told, how many stand there
                -- and how many of them are cubes of the same record
                local locker = find(locker_ids)
                if locker ~= nil then
                    local here, cubes = 0, 0
                    for _, other in ipairs(locker.cell:getAll()) do
                        if math.abs(other.position.x - locker.position.x) < 5
                            and math.abs(other.position.y - locker.position.y - @PLACE_DISTANCE@) < 5 then
                            here = here + 1
                            if other.recordId == locker.recordId then cubes = cubes + 1 end
                        end
                    end
                    print('OFTEST', 'object placed', tostring(g.@PLACED@ ~= 0), here, cubes)
                end
                -- Where the script saw the cube that it moved to a marker, and where the world has put it
                local mover = find(mover_ids)
                mover_object, far_object = mover, find(far_ids)
                local at = mover.position
                print('OFTEST', 'object mover', string.format('%.1f %.1f %.1f', g.@MOVER_X@, g.@MOVER_Y@, g.@MOVER_ANGLE@),
                    string.format('%.1f %.1f %.1f', at.x, at.y, math.deg(mover.rotation:getYaw())),
                    string.format('%.1f', g.@MOVER_NEAR@))
                -- The cube that the script gave an x in the next cell: whether the script saw it in that cell at once,
                -- and where the world has put it
                local crosser = find(crosser_ids)
                if crosser ~= nil then
                    local cell = crosser.cell
                    print('OFTEST', 'object crosser', string.format('%.1f', g.@CROSSED@), tostring(cell.isExterior),
                        cell.isExterior and string.format('%d %d', cell.gridX, cell.gridY) or 'none',
                        string.format('%.1f', crosser.position.x))
                end
            elseif step == 3 and elapsed >= @MOVE_FROM@ then
                step = 4
                print('OFTEST', 'player moves now')
                -- The cube that the result script of quest C (set from Lua, so it runs on the thread of Lua) gave an x
                -- and then a y
                local setter = find(setter_ids)
                setter_object = setter
                if setter ~= nil then
                    print('OFTEST', 'object setter', string.format('%.1f %.1f', setter.position.x, setter.position.y))
                end
                -- What the script sees of the far cube before the player is in the other cell, where the script of
                -- that cell moves its own cube and keeps the same global
                far_seen = world.globals.@FAR_X@
                traveller_object = find(traveller_ids)
                -- The script sets the x of the player first, in the cell the player is in
                world.globals.@HOP@ = 1
            elseif step == 4 and elapsed >= @MOVE_FROM@ + 0.5 then
                step = 5
                local player = world.players[1]
                print('OFTEST', 'object hopped', tostring(player.cell.isExterior), tostring(player.cell.name),
                    string.format('%.1f', player.position.x), tostring(world.globals.@HOP@),
                    string.format('%.1f %.1f %.1f', setter_object.position.x, setter_object.position.y,
                        math.deg(setter_object.rotation:getYaw())))
                world.globals.@MOVE_NOW@ = 1
            elseif step == 5 and elapsed >= @MOVE_LOOK@ then
                step = 6
                local mover, player = mover_object, world.players[1]
                print('OFTEST', 'object moved', string.format('%.1f', mover.position.z),
                    tostring(player.cell.isExterior), tostring(player.cell.name),
                    string.format('%.1f %.1f', player.position.x, player.position.y), tostring(world.globals.@MOVE_NOW@))
                -- The cube of each kind of cell that a script sent to a marker while its cell was not loaded: the one in
                -- the cell the player is in now has been put there
                for _, id in ipairs(sleeper_ids) do
                    local ok, sleeper = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', id))
                    if ok and sleeper:isValid() then
                        print('OFTEST', 'object sleeper', tostring(sleeper.cell.isExterior),
                            string.format('%.1f %.1f', sleeper.position.x, sleeper.position.y),
                            string.format('%.1f', math.deg(sleeper.rotation:getPitch())))
                    end
                end
                -- The cube that the script sent to the marker before it moved the player: it goes first, so it is in
                -- the cell that the player is in now
                local travelled = traveller_object.position
                print('OFTEST', 'object traveller', tostring(traveller_object.cell.isExterior),
                    string.format('%.1f %.1f', travelled.x, travelled.y))
                local far = far_object.position
                print('OFTEST', 'object far mover', tostring(far_object.cell.isExterior), tostring(far_object.cell.name),
                    string.format('%.1f %.1f', far.x, far.y), string.format('%.1f', far_seen))
            end
        end,
    },
}
"""
OBJECT_FROM = 4.0  # game seconds before the Lua script looks, enough for the cell to load and a number of frames to pass
OBJECT_SCRIPT = (OBJECT_SCRIPT
                 .replace("@SCRIPTED@", ", ".join(map(str, plugin.SCRIPTED_REF_IDS.values())))
                 .replace("@VICTIM@", ", ".join(map(str, plugin.VICTIM_REF_IDS.values())))
                 .replace("@FOLLOWER@", ", ".join(map(str, plugin.FOLLOWER_REF_IDS.values())))
                 .replace("@INVERSE@", ", ".join(map(str, plugin.INVERSE_REF_IDS.values())))
                 .replace("@MOVER@", ", ".join(map(str, plugin.MOVER_REF_IDS.values())))
                 .replace("@FAR@", ", ".join(map(str, plugin.FAR_MOVER_REF_IDS.values())))
                 .replace("@SLEEPER@", ", ".join(map(str, plugin.SLEEPER_REF_IDS.values())))
                 .replace("@SETTER@", ", ".join(map(str, plugin.SETTER_REF_IDS.values())))
                 .replace("@TRAVELLER@", ", ".join(map(str, plugin.TRAVELLER_REF_IDS.values())))
                 .replace("@CROSSER@", ", ".join(map(str, plugin.CROSSER_REF_IDS.values())))
                 .replace("@CROSSED@", plugin.MOVE_GLOBALS["crossed"][1])
                 .replace("@DOOMED@", ", ".join(map(str, plugin.DOOMED_REF_IDS.values())))
                 .replace("@LOCKER@", ", ".join(map(str, plugin.LOCKER_REF_IDS.values())))
                 .replace("@PLACE_DISTANCE@", str(plugin.PLACE_DISTANCE))
                 .replace("@PLACED@", plugin.WORLD_QUERY_GLOBALS["placed"][1])
                 .replace("@REGION_IN@", plugin.REGION_GLOBALS["in"][1])
                 .replace("@REGION_OUT@", plugin.REGION_GLOBALS["out"][1])
                 .replace("@REGION_UNLISTED@", plugin.REGION_GLOBALS["unlisted"][1])
                 .replace("@REGION_OTHER_WORLD@", plugin.REGION_GLOBALS["other_world"][1])
                 .replace("@HOUR_COMMAND@", plugin.CLOCK_GLOBALS["hour_command"][1])
                 .replace("@HOUR_GLOBAL@", plugin.CLOCK_GLOBALS["hour_global"][1])
                 .replace("@GLOBAL_VALUE@", plugin.CLOCK_GLOBALS["global_value"][1])
                 .replace("@AV_VARIABLE@", plugin.ACTOR_VALUE_GLOBALS["variable"][1])
                 .replace("@AV_SHARE@", plugin.ACTOR_VALUE_GLOBALS["share"][1])
                 .replace("@AV_DEAD@", plugin.ACTOR_VALUE_GLOBALS["dead"][1])
                 .replace("@AV_KARMA@", plugin.ACTOR_VALUE_GLOBALS["karma"][1])
                 .replace("@AV_WEAPON@", plugin.ACTOR_VALUE_GLOBALS["weapon"][1])
                 .replace("@AV_STRENGTH@", plugin.ACTOR_VALUE_GLOBALS["strength"][1])
                 .replace("@AV_LUCK@", plugin.ACTOR_VALUE_GLOBALS["luck"][1])
                 .replace("@AV_SPEED@", plugin.ACTOR_VALUE_GLOBALS["speed"][1])
                 .replace("@AV_PERSON_KARMA@", plugin.ACTOR_VALUE_GLOBALS["person_karma"][1])
                 .replace("@AV_DLC_THERE@", plugin.ACTOR_VALUE_GLOBALS["dlc_there"][1])
                 .replace("@AV_DLC_MISSING@", plugin.ACTOR_VALUE_GLOBALS["dlc_missing"][1])
                 .replace("@AV_PERSON_LEVEL@", plugin.ACTOR_VALUE_GLOBALS["person_level"][1])
                 .replace("@AV_DAY_OF_WEEK@", plugin.ACTOR_VALUE_GLOBALS["day_of_week"][1])
                 .replace("@AV_CURRENT_PACKAGE@", plugin.ACTOR_VALUE_GLOBALS["current_package"][1])
                 .replace("@AV_USER_LEVEL@", plugin.ACTOR_VALUE_GLOBALS["user_level"][1])
                 .replace("@LOCKED@", plugin.STATE_GLOBALS["locked"][1])
                 .replace("@LOCK_LEVEL@", plugin.STATE_GLOBALS["lock_level"][1])
                 .replace("@UNLOCKED@", plugin.STATE_GLOBALS["unlocked"][1])
                 .replace("@RELOCKED@", plugin.STATE_GLOBALS["relocked"][1])
                 .replace("@RELOCK_LEVEL@", plugin.STATE_GLOBALS["relock_level"][1])
                 .replace("@DESTROYED@", plugin.STATE_GLOBALS["destroyed"][1])
                 .replace("@DOOMED_DISABLED@", plugin.STATE_GLOBALS["doomed_disabled"][1])
                 .replace("@IN_LIST@", plugin.STATE_GLOBALS["in_list"][1])
                 .replace("@LIST_BEFORE@", plugin.STATE_GLOBALS["list_before"][1])
                 .replace("@LIST_AFTER@", plugin.STATE_GLOBALS["list_after"][1])
                 .replace("@PERK_BEFORE@", plugin.PROGRESS_GLOBALS["perk_before"][1])
                 .replace("@PERK_AFTER@", plugin.PROGRESS_GLOBALS["perk_after"][1])
                 .replace("@PERK_TEAMMATES@", plugin.PROGRESS_GLOBALS["perk_teammates"][1])
                 .replace("@PERK_REMOVED@", plugin.PROGRESS_GLOBALS["perk_removed"][1])
                 .replace("@NOTE_BEFORE@", plugin.PROGRESS_GLOBALS["note_before"][1])
                 .replace("@NOTE_AFTER@", plugin.PROGRESS_GLOBALS["note_after"][1])
                 .replace("@IS_ID@", plugin.PROGRESS_GLOBALS["is_id"][1])
                 .replace("@IS_NOT_ID@", plugin.PROGRESS_GLOBALS["is_not_id"][1])
                 .replace("@LINKED@", plugin.WORLD_QUERY_GLOBALS["linked"][1])
                 .replace("@LINKED_NONE@", plugin.WORLD_QUERY_GLOBALS["linked_none"][1])
                 .replace("@SAME_CELL@", plugin.WORLD_QUERY_GLOBALS["same_cell"][1])
                 .replace("@IN_WORLDSPACE@", plugin.WORLD_QUERY_GLOBALS["in_worldspace"][1])
                 .replace("@TEMPLATE_LOADS@", plugin.WORLD_QUERY_GLOBALS["template_loads"][1])
                 .replace("@FAR_X@", plugin.MOVE_GLOBALS["far"][1])
                 .replace("@MOVER_X@", plugin.MOVE_GLOBALS["x"][1]).replace("@MOVER_Y@", plugin.MOVE_GLOBALS["y"][1])
                 .replace("@MOVER_ANGLE@", plugin.MOVE_GLOBALS["angle"][1])
                 .replace("@MOVER_NEAR@", plugin.MOVE_GLOBALS["near"][1])
                 .replace("@MOVE_NOW@", plugin.MOVE_GLOBALS["now"][1])
                 .replace("@HOP@", plugin.MOVE_GLOBALS["hop"][1])
                 .replace("@MOVE_FROM@", str(MOVE_FROM)).replace("@MOVE_LOOK@", str(MOVE_LOOK))
                 .replace("@CUBE@", str(plugin.CUBE_ID))
                 .replace("@FROM@", str(OBJECT_FROM))
                 .replace("@LOADS@", plugin.OBJECT_GLOBALS["loads"][1])
                 .replace("@FRAMES@", plugin.OBJECT_GLOBALS["frames"][1])
                 .replace("@ACTIVATED@", plugin.OBJECT_GLOBALS["activated"][1])
                 .replace("@DISTANCE@", plugin.OBJECT_GLOBALS["distance"][1])
                 .replace("@VICTIM_DISABLED@", plugin.OBJECT_GLOBALS["victim"][1])
                 .replace("@ITEMS@", plugin.OBJECT_GLOBALS["items"][1])
                 .replace("@RANK@", plugin.OBJECT_GLOBALS["rank"][1])
                 .replace("@JOINED@", plugin.OBJECT_GLOBALS["joined"][1])
                 .replace("@DEAD@", plugin.OBJECT_GLOBALS["dead"][1])
                 .replace("@PLAYER_RANK@", plugin.OBJECT_GLOBALS["player_rank"][1])
                 .replace("@LISTED@", plugin.OBJECT_GLOBALS["listed"][1]))

# Where the three characters start relative to the player, and where the walker should end
PACKAGE_START = {name: plugin.PACKAGE_PLACES[name] for name in ("walker", "idler", "roamer")}
# and the two characters whose packages depend on scripts
GATE_START = {name: plugin.GATE_PLACES[name] for name in ("conditional", "scripted")}
GATE_MARGIN = 1.0  # seconds either side of GATE_FROM that the two Lua scripts' clocks may differ by
MOVE_SLACK = 2.0  # units, and degrees, that a moved cube or player may be off
OBJECT_DISTANCE_SLACK = 60  # how far the distance a script measures may be from the one Lua sees (the player walks)
ARRIVAL_RANGE = 30.0  # how close to the marker of a package for the night the walker may come, units
TRAVEL_SLACK = 20.0  # how far from the radius of its package the walker may have stopped (it stops at the edge of it)
FOLLOW_SLACK = 64.0 + 40.0  # the slack of a follower (it starts to walk beyond it) and a margin
FOLLOW_YAW_RANGE = 0.4  # how far from the way to the player a follower that has come to a stop faces, radians
UNMOVED_RANGE = 2.0  # how far a character that must stay may be from where it was put
ROAM_SLACK = 30.0  # how far outside the radius of its package the roamer may be (the radius of a package is in the data)
WALK_YAW_RANGE = 0.35  # how far from north the walker faces while it walks north, radians
YAW_RANGE = 0.2  # how far from the way the marker faces the walker is when it has arrived
ACTOR_LINE = re.compile(r"OFTEST\tactor (\w+) t=([\d.]+) pos=" + r"(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)" + r" yaw=(-?[\d.]+)")

NUMBER = r"(-?[\d.]+)"
VECTOR = ",".join([NUMBER] * 3)
SAMPLE = re.compile(r"OFTEST\tt=([\d.]+) exterior=(\w+) name=(.*) pos=" + VECTOR + " cam=" + VECTOR)
RAY = re.compile(r"OFTEST\tray down hit=\t(\w+)\t(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)")
RENDER_RAY = re.compile(r"OFTEST\trender ray (\w+) hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?")
WATER_SIDE_RAY_LINE = re.compile(r"OFTEST\tray water side hit=\t(\w+)")
CREATURE_ACTOR_RAY = re.compile(r"OFTEST\tray creature (\w+) hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?\t(.*)")
PERSON_RAY = re.compile(r"OFTEST\tray person hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?\t(.*)")
WALL_RAY = re.compile(r"OFTEST\tray wall hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?")
WALL_RENDER_RAY = re.compile(r"OFTEST\trender ray wall hit=\t(\w+)")
WATER_RAY = re.compile(r"OFTEST\tray water hit=\t(\w+)\t(?:(-?[\d.]+),(-?[\d.]+),(-?[\d.]+))?")
# a ray at the foot of the moved person: whether it hit, where along x, and where the person was along x then
MOVING_FOOT_RAY = re.compile(r"OFTEST\trender ray (foot(?:walk|run|stand)) hit=\t(\w+)\t(?:(-?[\d.]+),-?[\d.]+,-?[\d.]+)?"
                             r"\tperson\t(-?[\d.]+),")


# The arena: the quest gives the player the rifle (EquipItem), and a global script reads what the game knows after each
# step. The player shoots the dummy (core.combat.fire at the object, the way the player script of the engine does when
# the ray of its aim finds one) until it is dead; then the raider, who waits beyond the sight of a character, is put in
# front of the player, and the test watches it notice the player, come within the range of its pistol and shoot. A
# bystander (who never starts a fight, has no package and no weapon) is shot once at the same time, and the test watches it
# go for the player. At the end the player is given two grenades, which are their own ammunition, and throws three times.
ARENA_SCRIPT = """\
local world = require('openfallout.world')
local core = require('openfallout.core')
local util = require('openfallout.util')

local probes = { @PROBES@ }
local used, elapsed, step = 0, 0, 0

local function find(id)
    local ok, object = pcall(world.getObjectByFormId, core.getFormId('OFTest.esm', id))
    return ok and object:isValid() and object or nil
end
local function stage(number)
    local quest = world.quests.find('@QUEST@')
    print('OFTEST', 'arena', 'stage', number, tostring(quest ~= nil and quest:setStage(number)))
end
-- Asks the quest to write what the game knows into the globals (the stage runs in the next frame)
local function ask()
    used = used + 1
    stage(probes[used])
end
local function report(tag)
    local g = world.globals
    local player, raider = world.players[1], find(@RAIDER@)
    print('OFTEST', 'arena', tag, string.format(
        'armed=%.0f out=%.0f ammo=%.0f health=%.1f dummyDead=%.0f raiderDead=%.0f raiderCombat=%.0f raiderAmmo=%.0f '
        .. 'hits=%.0f deaths=%.0f started=%.0f raiderDistance=%s',
        g.@ARMED@, g.@OUT@, g.@AMMO@, g.@HEALTH@, g.@DUMMY_DEAD@, g.@RAIDER_DEAD@, g.@RAIDER_COMBAT@, g.@RAIDER_AMMO@,
        g.@HITS@, g.@DEATHS@, g.@STARTED@,
        raider and string.format('%.0f', (raider.position - player.position):length()) or 'none'))
    local bystander = find(@BYSTANDER@)
    if bystander then
        print('OFTEST', 'arena', 'bystander', tag, string.format('distance=%.0f combat=%s',
            (bystander.position - player.position):length(),
            tostring(core.combat.inCombat(core.getFormId('OFTest.esm', @BYSTANDER@)))))
    end
end

local steps = {
    { 1.0, function() stage(@SETUP@) end },
    { 2.0, ask },
    { 2.5, function()
        report('armed')
        world.players[1]:sendEvent('OFTestArenaFire', { target = find(@DUMMY@) })
    end },
    { 5.0, ask },
    { 5.2, function()
        -- The dead dummy is activated: its things can be taken (the player script says what it was shown)
        local dummy = find(@DUMMY@)
        if dummy then dummy:activateBy(world.players[1]) end
        world.players[1]:sendEvent('OFTestArenaLoot', {})
    end },
    { 5.5, function()
        report('shot')
        world.players[1]:sendEvent('OFTestArenaPipboy', {})
        world.players[1]:sendEvent('OFTestArenaFire', {})
        world.players[1]:sendEvent('OFTestArenaFire', { target = find(@BYSTANDER@), once = true })
        local raider = find(@RAIDER@)
        if raider then raider:teleport(raider.cell, util.vector3(@COMES@)) end
    end },
    { 5.8, function()
        -- Experience for two levels: the level up screen opens by itself
        stage(@LEVEL@)
        world.players[1]:sendEvent('OFTestArenaLevel', {})
    end },
}
for second = 6, 20 do
    table.insert(steps, { second, ask })
    table.insert(steps, { second + 0.5, function() report('raider ' .. second) end })
end
-- The grenades: the player has them in its hands, throws three times at the bystander, and the last throw is made with
-- the hands, as the grenades are gone
table.insert(steps, { 21, function() stage(@GRENADES@) end })
table.insert(steps, { 22, function()
    world.players[1]:sendEvent('OFTestArenaThrow', { target = find(@BYSTANDER@), count = 3 })
end })
table.insert(steps, { 25, core.quit })

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            local next = steps[step + 1]
            if next and elapsed >= next[1] then
                step = step + 1
                next[2]()
            end
        end,
    },
    eventHandlers = {
        -- The player script asks for the bench when it is done with the level up: a script gives it the scrap and shows
        -- the menu of the bench
        OFTestArenaCraftNow = function()
            stage(@CRAFT@)
            world.players[1]:sendEvent('OFTestArenaCraft', {})
        end,
        -- The player script is done with the bench: V.A.T.S. is tried on the target, and its screen opened with it
        OFTestArenaVatsNow = function()
            world.players[1]:sendEvent('OFTestArenaVats', { target = find(@TARGET@) })
        end,
        OFTestArenaVatsOpen = function()
            world.players[1]:sendEvent('OpenVats', { targets = { find(@TARGET@) } })
        end,
    },
}
"""
for _name, _value in (("@PROBES@", ", ".join(map(str, plugin.ARENA_PROBE_STAGES))), ("@QUEST@", plugin.ARENA_QUEST_NAME),
                      ("@RAIDER@", str(plugin.ARENA_RAIDER_REF_ID)), ("@DUMMY@", str(plugin.ARENA_DUMMY_REF_ID)),
                      ("@BYSTANDER@", str(plugin.ARENA_BYSTANDER_REF_ID)),
                      ("@TARGET@", str(plugin.ARENA_TARGET_REF_ID)),
                      ("@SETUP@", str(plugin.ARENA_SETUP_STAGE)), ("@GRENADES@", str(plugin.ARENA_GRENADE_STAGE)),
                      ("@LEVEL@", str(plugin.ARENA_LEVEL_STAGE)),
                      ("@CRAFT@", str(plugin.ARENA_CRAFT_STAGE)),
                      ("@COMES@", ", ".join(str(int(value)) for value in plugin.ARENA_RAIDER_COMES))):
    ARENA_SCRIPT = ARENA_SCRIPT.replace(_name, _value)
for _key, _tag in (("armed", "ARMED"), ("weapon_out", "OUT"), ("ammo", "AMMO"), ("health", "HEALTH"),
                   ("dummy_dead", "DUMMY_DEAD"), ("raider_dead", "RAIDER_DEAD"), ("raider_combat", "RAIDER_COMBAT"),
                   ("raider_ammo", "RAIDER_AMMO"), ("hits", "HITS"), ("deaths", "DEATHS"), ("started", "STARTED")):
    ARENA_SCRIPT = ARENA_SCRIPT.replace("@" + _tag + "@", plugin.ARENA_GLOBALS[_key][1])

# The player script of the arena: shoots at the object it is told to, once the weapon is ready again, as the player script of
# the engine does, and logs each shot
ARENA_PLAYER_SCRIPT = """\
local core = require('openfallout.core')
local I = require('openfallout.interfaces')

local target, shots, once = nil, 0, false
local throwing, thrown, planned = nil, 0, 0
-- Frames since the Pip-Boy was asked for, to say what the screen is after it had a few to open
local pipboyFrames = nil
local lootFrames = nil
-- Frames since the level up was asked for, and since its screen was seen
local levelFrames = nil
local levelSeen = 0
local levelEscape = nil
-- Frames since the bench was asked for
local craftFrames = nil
-- V.A.T.S.: the target, the frames since it was asked for, and the action points after the attacks
local vatsTarget, vatsFrames, vatsPoints, vatsStart = nil, nil, 0, nil

return {
    eventHandlers = {
        OFTestArenaThrow = function(data)
            throwing, thrown, planned = data.target, 0, data.count
        end,
        OFTestArenaPipboy = function()
            local pipboy = core.pipboy
            local stats = pipboy.stats()
            local equipped, items = 0, pipboy.items()
            for _, item in ipairs(items) do
                if item.equipped then equipped = equipped + 1 end
            end
            print('OFTEST', 'arena', 'pipboy', string.format(
                'level=%d experience=%d title=%s health=%.0f/%.0f items=%d equipped=%d attributes=%d skills=%d perks=%d',
                stats.level, stats.experience, stats.karmaTitle, stats.health, stats.maxHealth, #items, equipped,
                #pipboy.attributes(), #pipboy.skills(), #pipboy.perks()))
            -- The lists and cards: the rounds the rifle takes can be used and the others can not, the key is on the keyring,
            -- and the card of the rifle tells its damage and its ammunition with the rounds in the clip and in reserve
            local function named(name)
                for _, item in ipairs(items) do
                    if item.name == name then return item end
                end
                return { usable = false, key = false, cardDamage = 0, hasAmmo = false, ammoName = '', clip = 0, loaded = 0,
                    reserve = 0, id = '' }
            end
            local rounds, slugs, key, rifle = named('Test Rounds'), named('Test Slugs'), named('Test Key'),
                named('OFTestArenaRifle')
            print('OFTEST', 'arena', 'pipboyitems', string.format(
                'rounds=%s slugs=%s slugsUse=[%s] key=%s keyUsable=%s rifle=%.1f,%s,%s,%d,%d,%d newVegas=%s threshold=%.1f',
                tostring(rounds.usable), tostring(slugs.usable), pipboy.use(slugs.id), tostring(key.key),
                tostring(key.usable), rifle.cardDamage, tostring(rifle.hasAmmo), rifle.ammoName, rifle.clip,
                rifle.loaded, rifle.reserve, tostring(stats.newVegas), stats.threshold))
            I.UI.setMode('Interface', { windows = { 'Inventory' } })
            pipboyFrames = 0
        end,
        OFTestArenaLoot = function()
            lootFrames = 0
        end,
        OFTestArenaLevel = function()
            levelFrames = 0
        end,
        OFTestArenaCraft = function()
            craftFrames = 0
        end,
        OFTestArenaVats = function(data)
            vatsTarget = data.target
            vatsFrames = 0
        end,
        OFTestArenaFire = function(data)
            target = data.target
            once = data.once == true
            if target then
                local weapon = core.combat.weapon()
                print('OFTEST', 'arena', 'weapon', weapon.weapon, tostring(weapon.melee), weapon.range, weapon.interval,
                    weapon.rounds)
            end
        end,
    },
    engineHandlers = {
        -- Called in the frames while a screen stops the game as well
        onFrame = function()
            if pipboyFrames then
                pipboyFrames = pipboyFrames + 1
                if pipboyFrames == 5 then
                    print('OFTEST', 'arena', 'mode', tostring(I.UI.getMode()))
                    I.UI.removeMode('Interface')
                    pipboyFrames = nil
                end
            end
            if lootFrames then
                lootFrames = lootFrames + 1
                if lootFrames == 5 then
                    local loot = core.pipboy.loot()
                    local items, count = loot and core.pipboy.items(loot.holder) or {}, 0
                    for _, item in ipairs(items) do count = count + item.count end
                    print('OFTEST', 'arena', 'loot', loot and string.format('mode=%s items=%d count=%d',
                        tostring(I.UI.getMode()), #items, count) or 'none')
                    core.pipboy.closeLoot()
                    lootFrames = nil
                end
            end
            if levelFrames then
                levelFrames = levelFrames + 1
                -- The screen of the level up opens by itself when the engine has told of the levels
                if I.UI.getMode() == 'Interface' or levelFrames > 1500 then
                    levelSeen = levelSeen + 1
                end
                if levelSeen == 5 then
                    local stats = core.pipboy.stats()
                    local mode = tostring(I.UI.getMode())
                    local spent = core.pipboy.spendSkillPoints(32, math.min(stats.skillPoints, 3))
                    local after = core.pipboy.stats()
                    -- The screen takes back a point that it gave (Reset, the minus of a skill)
                    local refunded = core.pipboy.refundSkillPoints(32, 1)
                    local back = core.pipboy.stats()
                    print('OFTEST', 'arena', 'levelup', string.format(
                        'mode=%s level=%d skillPoints=%d perkPoints=%d spent=%d skillPointsAfter=%d refunded=%d '
                        .. 'skillPointsBack=%d',
                        mode, stats.level, stats.skillPoints, stats.perkPoints, spent, after.skillPoints, refunded,
                        back.skillPoints))
                    -- Escape: the screen of a level up is not left with points still to give, it comes back
                    I.UI.removeMode('Interface')
                    levelFrames = nil
                    levelEscape = 0
                end
            end
            if levelEscape then
                levelEscape = levelEscape + 1
                if levelEscape == 60 or levelEscape == 180 then
                    local back = I.UI.getMode()
                    -- Everything is given, as the buttons of the screen do (the points, then a perk, if one is offered)
                    core.pipboy.spendSkillPoints(32, core.pipboy.stats().skillPoints)
                    for _, skill in ipairs(core.pipboy.skills()) do
                        local left = core.pipboy.stats().skillPoints
                        if left > 0 then core.pipboy.spendSkillPoints(skill.number, left) end
                    end
                    for _, perk in ipairs(core.pipboy.offeredPerks()) do
                        if perk.available ~= false and core.pipboy.stats().perkPoints > 0 then
                            core.pipboy.choosePerk(perk.id)
                        end
                    end
                    local stats = core.pipboy.stats()
                    -- The experience reached two levels, and each has a screen of its own: the first comes back after
                    -- Escape, and when all of it is given the second opens
                    print('OFTEST', 'arena', levelEscape == 60 and 'levelback' or 'levelnext',
                        string.format('mode=%s skillPoints=%d perkPoints=%d offered=%d', tostring(back), stats.skillPoints,
                            stats.perkPoints, #core.pipboy.offeredPerks()))
                    I.UI.removeMode('Interface')
                elseif levelEscape == 120 then
                    -- The first level is done, so the screen of the second one is open
                    local next = I.UI.getMode()
                    local stats = core.pipboy.stats()
                    print('OFTEST', 'arena', 'levelsecond', string.format('mode=%s skillPoints=%d', tostring(next),
                        stats.skillPoints))
                    levelEscape = 121
                elseif levelEscape == 240 then
                    print('OFTEST', 'arena', 'levelgone', 'mode=' .. tostring(I.UI.getMode()))
                    levelEscape = nil
                    core.sendGlobalEvent('OFTestArenaCraftNow', {})
                end
            end
            if craftFrames then
                craftFrames = craftFrames + 1
                if craftFrames == 10 then
                    craftFrames = nil
                    local pipboy = core.pipboy
                    local menu = pipboy.recipes()
                    if not menu then
                        print('OFTEST', 'arena', 'recipes', 'none')
                    else
                        local statuses, ready, hard = {}, nil, nil
                        for _, recipe in ipairs(menu.recipes) do
                            statuses[#statuses + 1] = recipe.status
                            if recipe.status == 'ready' then ready = recipe elseif recipe.status == 'skill' then hard = recipe end
                        end
                        local ingredient = ready and ready.ingredients[1]
                        print('OFTEST', 'arena', 'recipes', string.format('name=%s mode=%s count=%d statuses=%s scrap=%s',
                            menu.name, tostring(I.UI.getMode()), #menu.recipes, table.concat(statuses, ','),
                            ingredient and string.format('%d/%d', ingredient.have, ingredient.count) or 'none'))
                        local function count(id)
                            for _, item in ipairs(pipboy.items()) do
                                if item.id == id then return item.count end
                            end
                            return 0
                        end
                        local rounds = ready and ready.outputs[1].id or ''
                        local before = count(rounds)
                        local made = ready and pipboy.craft(ready.id) or 'none'
                        local denied = hard and pipboy.craft(hard.id) or 'none'
                        local again = ready and pipboy.craft(ready.id) or 'none'
                        print('OFTEST', 'arena', 'craft', string.format('made=%s denied=%s again=%s gained=%d', made, denied,
                            again, count(rounds) - before))
                        pipboy.closeRecipes()
                    end
                    core.sendGlobalEvent('OFTestArenaVatsNow', {})
                end
            end
            if vatsFrames then
                vatsFrames = vatsFrames + 1
                local vats = core.vats
                if vatsFrames == 1 then
                    local state, parts = vats.state(), vats.parts(vatsTarget)
                    local names, chances = {}, {}
                    for _, part in ipairs(parts) do
                        names[#names + 1] = part.target
                        chances[#chances + 1] = string.format('%.0f', part.chance)
                    end
                    print('OFTEST', 'arena', 'vats', string.format(
                        'usable=%s can=%s points=%.0f/%.0f cost=%.0f parts=%s chances=%s', tostring(state.usable),
                        tostring(vats.canTarget(vatsTarget)), state.actionPoints, state.maxActionPoints, state.cost,
                        table.concat(names, ','), table.concat(chances, ',')))
                elseif vatsFrames == 5 then
                    -- The screen opens on the target it is given, once the bench is out of the way
                    if I.UI.getMode() ~= nil then
                        vatsFrames = 4
                    else
                        core.sendGlobalEvent('OFTestArenaVatsOpen', {})
                    end
                elseif vatsFrames == 15 then
                    print('OFTEST', 'arena', 'vatsscreen', string.format('mode=%s open=%s', tostring(I.UI.getMode()),
                        tostring(vats.state().open)))
                    I.UI.removeMode('Interface')
                elseif vatsFrames == 17 then
                    print('OFTEST', 'arena', 'vatsclosed', string.format('mode=%s open=%s', tostring(I.UI.getMode()),
                        tostring(vats.state().open)))
                    -- Attacks at the head until one hits (the points allow three)
                    local head = nil
                    for _, part in ipairs(vats.parts(vatsTarget)) do
                        if part.target == 'Head' then head = part end
                    end
                    local before = vats.state().actionPoints
                    local made, hit = 0, nil
                    while head and not hit and made < 4 do
                        local result = vats.attack(vatsTarget, head.index)
                        if result.refused then
                            print('OFTEST', 'arena', 'vatsrefused', result.refused)
                            break
                        end
                        made = made + 1
                        if result.hit then hit = result end
                    end
                    local after = vats.state()
                    vatsPoints = after.actionPoints
                    local after_parts = {}
                    for _, part in ipairs(vats.parts(vatsTarget)) do
                        after_parts[part.target] = part.condition
                    end
                    print('OFTEST', 'arena', 'vatsattack', string.format(
                        'made=%d hit=%s spent=%.0f crippled=%s killed=%s head=%.0f torso=%.0f', made,
                        tostring(hit ~= nil), before - after.actionPoints, tostring(hit and hit.crippled),
                        tostring(hit and hit.killed), after_parts['Head'] or -1, after_parts['Torso'] or -1))
                    local again = vats.attack(vatsTarget, head and head.index or 0)
                    print('OFTEST', 'arena', 'vatsdead', tostring(again.refused))
                    vatsStart = core.getSimulationTime()
                elseif vatsStart and core.getSimulationTime() - vatsStart >= 1.5 then
                    -- The action points come back while V.A.T.S. is closed
                    print('OFTEST', 'arena', 'vatsregen', tostring(vats.state().actionPoints > vatsPoints))
                    vatsFrames, vatsStart = nil, nil
                end
            end
        end,
        onUpdate = function()
            if throwing and core.combat.available() then
                -- What the weapon in the hands is before each throw: a grenade is used up by it, then the hands are left
                local weapon = core.combat.weapon()
                local result = core.combat.fire(throwing)
                if result.fired then
                    thrown = thrown + 1
                    print('OFTEST', 'arena', 'throw', thrown, string.format('weapon=%s melee=%s rounds=%d ammoUse=%d hit=%s',
                        weapon.weapon, tostring(weapon.melee), weapon.rounds, weapon.ammoUse, tostring(result.hit)))
                    if thrown >= planned then throwing = nil end
                end
            end
            if target == nil or not core.combat.available() then return end
            local result = core.combat.fire(target)
            if result.fired then
                shots = shots + 1
                print('OFTEST', 'arena', once and 'bystander shot' or 'shot', shots, string.format(
                    'hit=%s damage=%.1f killed=%s', tostring(result.hit), result.damage, tostring(result.killed)))
                if result.killed or once then target = nil end
            elseif result.outOfAmmo then
                print('OFTEST', 'arena', 'out of ammunition')
                target = nil
            end
        end,
    },
}
"""


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


KEY_TAB = 0xFF09
KEY_RETURN = 0xFF0D


def press_keys(keysyms, pause=0.4):
    """Presses the keys on the virtual display (the X test extension), one after the other. False when the libraries
    or the display are not there."""
    try:
        x11 = ctypes.CDLL("libX11.so.6")
        xtst = ctypes.CDLL("libXtst.so.6")
    except OSError:
        return False
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XKeysymToKeycode.restype = ctypes.c_ubyte
    display = ctypes.c_void_p(x11.XOpenDisplay(os.environ.get("DISPLAY", "").encode()))
    if not display.value:
        return False
    for keysym in keysyms:
        code = x11.XKeysymToKeycode(display, ctypes.c_ulong(keysym))
        for down in (1, 0):
            xtst.XTestFakeKeyEvent(display, code, down, 0)
            x11.XFlush(display)
            time.sleep(0.1)
        time.sleep(pause)
    x11.XCloseDisplay(display)
    return True


def press_keys_when_logged(log, pattern, keysyms, delay, stop):
    """Waits for the line of the log and presses the keys `delay` seconds after it (a message box takes the keys when it
    is on the screen, which it is a moment after the line)."""
    while not stop.is_set():
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        if re.search(pattern, text):
            stop.wait(delay)
            if not stop.is_set():
                press_keys(keysyms)
            return
        stop.wait(0.2)


def run_engine(program, resources, data, home, runtime, seconds, start, pixels, scripts="walktest.omwscripts",
               keys=None, new_game=False, load=None, before=(), plugins=True, extra=()):
    """Runs the engine on the start for at most `seconds`. `keys` is (a pattern, the keys, the delay): the keys are
    pressed after the log has a line that matches. `new_game` adds `--new-game`; a `start` of None leaves `--start` out;
    `load` is a saved game to start from (`--load-savegame`); `before` are plugins that come before OFTest.esm in the
    load order; `plugins` False names no plugin at all; `extra` are more arguments."""
    env = dict(os.environ, HOME=str(home), XDG_RUNTIME_DIR=str(runtime), LIBGL_ALWAYS_SOFTWARE="1")
    for name in ("XDG_CONFIG_HOME", "XDG_DATA_HOME"):
        env.pop(name, None)
    command = [str(program), "--resources", str(resources), "--data", str(data),
               *(argument for name in before if plugins for argument in ("--content", name)),
               *(("--content", "OFTest.esm") if plugins else ()),
               "--content", scripts, *extra, "--skip-menu", *(["--new-game"] if new_game else []),
               *([] if start is None else ["--start", start]), *([] if load is None else ["--load-savegame", str(load)]),
               "--no-grab"]
    # The engine shows a dialog instead of logging a fatal error when stdin is not a terminal, so give it one.
    master, slave = pty.openpty()
    process = subprocess.Popen(command, stdin=slave, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    os.close(slave)
    colours = {}
    stop = threading.Event()
    presser = None
    if keys is not None:
        presser = threading.Thread(target=press_keys_when_logged, daemon=True, args=(
            home / ".config" / "openfallout" / "openfallout.log", keys[0], keys[1], keys[2], stop))
        presser.start()
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
    stop.set()
    if presser is not None:
        presser.join()
    return process.returncode, colours


def check(text, scenario, colours, animated=True):
    """The problems the log shows, as a list of sentences; empty when the run did what it should. The player is moved to
    the other kind of cell at the end (see MOVE_FROM), so the checks of the start look at the log up to there."""
    problems = []
    whole = text
    text = text.split(PLAYER_MOVES, 1)[0]
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
        elif mode == ["greener"]:
            if have[1] < have[0] + WATER_GREEN or have[1] < have[2] + WATER_GREEN:
                problems.append(f"the pixel at {position} that should show the {name} is {have}, which should have a "
                                f"green channel at least {WATER_GREEN} above the red and the blue one ({want})")
        elif any(abs(h - w) > PIXEL_TOLERANCE for h, w in zip(have, want)):
            problems.append(f"the pixel at {position} that should show the {name} is {have}, the {name} colour of the "
                            f"cell is {want}")
    water_missing = re.findall(r"Failed to open image: Resource 'textures/water/[^']*' not found", text)
    if water_missing:
        problems.append(f"the log reports textures of the water of Morrowind as missing: {water_missing[:3]} (the "
                        "engine should not look for frames that the game has not got)")
    missing = re.findall(r"Failed to (?:load|open) (?:image|'[^']*sky[^']*')[^\n]*(?:tx_sun|tx_moon|tx_masser|tx_secunda"
                         r"|tx_sky|sky_[a-z_0-9]*\.nif)[^\n]*", text)
    if missing:
        problems.append(f"the log reports files of the sky of Morrowind as missing: {missing[:3]}")
    samples = [(float(m.group(1)), m.group(2), m.group(3), tuple(float(m.group(i)) for i in (4, 5, 6)),
                tuple(float(m.group(i)) for i in (7, 8, 9))) for m in map(SAMPLE.search, text.splitlines())
               if m and float(m.group(1)) <= WALK_SECONDS]
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
    foot = FOOT_RAY[:3] + (FOOT_PLAYED_FACE if animated else FOOT_REST_FACE,)
    for name, (_, _, axis, want) in {**BODY_RAYS, "foot": foot}.items():
        ray = found.get(name)
        if ray is None:
            problems.append(f"the script did not log the ray at what is drawn of the person ('{name}')")
        elif ray.group(2) != "true" or abs(float(ray.group(3 + axis)) - (person[axis] + want)) > 1.5:
            problems.append(f"the ray '{name}' at what is drawn of the person should hit a face at {person[axis] + want:.1f}"
                            f" on its axis: {ray.group(0)}")
    for line in text.splitlines():
        if re.search(r" [EW]\] .*(?:person_|Hair not found|Head part not found)", line):
            problems.append(line)
    problems += check_creatures(text, scenario, found, animated)
    # The person is moved east, at the speed of walking, then running, and then stands: the engine takes the speed from
    # how fast it changes its place, and plays the animation of each, which put the foot in a place of their own. The
    # control run (--no-animation) has no animation files, so the foot stays where the skeleton has it.
    moved = {}
    for match in map(MOVING_FOOT_RAY.search, text.splitlines()):
        if match:
            moved[match.group(1)] = (match.group(2) == "true", float(match.group(3) or 0), float(match.group(4) or 0))
    for name, face in MOVING_FOOT_FACES.items():
        if name not in moved:
            problems.append(f"the script did not log the ray at the foot of the person ('{name}')")
            continue
        hit, hit_x, person_x = moved[name]
        want = person_x + (face if animated else FOOT_REST_FACE)
        if not hit or abs(hit_x - want) > MOVING_FOOT_TOLERANCE:
            problems.append(f"the ray '{name}' at the foot of the moved person should hit the box at x={want:.0f} "
                            f"(the person is at {person_x:.0f}), it found x={hit_x:.0f} (hit: {hit})")
    start_x = scenario.origin[0] + plugin.NPC_DISTANCE
    for name, least in (("footwalk", 60.0), ("footrun", 280.0)):
        if name in moved and moved[name][2] < start_x + least:
            problems.append(f"the person was at x={moved[name][2]:.0f} at '{name}', expected it to have been moved "
                            f"east at least {least:.0f} units from {start_x:.0f}")
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
    problems += check_packages(text, scenario, animated)
    problems += check_quests(text)
    problems += check_objects(whole, scenario)
    if "Quitting peacefully" not in whole:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems


def check_dialogue(text):
    """The problems with the conversation: the greeting for the voice of the talker (and not the one for another voice)
    is said, its begin script has run, the window shows it (the mode Dialogue) and goes on, the options are the topic at
    the top of the list with its prompt and the goodbye, the response to the first adds a topic and its end
    script runs, the added topic ends the conversation and the window closes with it."""
    problems = []

    def lines(kind):
        found = (re.search(rf"OFTEST\tdialogue[\t ]{kind}\t(.*)$", line) for line in text.splitlines())
        return [match.group(1).strip() for match in found if match]

    def one(kind):
        found = lines(kind)
        if len(found) != 1:
            problems.append(f"expected one 'dialogue {kind}' line, found {len(found)}")
            return None
        return found[0]

    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run", line, re.I):
            problems.append(line)
    timeouts = lines("timeout at step")
    if timeouts:
        problems.append("the conversation did not go on: " + "; ".join(timeouts))
    found = one("available")
    if found is not None and found != "true":
        problems.append(f"core.conversation.available() should be true with the topics of the plugin loaded: {found}")
    if "OFTEST\tdialogue talker found" not in text:
        problems.append("the talker was not found by the script of the world")

    def has(line, *parts):
        return line is not None and all(part in line for part in parts)

    voiced = re.findall(r"Voice: (\S+)", text)
    greeting_voice = plugin.voice_path("OFTestVoice", "oftest_oftestgreeting", plugin.GREETING_INFO_ID)
    # the second conversation, which the script ends, says the greeting again
    wanted = [greeting_voice, plugin.SOUND_PATH, plugin.FOLDER_SOUND_PATH, greeting_voice]
    if voiced != wanted:
        problems.append("the lines that have a voice file should look it up in the folder of the voice type of the "
                        "talker, a line whose only file is another voice's (the ask line) and the lines with none "
                        "should be silent, and a response that names a sound record (the added topic) should be said "
                        "with the file of the record, or with the first file of the folder that the record names: "
                        f"{voiced}, wanted {wanted}")

    greeting = one("greeting")
    if not has(greeting, "active=true speaking=true ended=false", f"speaker={plugin.TALKER_TITLE}",
               f"lines=[{plugin.GREETING_TEXT}]", "options=[]", "mode=Dialogue"):
        problems.append("the conversation should open with the greeting for the voice and the sex of the talker (and "
                        f"not the one for another voice), in the mode Dialogue: {greeting}")
    options = one("options")
    if not has(options, "active=true speaking=false", f"lines=[{plugin.GREETING_TEXT}]",
               f"options=[{plugin.ASK_PROMPT}|Goodbye.]", "mode=Dialogue"):
        problems.append("once the greeting was read the options should be the topic at the top of the list (with its "
                        f"prompt, first for its priority) and the goodbye: {options}")
    asked = one("asked")
    if not has(asked, "speaking=true", f"lines=[{plugin.ASK_TEXT}]", "options=[]"):
        problems.append(f"picking the topic should say its response: {asked}")
    after = one("after ask")
    if not has(after, "speaking=false", f"lines=[{plugin.ASK_TEXT}]",
               f"options=[{plugin.ASK_PROMPT}|Goodbye.|{plugin.ADDED_TOPIC_NAME}]"):
        problems.append("after the response the topic that it adds should be among the options, after the topics of "
                        f"the list: {after}")
    added = one("added")
    if not has(added, "speaking=true", f"lines=[{plugin.ADDED_TEXT}|{plugin.ADDED_MORE_TEXT}]"):
        problems.append(f"picking the added topic should say its response: {added}")
    over = one("over")
    if not has(over, "active=false", "ended=true"):
        problems.append(f"a response flagged as the goodbye should end the conversation: {over}")
    closed = one("closed")
    if not has(closed, "active=false", "mode=nil"):
        problems.append(f"the window should be gone when the conversation is over: {closed}")
    dropped = one("dropped")
    if not has(dropped, "active=false", "mode=nil"):
        problems.append("a conversation that a script ends with core.conversation.close should take the window with it, "
                        f"and not leave the game paused: {dropped}")
    # The message box with buttons that the response ending the conversation shows: the third button is picked (the
    # second has a condition the player does not meet), the host reports the number it has in the record, and the script of
    # the talker reads it once (the number plus one)
    shown = re.findall(r"Message: " + re.escape(plugin.BUTTON_MESSAGE_TEXT) + r"\s*$", text, re.M)
    if len(shown) != 1:
        problems.append(f"the response that ends the conversation should show the message with buttons once: {len(shown)}")
    buttons = re.findall(r"Message buttons: (\d+)", text)
    if buttons != [str(len(plugin.BUTTON_TEXTS) - 1)]:
        problems.append(f"the button whose condition fails should not be shown, so two of three: {buttons}")
    picked = re.findall(r"Message button: (-?\d+)", text)
    if picked != [str(plugin.BUTTON_PICKED)]:
        problems.append(f"the third button should be picked with Tab and Return, as number {plugin.BUTTON_PICKED} of the "
                        f"record: {picked}")
    if "OFTEST\tdialogue\tbutton timeout" in text:
        problems.append("no button was picked in the message box (the keys did not reach it, or the script did not read it)")
    else:
        read = lines("button")
        if read != [str(plugin.BUTTON_PICKED + 1)]:
            problems.append(f"the script of the talker should read the button once, as {plugin.BUTTON_PICKED + 1}: {read}")

    # The end script of the question made the talker say a topic outside of the conversation (Say), and when it had said
    # it the Begin SayToDone block of the script of the talker ran: once
    said = lines("said")
    if not said or said[-1] != "1" or any(value not in ("0", "1") for value in said):
        problems.append("the talker was made to say a topic once, so the Begin SayToDone block of its script should have "
                        f"run once by the time the button was read: {said}")

    # The topic whose only response could not be said was told too, and the game tells the script of the speaker that it is
    # done (Begin SayToDone for that topic) in the frame after: once, with the line about it in the log
    empty = lines("empty")
    if not empty or empty[-1] != "1" or any(value not in ("0", "1") for value in empty):
        problems.append("the talker was made to say a topic that nobody can say, so the Begin SayToDone block for it should "
                        f"have run once by the time the button was read: {empty}")
    if not re.search(r"Say: .* is to say topic OFTestEmpty .*: nothing it can say", text):
        problems.append("the log should say that the talker was to say OFTestEmpty and had nothing it could say")

    # The end script of the response that was said had run by then (it sets a global that the block reads): once
    ordered = lines("ordered")
    if not ordered or ordered[-1] != "1" or any(value not in ("0", "1") for value in ordered):
        problems.append("the end script of a response that is said should have run, when its line was over, before the "
                        f"Begin SayToDone block of the talker did: {ordered}")

    # The blocks name what they are for: the one for a topic that was not said and the one for a menu that never opened
    # did not run, and the ones for the conversation and for the message box ran while those were open
    blocks = [line.split("\t") for line in lines("blocks")]
    if not blocks or any(len(values) != 4 for values in blocks):
        problems.append(f"expected 'dialogue blocks' lines with four values: {blocks}")
    else:
        if any(values[0] != "0" for values in blocks):
            problems.append("a Begin SayToDone block for a topic that was not said should not run after another topic "
                            f"was said: {blocks}")
        if any(values[3] != "0" for values in blocks):
            problems.append("a Begin MenuMode block for a menu that never opened (1074) should not run while the "
                            f"conversation or the message box is open: {blocks}")
        if blocks[-1][1] in ("", "0", "nil"):
            problems.append(f"a Begin MenuMode 1009 block should run while the conversation is open: {blocks[-1]}")
        if blocks[-1][2] in ("", "0", "nil"):
            problems.append(f"a Begin MenuMode 1001 block should run while the message box is open: {blocks[-1]}")

    globals_ = {line.split("\t")[0]: line.split("\t")[1:] for line in lines("globals")}
    for tag, want in (("greeting", ["1", "0"]), ("options", ["1", "0"]), ("asked", ["1", "1"]), ("closed", ["1", "1"])):
        if globals_.get(tag) != want:
            problems.append(f"after '{tag}' the globals set by the begin script (greeting) and the end script (asked "
                            f"topic) should be {want}: {globals_.get(tag)}")
    return problems


def check_chargen(text):
    """The problems with the making of the character: the screens open one after the other in the mode of the interface,
    the name is typed on the keyboard, the picks follow the rules (the points of SPECIAL, three tags, the traits that are
    traits), and what the script of the quest reads afterwards is what was made."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run", line, re.I):
            problems.append(line)

    def lines(kind):
        found = (re.search(rf"OFTEST\tchargen[\t ]{kind}\t(.*)$", line) for line in text.splitlines())
        return [match.group(1).strip() for match in found if match]

    def one(kind):
        found = lines(kind)
        if len(found) != 1:
            problems.append(f"expected one 'chargen {kind}' line, found {len(found)}")
            return None
        return found[0]

    def has(line, *parts):
        return line is not None and all(part in line for part in parts)

    timeouts = [line for line in lines("timeout at step") + lines("timeout waiting for the script of the quest")]
    if timeouts or "OFTEST\tchargen\ttimeout" in text:
        problems.append("the making of the character did not go on: " + "; ".join(timeouts or ["timeout"]))
    traits = (f"{plugin.TRAIT_A_NAME}|{plugin.TRAIT_B_NAME}", f"{plugin.TRAIT_B_NAME}|{plugin.TRAIT_A_NAME}")

    race = one("screen race")
    if not has(race, "name= ", "female=false", "points=5", "attributes=[5,5,5,5,5,5,5]", "tags=[]", "races=2",
               "skills=13", "mode=Interface") or not any(has(race, f"traits=[{pair}]") for pair in traits):
        problems.append("the first screen should be the sex and race, in the mode Interface, with SPECIAL at 5 each, "
                        "five points to give, 13 skills, two races, and the two traits (not the perk that is none): "
                        f"{race}")
    picked = one("race picked")
    if picked != "true\ttrue" and picked != "true true":
        problems.append(f"the race and the sex picked should be what the state tells: {picked}")
    name = one("screen name")
    if not has(name, "name= ", "female=true", "mode=Interface"):
        problems.append(f"the screen of the name comes after the race (GetPlayerName), with no name yet: {name}")
    special = one("screen special")
    if not has(special, "name=ada ", "points=5", "attributes=[5,5,5,5,5,5,5]", "mode=Interface"):
        problems.append("the screen of SPECIAL should come after the name typed on the keyboard (ada) and Return, and "
                        f"start from the attributes of the player: {special}")
    adjusted = one("adjust")
    if adjusted is None or adjusted.split() != ["true", "true", "true", "false", "false"]:
        problems.append("three points to the strength, two taken from the luck and four to the agility fit (5 to give "
                        "and 2 taken), a point more does not, and the luck can not go below 1 (then 3 - 9): "
                        f"{adjusted}")
    done = one("special done")
    if not has(done, "points=0", "attributes=[8,5,5,5,5,9,3]"):
        problems.append(f"SPECIAL should come to 8, 5, 5, 5, 5, 9 and 3: {done}")
    tagged = one("tag")
    if tagged is None or tagged.split() != ["true", "true", "true", "false", "false"]:
        problems.append("three skills can be tagged (Barter, Guns, Survival) but not a fourth, and Big Guns is no "
                        f"skill of New Vegas: {tagged}")
    skills = one("skills done")
    if not has(skills, "tags=[32,41,44]"):
        problems.append(f"the tags should be Barter, Guns and Survival: {skills}")
    traited = one("traits done")
    if not has(traited, "tags=[32,41,44]") or traited.count("*") != 2:
        problems.append(f"both traits should be picked: {traited}")
    closed = one("closed")
    if not has(closed, "mode=nil"):
        problems.append(f"the interface should be left when the last screen is done: {closed}")
    attributes = [line.split("\t") for line in lines("attribute")]
    if len(attributes) != 7 or [row[0] for row in attributes] != [str(n) for n in range(5, 12)] \
            or attributes[0][1:] != ["Strength of the test", "The test strength description."] \
            or attributes[1][1] != "Perception":
        problems.append("the attributes should have the names and texts that the actor value records give (and the "
                        f"name of the game where there is no record): {attributes}")

    globals_ = {}
    found = one("globals")
    if found is not None:
        globals_ = dict(part.split("=", 1) for part in found.split() if "=" in part)
    # Barter is governed by the charisma (5), Guns by the agility (9) and Survival by the endurance (5); the luck is 3, so
    # a skill is 2 + twice its attribute + 1, and 15 more for the three that are tagged
    wanted = {"go": "1", "state": "2", "during": "1", "after": "0", "female": "1", "race_two": "1", "strength": "8",
              "perception": "5", "endurance": "5", "charisma": "5", "intelligence": "5", "agility": "9", "luck": "3",
              "barter": "28", "guns": "36", "survival": "28", "trait_a": "1", "trait_b": "1", "other": "0"}
    for key, value in wanted.items():
        if globals_.get(key) != value:
            problems.append(f"the script of the quest should read {key} as {value}: {globals_.get(key)}")
    return problems


def check_triggers(text):
    """The problems with the trigger volume: the player is not in the box at first; the box, turned a quarter, takes a
    point along y and not one along x; coming in runs OnTriggerEnter for the player and for anyone once, and the person
    that the script makes at the middle of the box (an actor that did not come with the cell) comes in a pass later,
    which runs it for anyone once more; staying runs OnTrigger, going out runs OnTriggerLeave, and coming in again runs
    the enters again (and for the second person that is made)."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error", line, re.I):
            problems.append(line)
    found = {}
    for line in text.splitlines():
        match = re.search(r"OFTEST\ttrigger\t(\w+)\tenters=(\d+) any=(\d+) stays=(\d+) leaves=(\d+)$", line)
        if match:
            found[match.group(1)] = tuple(int(group) for group in match.groups()[1:])
    for tag in ("before", "inside", "outside", "again"):
        if tag not in found:
            problems.append(f"no 'trigger {tag}' line in the log")
    if problems:
        return problems
    if found["before"] != (0, 0, 0, 0):
        problems.append(f"nothing should have run while the player is far from the box: {found['before']}")
    enters, any_enters, stays, leaves = found["inside"]
    if (enters, any_enters, leaves) != (1, 2, 0) or stays < 1:
        problems.append("a player that came into the box (a point along its long side, which is the turned y) should have "
                        "run OnTriggerEnter once for the player and twice for anyone (the player, and the person that "
                        f"the script made at the middle of the box), then OnTrigger: {found['inside']}")
    enters, any_enters, stays_out, leaves = found["outside"]
    if (enters, any_enters, leaves) != (1, 2, 1) or stays_out < stays:
        problems.append("a player 100 along x is outside the turned box, which should have run OnTriggerLeave once and no "
                        f"enter: {found['outside']}")
    enters, any_enters, _, leaves = found["again"]
    if (enters, any_enters, leaves) != (2, 4, 1):
        problems.append("coming in again should have run the enters a second time (for anyone twice more: the player and "
                        f"the second person that is made, while the first stays) and no leave: {found['again']}")
    # The chair: not used before the player activates it; used once the player has (and put at it); not used any more once
    # the player is moved away from where it sat
    sat = {}
    for line in text.splitlines():
        match = re.search(r"OFTEST\tfurniture\t(\w+)\t(-?\d+)\t(.+)$", line)
        if match:
            sat[match.group(1)] = (int(match.group(2)), match.group(3))
    for tag in ("before", "sat", "away"):
        if tag not in sat:
            problems.append(f"no 'furniture {tag}' line in the log")
    if len(sat) == 3:
        if sat["before"][0] != 0:
            problems.append(f"IsCurrentFurnitureRef should be 0 for a player who has not used the chair: {sat['before']}")
        if sat["sat"][0] != 1:
            problems.append("a player who activated the chair should be using it (IsCurrentFurnitureRef 1 in the script of "
                            f"the trigger): {sat['sat']}")
        where = sat["sat"][1].split("\t")
        if len(where) != 4:
            problems.append(f"the player who sat down should be put at the chair: {sat['sat']}")
        else:
            across, along, up, yaw = (float(value) for value in where)
            # On the entry of the marker of the model (the chair is not turned): beside the chair, facing as the entry says
            (entry_x, entry_y, _), orientation, _ = plugin.FURNITURE_ENTRY
            if abs(across - entry_x) > 5 or abs(along - entry_y) > 5 or abs(up) > 5:
                problems.append("the player who sat down should be put on the entry of the marker of the chair "
                                f"({entry_x:.0f}, {entry_y:.0f}) from it, not at the middle of it: {sat['sat']}")
            if abs(yaw - orientation / 1000.0) > 0.3:
                problems.append(f"the player should face as the entry of the chair says ({orientation / 1000.0}): "
                                f"{sat['sat']}")
        if sat["away"][0] != 0:
            problems.append("a player who was moved away from the chair should not use it any more (IsCurrentFurnitureRef "
                            f"0): {sat['away']}")
    if "Furniture: " not in text:
        problems.append("the engine did not log that the player used the furniture")
    # A chair whose only entry is taken is not used by a second one: the player stays where it was
    taken = dict(re.findall(r"OFTEST\toccupied\t(\w+)\t(\d+|no chair)$", text, re.M))
    if set(taken) != {"before", "after"}:
        problems.append(f"expected 'occupied before' and 'occupied after' lines: {taken}")
    elif "no chair" in taken.values() or abs(int(taken["after"]) - int(taken["before"])) > 5:
        problems.append("a player who activates a chair whose only entry is in use should not be put there: "
                        f"{taken}")
    if "cannot use" not in text:
        problems.append("the engine did not log that the chair with its only entry in use was refused")
    # The sitter travelled to its chair and sat there (the script sees it), and stayed though its second package, which is
    # not done, came after it
    later = re.findall(r"OFTEST\tsitter\tlater\t(-?\d+)\t(\d+|no sitter)$", text, re.M)
    if len(later) != 1:
        problems.append(f"expected one 'sitter later' line: {later}")
    else:
        if later[0][0] != "1":
            problems.append(f"a character that has got to a chair by its package should use it: {later[0]}")
        # It stands on the entry of the marker of the chair, which is that far from the middle of the chair
        entry_distance = math.hypot(*plugin.FURNITURE_ENTRY[0][:2])
        if later[0][1] == "no sitter" or abs(int(later[0][1]) - entry_distance) > 15:
            problems.append("a character in a chair should stay there, on the entry of its marker, while its package for "
                            f"it is on, not walk on to the package that comes after: {later[0]}")
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems


def check_savesprobe(first, second):
    """The problems with the probe of the saves. `first` is the log of the engine that saved the game when quest A had
    stood at its stage, loaded it back when quest C had (once), and saved again after the load; `second` the log of the engine
    started on the first save. Both must find the quests as the save had them, say so ("check ok"), save again, and not load."""
    problems = []
    for name, text in (("probe", first), ("fresh", second)):
        for line in text.splitlines():
            if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error|Error (on )?load|Failed to load|"
                         r"Can't load|damaged|newer than this engine", line, re.I):
                problems.append(f"({name}) {line}")

    def lines(text, *words):
        pattern = r"AUTOPLAY\t" + r"\t".join(re.escape(word) for word in words) + r"(?:\t(.*))?$"
        return [match.group(1) or "" for match in (re.search(pattern, line) for line in text.splitlines()) if match]

    saved = lines(first, "digest", "saved")
    if len(saved) != 1:
        problems.append(f"the probe should save the game once, with the quests as they are: {saved}")
        return problems
    if not lines(first, "save", "autoplay-save", "ok"):
        problems.append("the menu script did not save the game")
    if len(lines(first, "load", "autoplay-save")) != 1:
        problems.append(f"the game should be loaded back once: {lines(first, 'load', 'autoplay-save')}")
    for name, text in (("probe", first), ("fresh", second)):
        if not lines(text, "loaded", "phase=saved"):
            problems.append(f"({name}) the world script did not find the phase of the save in the game that was loaded")
        if lines(text, "digest", "loaded") != saved:
            problems.append(f"({name}) the quests after the load should be those of the save: {lines(text, 'digest', 'loaded')} "
                            f"instead of {saved}")
        if len(lines(text, "digest", "check ok")) != 1 or lines(text, "digest", "check MISMATCH"):
            problems.append(f"({name}) the check of the quests after the load should be ok exactly once")
        if not lines(text, "save", "autoplay-again", "ok"):
            problems.append(f"({name}) the game that was loaded should be saved again")
    if lines(second, "load"):
        problems.append(f"the engine started on the save should not load a game: {lines(second, 'load')}")
    return problems


def check_mods(text):
    """The problems with the load order. The engine was told to load the mod before its master; the log must say that the
    master moved, the content list must have the master first, the global the mod changes must have the value of the mod
    and the global it adds must be there."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error|required dependency|"
                     r"cannot be loaded", line, re.I):
            problems.append(line)
    if not re.search(r"Load order: OFTest\.esm moves from place 2 to place 1 of 2: it is flagged as a master", text):
        problems.append("the log does not say that the master moved before the mod")
    order = re.search(r"OFTEST\tmods order\t(\S+)", text)
    if not order:
        problems.append("the script did not log the content list")
    else:
        files = [name.lower() for name in order.group(1).split(",")]
        if "oftest.esm" not in files or "oftestmod.esp" not in files or files.index("oftest.esm") > files.index("oftestmod.esp"):
            problems.append(f"the master must load before the mod: {order.group(1)}")
        elif files.index("oftestmod.esp") != files.index("oftest.esm") + 1:
            problems.append(f"the mod must load right after its master: {order.group(1)}")
    problems += check_mod_values(text)
    problems += check_mod_records(text)
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems


def check_mod_records(text):
    """The problems with the records of the mod. It overrides the interior cell (with another name, which Lua does not show:
    a cell is named by its editor ID) and, in it, disables one cube, enables one that the master disables, moves one up, deletes two (one of the records has no
    subrecords), moves one to the exterior cell, leaves one alone and adds one of its own."""
    problems = []
    place = plugin.mod_place
    moved = (int(place("move")[0]), int(place("move")[1]), int(plugin.MOD_MOVED_BY))
    wanted = {
        "disable": ("interior", "false", place("disable")),
        "enable": ("interior", "true", place("enable")),
        "move": ("interior", "true", moved),
        "delete": ("none", None, None),
        "bare": ("none", None, None),
        "elsewhere": ("exterior", "true", plugin.MOD_ELSEWHERE_PLACE),
        "keep": ("interior", "true", place("keep")),
        "new": ("interior", "true", plugin.MOD_NEW_PLACE),
    }
    for key, (where, enabled, position) in wanted.items():
        found = re.search(rf"OFTEST\tmods ref\t{key}\t(\S+)(?:\t(\S+)\t(\S+))?", text)
        if not found:
            problems.append(f"the script did not log the cube '{key}'")
            continue
        if found.group(1) != where:
            problems.append(f"cube '{key}': it is listed in {found.group(1)}, it must be in {where}")
            continue
        if enabled is not None and found.group(2) != enabled:
            problems.append(f"cube '{key}': enabled is {found.group(2)}, it must be {enabled}")
        if position is not None:
            expected = ",".join(str(int(value)) for value in position)
            if found.group(3) != expected:
                problems.append(f"cube '{key}': it stands at {found.group(3)}, it must be at {expected}")
    return problems


def check_mod_values(text):
    """The problems with the two globals of the mod: the one it changes has the value of the mod, the one it adds is there."""
    problems = []
    base = re.search(r"OFTEST\tmods base\t(\S+)\t(\S+)", text)
    if not base:
        problems.append("the script did not log the globals")
    else:
        if base.group(1) == "nil" or float(base.group(1)) != plugin.MOD_OVERRIDDEN_VALUE:
            problems.append(f"the mod changes {plugin.MOD_BASE_GLOBAL_NAME} to {plugin.MOD_OVERRIDDEN_VALUE}, the global is "
                            f"{base.group(1)} (the master has {plugin.MOD_BASE_VALUE})")
        if base.group(2) == "nil" or float(base.group(2)) != plugin.MOD_ADDED_VALUE:
            problems.append(f"the mod adds {plugin.MOD_ADDED_GLOBAL_NAME} with the value {plugin.MOD_ADDED_VALUE}, the global "
                            f"is {base.group(2)}")
    return problems


def check_activation(text):
    """The problems with the plugins that the games' rules make active. No plugin was named: the ini file names
    OFTest.esm (sTestFile1) and a file OFTestMod.NAM sits next to the mod, so both must load, the master first, and the
    mod must change and add its globals."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error|required dependency|"
                     r"cannot be loaded|No content file", line, re.I):
            problems.append(line)
    if not re.search(r"the plugins the games make active load: OFTest\.esm, OFTestMod\.esp", text):
        problems.append("the log does not say which plugins the games' rules made active")
    order = re.search(r"OFTEST\tmods order\t(\S+)", text)
    if not order:
        problems.append("the script did not log the content list")
    else:
        files = [name.lower() for name in order.group(1).split(",")]
        if files.count("oftest.esm") != 1 or files.count("oftestmod.esp") != 1:
            problems.append(f"both plugins must load once: {order.group(1)}")
        elif files.index("oftest.esm") > files.index("oftestmod.esp"):
            problems.append(f"the master must load before the mod: {order.group(1)}")
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems + check_mod_values(text) + check_mod_records(text)


def check_archives(text, loose_wins):
    """The problems with the archives. The archive OFTestArchive.bsa was named on the command line, OFTestMod - Main.bsa
    is found because OFTestMod.esp has a master. A file that is loose and in the archive comes from the loose one when
    the ini file says bInvalidateOlderFiles=1 (the default) and from the archive when it says 0; with 1,
    ArchiveInvalidation.txt hides oftest/hidden.txt of the archive, with 0 it is not read."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error|Archive '.*' not found", line,
                     re.I):
            problems.append(line)
    if not re.search(r"Archive order \(the first one that has a file wins\): .*OFTestMod - Main\.bsa", text):
        problems.append("the log does not list the archive that is named after the mod")
    if not re.search(r"Archive order .*OFTestArchive\.bsa", text):
        problems.append("the log does not list the archive that was named on the command line")
    wanted = {"shared": "loose" if loose_wins else "archive", "hidden": "missing" if loose_wins else "archive",
              "only": "archive", "mod": "modarchive"}
    for name, value in wanted.items():
        found = re.search(rf"OFTEST\tarchives\t{name}\t(\S+)", text)
        if not found:
            problems.append(f"the script did not log {name}")
        elif found.group(1) != value:
            problems.append(f"oftest/{name}: the file is {found.group(1)}, it must be {value}")
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems


def check_saves(first, second, third=None):
    """The problems with the saves. `first` is the log of the engine that saved, changed the state and loaded the save
    again, `second` the log of the engine that was started on the save, `third` (when there is one) the log of an engine
    that was started on it with another plugin before OFTest.esm, so that every form ID of the save has another top byte now
    and the save has to be read through the map of plugins. The state that the script reads (the stages of
    the quests, whether a stage is done, the objective of quest B, the global) before the save is what it reads after the
    load, in both; what was changed after the save (quest C moved to a stage, the global) is gone; and three seconds
    later it is the same still: the quests that ran before the save did not run again (the global did not get the seven
    that stage 10 of quest A adds)."""
    problems = []
    loads = [("fresh", second)] + ([("shifted", third)] if third is not None else [])
    for name, text in (("saving", first), *loads):
        for line in text.splitlines():
            if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error|Error (on )?load|Failed to load|"
                         r"Can't load|damaged|newer than this engine", line, re.I):
                problems.append(f"({name}) {line}")

    def states(text):
        found = {}
        for line in text.splitlines():
            match = re.search(r"OFTEST\tsaves\t(before|changed|loaded|later)\t(.+)$", line)
            if match:
                found[match.group(1)] = dict(item.split("=", 1) for item in match.group(2).split(" "))
        return found

    first_states = states(first)
    started = [("the engine started on the save", second)]
    if third is not None:
        started.append(("the engine started on the save with another plugin before it", third))
    started_states = [(name, states(text)) for name, text in started]
    for tag in ("before", "changed", "loaded", "later"):
        if tag not in first_states:
            problems.append(f"no 'saves {tag}' line in the log of the engine that saved")
    for name, found in started_states:
        for tag in ("loaded", "later"):
            if tag not in found:
                problems.append(f"no 'saves {tag}' line in the log of {name}")
    if "OFTEST\tsaves\tsaved\t" not in first:
        problems.append("the menu script did not save the game")
    if "OFTEST\tsaves\tloading\t" not in first:
        problems.append("the menu script did not find the save to load")
    if problems:
        return problems
    before = first_states["before"]
    wanted = {"a_stage": str(plugin.QUEST_A_STAGE), "a_done": "true", "b_stage": str(plugin.QUEST_B_STAGE),
              "b_completed": "true", "b_shown": "true", "b_finished": "true", "c_stage": "0", "c_done": "false"}
    for key, value in wanted.items():
        if before[key] != value:
            problems.append(f"before the save {key} should be {value} (the quests ran): {before}")
    if float(before["counter"]) != SAVES_COUNTER:
        problems.append(f"the script set the global to {SAVES_COUNTER}: {before}")
    changed = first_states["changed"]
    if (changed["c_stage"], changed["c_done"]) != (str(plugin.QUEST_C_STAGE), "true") \
            or float(changed["counter"]) != SAVES_CHANGED_COUNTER:
        problems.append(f"the changes after the save did not take: {changed}")
    for name, found in (("the engine that saved", first_states), *started_states):
        for tag in ("loaded", "later"):
            state = found[tag]
            if {key: state[key] for key in wanted} != wanted or float(state["counter"]) != SAVES_COUNTER:
                problems.append(f"{name}: after the load ({tag}) the state should be the one of the save "
                                f"({wanted}, counter {SAVES_COUNTER}): {state}")
    # The cubes: what happened to them before the save is what they are after the load, and not what happened after
    ids = plugin.SAVED_REF_IDS
    origin = {name: (plugin.SAVED_PLACE[0] + 300.0 * index, plugin.SAVED_PLACE[1], 128.0)
              for index, name in enumerate(ids)}

    def cube(place, enabled="true", scale=1.0, count=1):
        return f"{place[0]:.0f},{place[1]:.0f},{place[2]:.0f},{enabled},{scale:.1f},{count}"

    left = plugin.SAVED_STACK - plugin.SAVED_TAKEN
    saved_cubes = {"moved": cube(plugin.SAVED_MOVED_TO), "hidden": cube(origin["hidden"], "false"),
                   "grown": cube(origin["grown"], scale=plugin.SAVED_SCALE), "gone": "gone",
                   "doomed": cube(origin["doomed"]), "stacked": cube(origin["stacked"], count=left), "made": "1"}
    changed_cubes = {"moved": cube(origin["moved"]), "hidden": cube(origin["hidden"]),
                     "grown": cube(origin["grown"]), "gone": "gone", "doomed": "gone",
                     "stacked": cube(origin["stacked"], count=left - plugin.SAVED_TAKEN_AFTER), "made": "0"}

    def cubes(text):
        found = {}
        for line in text.splitlines():
            match = re.search(r"OFTEST\tsaves refs\t(before|changed|loaded|later)\t(.+)$", line)
            if match:
                found[match.group(1)] = dict(item.split("=", 1) for item in match.group(2).split(" "))
        return found

    first_cubes = cubes(first)
    started_cubes = [(name, cubes(text)) for name, text in started]
    for tag, expected in (("before", saved_cubes), ("changed", changed_cubes)):
        if first_cubes.get(tag) != expected:
            problems.append(f"the cubes ({tag}) should be {expected}: {first_cubes.get(tag)}")
    for name, found in (("the engine that saved", first_cubes), *started_cubes):
        for tag in ("loaded", "later"):
            if found.get(tag) != saved_cubes:
                problems.append(f"{name}: after the load ({tag}) the cubes should be the ones of the save "
                                f"{saved_cubes}: {found.get(tag)}")
    # The player and the person: what the quest gave before the save (rounds, luck, karma, a death) is there after the load,
    # and not what it did after it
    def characters(text):
        found = {}
        for line in text.splitlines():
            match = re.search(r"OFTEST\tsaves characters\t(before|changed|loaded|later)\t(.+)$", line)
            if match:
                found[match.group(1)] = dict(item.split("=", 1) for item in match.group(2).split(" "))
        return found

    first_characters = characters(first)
    started_characters = [(name, characters(text)) for name, text in started]
    saved_characters = first_characters.get("before")
    changed_characters = first_characters.get("changed")
    if saved_characters is None or changed_characters is None:
        problems.append(f"no 'saves characters' lines before and after the save: {first_characters}")
    else:
        # The rounds and the death are told as they are; luck and karma start where the player's record puts them, so
        # they are told by what the quest added
        if (saved_characters["items"], saved_characters["dead"]) != (str(plugin.SAVED_ITEMS), "1") \
                or (changed_characters["items"], changed_characters["dead"]) != ("0", "0"):
            problems.append(f"the quest did not give and take back the rounds and the death: {first_characters}")
        for key, taken in (("luck", plugin.SAVED_LUCK_AFTER), ("karma", plugin.SAVED_KARMA_AFTER)):
            if int(changed_characters[key]) - int(saved_characters[key]) != taken:
                problems.append(f"{key} should be {taken} higher after the save: {first_characters}")
        # The script of an object of the cell rewards KARMA_REWARD when the cell is loaded, which a load does again: the karma
        # of the save plus that (not the karma of the game before the load, which is more)
        wanted_after_load = dict(saved_characters, karma=str(int(saved_characters["karma"]) + plugin.KARMA_REWARD))
        for name, found in (("the engine that saved", first_characters), *started_characters):
            for tag in ("loaded", "later"):
                if found.get(tag) != wanted_after_load:
                    problems.append(f"{name}: after the load ({tag}) the characters should be the ones of the save "
                                    f"{wanted_after_load}: {found.get(tag)}")
    # The local script of the person keeps what it counted (the events it was sent before the save, and not those after), and
    # the cube that was made is known to Lua by the same number after the load
    def tagged(text, kind, pattern):
        found = {}
        for line in text.splitlines():
            match = re.search(rf"OFTEST\tsaves {kind}\t(before|changed|loaded|later)\t{pattern}$", line)
            if match:
                found[match.group(1)] = match.group(2)
        return found
    first_counts = tagged(first, "local", r"count=(-?\d+)")
    first_made = tagged(first, "made", r"(\S+)")
    if first_counts.get("before") != str(plugin.SAVED_BUMPS) \
            or first_counts.get("changed") != str(plugin.SAVED_BUMPS + plugin.SAVED_BUMPS_AFTER):
        problems.append(f"the person should have counted {plugin.SAVED_BUMPS} events before the save and "
                        f"{plugin.SAVED_BUMPS + plugin.SAVED_BUMPS_AFTER} after it: {first_counts}")
    made_id = first_made.get("before")
    if made_id in (None, "none"):
        problems.append(f"no cube was made before the save: {first_made}")
    for name, text in (("the engine that saved", first), *started):
        counts = tagged(text, "local", r"count=(-?\d+)")
        made = tagged(text, "made", r"(\S+)")
        for tag in ("loaded", "later"):
            if counts.get(tag) != str(plugin.SAVED_BUMPS):
                problems.append(f"{name}: after the load ({tag}) the local script of the person should have counted "
                                f"{plugin.SAVED_BUMPS} events (the ones of the save): {counts.get(tag)}")
            if made.get(tag) != made_id:
                problems.append(f"{name}: after the load ({tag}) the cube that was made should be {made_id} to Lua: "
                                f"{made.get(tag)}")
    if "Quitting peacefully" not in first:
        problems.append("the log of the engine that saved does not end with 'Quitting peacefully'")
    for name, text in started:
        if "Quitting peacefully" not in text:
            problems.append(f"the log of {name} does not end with 'Quitting peacefully'")
    return problems


def check_arena(text):
    """The problems with the arena: the stages of the quest are set; the rifle is in the hands of the player with its rounds;
    the dummy takes the blows the player shoots at it (its OnHit block counts each, a round is used for each shot) and
    dies of them (OnDeath once, GetDead); the raider, put in front of the player, starts a fight (its OnStartCombat block
    runs, IsInCombat holds), takes its pistol from what it carries and shoots (it has fewer rounds), comes within the range
    of the pistol, and the player loses health. A bystander who never starts a fight is shot once and goes for the player.
    The player, given two grenades, uses one up with each throw and throws the third time with its hands."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error", line, re.I):
            problems.append(line)
    pattern = (r"OFTEST\tarena\t(\w[\w ]*)\tarmed=(-?[\d.]+) out=(-?[\d.]+) ammo=(-?[\d.]+) health=(-?[\d.]+) "
               r"dummyDead=(-?[\d.]+) raiderDead=(-?[\d.]+) raiderCombat=(-?[\d.]+) raiderAmmo=(-?[\d.]+) "
               r"hits=(-?[\d.]+) deaths=(-?[\d.]+) started=(-?[\d.]+) raiderDistance=(\w+)$")
    reports = {}
    names = ("armed", "out", "ammo", "health", "dummy_dead", "raider_dead", "raider_combat", "raider_ammo", "hits", "deaths",
             "started")
    for line in text.splitlines():
        match = re.search(pattern, line)
        if match:
            row = dict(zip(names, (float(value) for value in match.groups()[1:12])))
            row["distance"] = None if match.group(13) == "none" else float(match.group(13))
            reports[match.group(1)] = row
    for tag in ("armed", "shot", "raider 6", "raider 20"):
        if tag not in reports:
            problems.append(f"no 'arena {tag}' line in the log")
    if problems:
        return problems
    stages = re.findall(r"OFTEST\tarena\tstage\t(\d+)\t(\w+)$", text, re.M)
    if any(done != "true" for _, done in stages):
        problems.append(f"a stage of the arena quest was not set: {stages}")
    shots = [(int(number), hit, killed) for number, hit, killed in re.findall(
        r"OFTEST\tarena\tshot\t(\d+)\thit=(\w+) damage=[\d.]+ killed=(\w+)$", text, re.M)]
    armed, shot = reports["armed"], reports["shot"]
    if armed["armed"] != 1:
        problems.append(f"the rifle should be equipped by EquipItem (GetEquipped 1): {armed}")
    if armed["ammo"] != plugin.ARENA_PLAYER_AMMO:
        problems.append(f"the player should have its {plugin.ARENA_PLAYER_AMMO} rounds: {armed}")
    if armed["raider_combat"] != 0 or armed["started"] != 0 or armed["distance"] is None or armed["distance"] < 2500:
        problems.append(f"the raider, beyond the sight of a character, should be at peace and far away: {armed}")
    if not shots:
        problems.append("the player should have shot at the dummy (no 'arena shot' line)")
    elif shot["ammo"] != plugin.ARENA_PLAYER_AMMO - len(shots):
        problems.append(f"each shot should use a round: {len(shots)} shots left {shot['ammo']} of "
                        f"{plugin.ARENA_PLAYER_AMMO} rounds")
    if shots and any(hit != "true" for _, hit, _ in shots):
        problems.append(f"every shot at the dummy should hit: {shots}")
    if not shots or shots[-1][2] != "true":
        problems.append(f"the last shot should have killed the dummy: {shots}")
    if shot["dummy_dead"] != 1 or shot["deaths"] != 1:
        problems.append(f"the dummy should be dead (GetDead) and its OnDeath block should have run once: {shot}")
    if shot["hits"] != len(shots):
        problems.append(f"the OnHit block of the dummy should have run once for each of the {len(shots)} blows: {shot}")
    # The raider: it notices the player, closes in, takes its pistol and shoots
    final = reports["raider 20"]
    if final["started"] < 1 or final["raider_combat"] != 1:
        problems.append(f"the raider should have started a fight (OnStartCombat, IsInCombat): {final}")
    if final["raider_dead"] != 0:
        problems.append(f"the raider should be alive: {final}")
    if final["distance"] is None or final["distance"] > 700:
        problems.append("the raider should have come within the range its pistol is shot from (600 units, half of the 1200 "
                        f"of the pistol): {final}")
    if final["raider_ammo"] >= plugin.ARENA_RAIDER_AMMO:
        problems.append(f"the raider should have taken its pistol and shot rounds with it: {final}")
    # The levels the player gained (5.8 seconds in) added health, so the baseline is the first look after them
    if final["health"] >= reports["raider 6"]["health"]:
        problems.append("the player should have lost health to the raider: "
                        f"{reports['raider 6']['health']} before, {final['health']} after")
    # The bystander never starts a fight, has no package and no weapon: shot once, it has to go for the player
    shot_at = re.findall(r"OFTEST\tarena\tbystander shot\t(\d+)\thit=(\w+) damage=[\d.]+ killed=(\w+)$", text, re.M)
    if len(shot_at) != 1 or shot_at[0][1] != "true" or shot_at[0][2] != "false":
        problems.append(f"the bystander should be hit by one shot and survive it: {shot_at}")
    seen = {tag: (float(distance), combat == "true") for tag, distance, combat in re.findall(
        r"OFTEST\tarena\tbystander\t(\w[\w ]*)\tdistance=(-?\d+) combat=(\w+)$", text, re.M)}
    if "armed" not in seen or "raider 20" not in seen:
        problems.append(f"no 'arena bystander' lines in the log: {seen}")
    else:
        before, after = seen["armed"], seen["raider 20"]
        if before[1] or before[0] < 600:
            problems.append(f"the bystander should be at peace and at a distance before it is shot: {before}")
        if not after[1] or after[0] > 300:
            problems.append("the bystander, shot once, should be in a fight with the player and have come to it "
                            f"(distance, in combat): {after}")
    # The grenades are their own ammunition: each throw uses one up, and the third throw is made with the hands
    throws = re.findall(r"OFTEST\tarena\tthrow\t(\d+)\tweapon=(\S*) melee=(\w+) rounds=(-?\d+) ammoUse=(-?\d+) hit=(\w+)$",
                        text, re.M)
    if len(throws) != 3:
        problems.append(f"the player should have thrown three times: {throws}")
    else:
        count = plugin.ARENA_GRENADE_COUNT
        for number, (_, weapon, melee, rounds, ammo_use, hit) in enumerate(throws[:count]):
            if weapon == "" or melee != "false" or int(rounds) != count - number or int(ammo_use) != 1:
                problems.append(f"throw {number + 1} should be made with a grenade and {count - number} left: {throws}")
        _, weapon, melee, rounds, ammo_use, hit = throws[count]
        if weapon != "" or melee != "true":
            problems.append(f"the throw after the last grenade should be made with the hands: {throws}")
    problems += check_arena_screens(text)
    return problems


def check_autoplay(text):
    """The problems with the gates of autoplay on the arena: each of the five gates runs (shoot at the activator, shoot at the
    dummy until it is dead, go to the other cell by its form ID, activate the three switches, go back), the player fires the
    three shots at the activator and the script of the activator hears each (OnHitWith), the dummy dies of the shots it is
    given (OnDeath once), the script of the switches hears three activations, and the player is in the arena again at the end."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error|AUTOPLAY\tfire error", line, re.I):
            problems.append(line)
    if re.search(r"OFTEST\tautoplay\tstage\ttrue$", text, re.M) is None:
        problems.append("the quest of the arena was not put at the stage that gives the rifle")
    gates = re.findall(r"AUTOPLAY\tgate\t(\w+)\t\w+\tstage=(\d+) (\S+) try=(\d+)\t(.*)$", text, re.M)
    expected = [("fire", f"ref={plugin.AUTOPLAY_TARGET_REF_ID:06X}"), ("fire", f"ref={plugin.ARENA_DUMMY_REF_ID:06X}"),
                ("teleport", f"cell={plugin.AUTOPLAY_ROOM_CELL_ID:06X}"),
                ("activate", f"ref={plugin.AUTOPLAY_SWITCH_REF_IDS[0]:06X}"),
                ("teleport", f"cell={plugin.ARENA_CELL_ID:06X}")]
    if [(how, target) for how, _, target, _, _ in gates] != expected:
        problems.append(f"autoplay should run its five gates, in order: {gates}")
    for how, _, target, _, result in gates:
        if result != "ok":
            problems.append(f"the gate {how} {target} did not work: {result}")
    done = re.findall(r"AUTOPLAY\tfire done\t(\w+)\tshots=(\d+) real=([\d.]+)$", text, re.M)
    if [reason for reason, _, _ in done] != ["shots", "dead"] or done and int(done[0][1]) != 3:
        problems.append(f"the player should fire its three shots at the activator, then fire at the dummy until it is dead: {done}")
    states = []
    for line in text.splitlines():
        match = re.search(r"OFTEST\tautoplay\tstate\tt=(\d+) targetHits=(-?[\d.]+) dummyHits=(-?[\d.]+) deaths=(-?[\d.]+) "
                          r"switches=(-?[\d.]+) cell=(.*)$", line)
        if match:
            states.append((int(match.group(1)), *(float(value) for value in match.groups()[1:5]), match.group(6)))
    if not states:
        problems.append("no 'autoplay state' line in the log")
        return problems
    last = states[-1]
    if last[1] != 3:
        problems.append(f"the script of the activator should have heard three blows (OnHitWith): {last}")
    if last[2] < 1 or last[3] != 1:
        problems.append(f"the dummy should have been hit and have died once (OnHit, OnDeath): {last}")
    if last[4] != 3:
        problems.append(f"the script of the switches should have heard three activations: {last}")
    cells = {state[5] for state in states}
    if len(cells) != 2:
        problems.append(f"the player should have been in two cells (the arena, the room, and the arena again): {sorted(cells)}")
    elif states[0][5] != last[5]:
        problems.append(f"the player should be back in the arena at the end: {states[0][5]} {last[5]}")
    return problems


def check_arena_screens(text):
    """The problems with the screens of the arena: the Pip-Boy tells what the character is and carries (the experience of
    the kill, the rifle held, the seven attributes and thirteen skills) and opens its window; the dead dummy is looted (the
    rounds it carries are shown in a window that stops the game); the experience of two levels opens the screen of the level
    up, which has points to spend on a skill."""
    problems = []
    pipboy = re.search(r"OFTEST\tarena\tpipboy\tlevel=(\d+) experience=(\d+) title=(.*?) health=([\d.]+)/([\d.]+) items=(\d+) "
                       r"equipped=(\d+) attributes=(\d+) skills=(\d+) perks=(\d+)$", text, re.M)
    if not pipboy:
        problems.append("no 'arena pipboy' line in the log (core.pipboy did not tell what the character is)")
    else:
        level, experience, _, health, most, items, equipped, attributes, skills, _ = pipboy.groups()
        if int(level) != 1 or int(experience) < 1:
            problems.append(f"the kill of the dummy should have given experience at level 1: {pipboy.group(0)}")
        if float(health) <= 0 or float(most) < float(health):
            problems.append(f"the player should have health: {pipboy.group(0)}")
        if int(items) < 2 or int(equipped) < 1:
            problems.append(f"the Pip-Boy should list the rifle (held) and its rounds: {pipboy.group(0)}")
        if int(attributes) != 7 or int(skills) != 13:
            problems.append(f"the Pip-Boy should list 7 attributes and 13 skills: {pipboy.group(0)}")
    cards = re.search(r"OFTEST\tarena\tpipboyitems\trounds=(\w+) slugs=(\w+) slugsUse=\[(.*?)\] key=(\w+) keyUsable=(\w+) "
                      r"rifle=([\d.]+),(\w+),(.*?),(\d+),(\d+),(\d+) newVegas=(\w+) threshold=([\d.]+)$", text, re.M)
    if not cards:
        problems.append("no 'arena pipboyitems' line in the log (the lists and cards of the Pip-Boy were not read)")
    else:
        rounds, slugs, slugs_use, key, key_usable, damage, has_ammo, ammo_name, clip, loaded, reserve, nv, threshold = \
            cards.groups()
        if (rounds, slugs, slugs_use) != ("true", "false", ""):
            problems.append("the rounds of the rifle in hand should be usable and the others not: " + cards.group(0))
        if (key, key_usable) != ("true", "false"):
            problems.append("the key should be a key that is not used from the list: " + cards.group(0))
        if float(damage) != plugin.ARENA_RIFLE_DAMAGE or has_ammo != "true" or ammo_name != "Test Rounds":
            problems.append("the card of the rifle should tell its damage and its ammunition: " + cards.group(0))
        if int(clip) != 12 or not 0 < int(loaded) <= 12 or int(reserve) < 0:
            problems.append("the card of the rifle should tell the rounds in the clip and in reserve: " + cards.group(0))
        # The arena has no master of Fallout 3, so the game is New Vegas, which tells a threshold (nothing is worn here)
        if nv != "true" or float(threshold) != 0:
            problems.append("New Vegas should tell a damage threshold of 0 for a character in no armor: " + cards.group(0))
    if not re.search(r"OFTEST\tarena\tmode\tInterface$", text, re.M):
        problems.append("the Pip-Boy should have opened the window that stops the game (mode Interface)")
    loot = re.search(r"OFTEST\tarena\tloot\t(.*)$", text, re.M)
    if not loot or loot.group(1) != f"mode=Interface items=1 count={plugin.ARENA_DUMMY_ITEM_COUNT}":
        problems.append("the dead dummy should have been looted in a window that stops the game, with the "
                        f"{plugin.ARENA_DUMMY_ITEM_COUNT} rounds it carries: {loot.group(1) if loot else 'no line'}")
    level_up = re.search(r"OFTEST\tarena\tlevelup\tmode=(\w+) level=(\d+) skillPoints=(\d+) perkPoints=(\d+) spent=(\d+) "
                         r"skillPointsAfter=(\d+) refunded=(\d+) skillPointsBack=(\d+)$", text, re.M)
    if not level_up:
        problems.append("no 'arena levelup' line in the log")
    else:
        mode, level, points, _, spent, after, refunded, back = level_up.groups()
        # The 600 experience of the arena (and the kill) reach the levels 2 and 3
        if mode != "Interface" or int(level) != 3:
            problems.append(f"the experience should have raised the level to 3 and opened the screen of the level up: "
                            f"{level_up.group(0)}")
        if int(points) < 1 or int(spent) < 1 or int(after) != int(points) - int(spent):
            problems.append(f"the level up should have given skill points that can be spent on a skill: {level_up.group(0)}")
        if int(refunded) != 1 or int(back) != int(after) + 1:
            problems.append(f"a point spent on a skill should be taken back by one refund: {level_up.group(0)}")
    # The screen of a level up cannot be left (Escape) with points still to give: it comes back, and goes once they are given
    level_back = re.search(r"OFTEST\tarena\tlevelback\tmode=(\w+) skillPoints=(\d+) perkPoints=(\d+) offered=(\d+)$",
                           text, re.M)
    if not level_back:
        problems.append("no 'arena levelback' line in the log")
    else:
        mode, points, perks, offered = level_back.groups()
        if mode != "Interface":
            problems.append(f"the screen of the level up should have come back after Escape: {level_back.group(0)}")
        if int(points) != 0 or (int(perks) != 0 and int(offered) != 0):
            problems.append(f"the points of the level up should all have been given: {level_back.group(0)}")
    # The experience reached two levels, and each has a screen: the second opens when the first is done, with the points
    # of its own level
    level_second = re.search(r"OFTEST\tarena\tlevelsecond\tmode=(\w+) skillPoints=(\d+)$", text, re.M)
    if not level_second:
        problems.append("no 'arena levelsecond' line in the log")
    elif level_second.group(1) != "Interface" or int(level_second.group(2)) < 1:
        problems.append("the screen of the second level should open with its own skill points once the first is done: "
                        f"{level_second.group(0)}")
    level_next = re.search(r"OFTEST\tarena\tlevelnext\tmode=(\w+) skillPoints=(\d+) perkPoints=(\d+) offered=(\d+)$",
                           text, re.M)
    if not level_next:
        problems.append("no 'arena levelnext' line in the log")
    elif level_next.group(1) != "Interface" or int(level_next.group(2)) != 0:
        problems.append(f"the points of the second level should all have been given: {level_next.group(0)}")
    if "OFTEST\tarena\tlevelgone\tmode=nil" not in text:
        problems.append("the screen of the level up should be gone once every point of every level was given")
    recipes = re.search(r"OFTEST\tarena\trecipes\t(.*)$", text, re.M)
    want = (f"name={plugin.ARENA_CATEGORY_NAME} mode=Interface count=2 statuses=ready,skill "
            f"scrap={plugin.ARENA_SCRAP_GIVEN}/{plugin.ARENA_RECIPE_SCRAP}")
    if not recipes or recipes.group(1) != want:
        problems.append("the script of the bench should have shown the menu of recipes in a window that stops the game, "
                        f"with the recipe the player can make and the one it lacks the skill for: want '{want}', "
                        f"found '{recipes.group(1) if recipes else 'no line'}'")
    craft = re.search(r"OFTEST\tarena\tcraft\t(.*)$", text, re.M)
    want = f"made=made denied=skill again=items gained={plugin.ARENA_RECIPE_ROUNDS}"
    if not craft or craft.group(1) != want:
        problems.append("making the first recipe should take its scrap and give its rounds, the recipe of too much skill "
                        f"should be denied, and a second making should lack scrap: want '{want}', found "
                        f"'{craft.group(1) if craft else 'no line'}'")
    problems += check_arena_vats(text)
    return problems


def check_arena_vats(text):
    """The problems with V.A.T.S.: the player can target the character in front of it, whose body (the part data of its
    race) has four parts that V.A.T.S. can aim at, each with its chance (the most is 95 percent, a part's own chance counts);
    the screen opens on it and stops the game, and closing it tells the engine; the attacks at the head cost 25 action
    points each, cripple the head (and kill the target, which has 10 health), a dead target is refused, and the action
    points come back while V.A.T.S. is closed."""
    problems = []
    info = re.search(r"OFTEST\tarena\tvats\tusable=(\w+) can=(\w+) points=([\d.]+)/([\d.]+) cost=([\d.]+) parts=(.*?) "
                     r"chances=(.*)$", text, re.M)
    if not info:
        return ["no 'arena vats' line in the log (core.vats did not tell what the player can target)"]
    usable, can, points, most, cost, parts, chances = info.groups()
    if usable != "true" or can != "true":
        problems.append(f"V.A.T.S. should be usable and the target in front of the player in reach: {info.group(0)}")
    if parts != "Torso,Head,Left Arm,Right Leg":
        problems.append(f"V.A.T.S. should offer the four parts the race gives a target name (not the spine): {info.group(0)}")
    if chances != "95,95,60,80":
        problems.append("the chances should be the part's own (100, 100, 30, 40) twice over for a rifle shot at this "
                        f"distance (the most that the area of the target counts), at most 95: {info.group(0)}")
    if float(cost) != 25 or float(points) < 75 or float(points) > float(most):
        problems.append(f"an attack with a rifle should cost 25 of the player's action points: {info.group(0)}")
    for tag, want in (("vatsscreen", "mode=Interface open=true"), ("vatsclosed", "mode=nil open=false")):
        line = re.search(r"OFTEST\tarena\t" + tag + r"\t(.*)$", text, re.M)
        if not line or line.group(1) != want:
            problems.append(f"V.A.T.S. '{tag}' should be '{want}': {line.group(1) if line else 'no line'}")
    attack = re.search(r"OFTEST\tarena\tvatsattack\tmade=(\d+) hit=(\w+) spent=([\d.]+) crippled=(\w+) killed=(\w+) "
                       r"head=(-?\d+) torso=(-?\d+)$", text, re.M)
    if not attack:
        problems.append("no 'arena vatsattack' line in the log")
    else:
        made, hit, spent, crippled, killed, head, torso = attack.groups()
        if hit != "true" or float(spent) != 25 * int(made):
            problems.append(f"an attack should hit (95 percent) and cost 25 points: {attack.group(0)}")
        if crippled != "true" or int(head) > 0 or int(torso) != 100:
            problems.append(f"the hit at the head should cripple the head and leave the torso alone: {attack.group(0)}")
        if killed != "true":
            problems.append(f"the target (10 health) should have died of a head shot of 20: {attack.group(0)}")
    dead = re.search(r"OFTEST\tarena\tvatsdead\t(\w+)$", text, re.M)
    if not dead or dead.group(1) != "dead":
        problems.append(f"an attack at the dead target should be refused as 'dead': {dead.group(1) if dead else 'no line'}")
    regen = re.search(r"OFTEST\tarena\tvatsregen\t(\w+)$", text, re.M)
    if not regen or regen.group(1) != "true":
        problems.append("the action points should come back while V.A.T.S. is closed")
    return problems


def check_new_game(text):
    """The problems with the new game: the engine found the quest CG00, started it, its MenuMode block set stage 0, the
    result script of the stage took the controls of the player and moved them to the marker of the interior cell (facing the
    way the marker does) and set stage 5; the script of the quest showed a box with two buttons, which autoplay.lua
    answered with the first, so that stage 10 was set, which gave the controls back and opened the screen for the name,
    which autoplay.lua finished."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Lua error|Fatal error|Failed to load Lua|cannot be run|Frame error", line, re.I):
            problems.append(line)
    if f"New game: the quest of character creation is {plugin.NEW_GAME_QUEST_NAME}" not in text:
        problems.append(f"the engine did not log the quest {plugin.NEW_GAME_QUEST_NAME} as the quest of character creation")
    if f"Loading cell {plugin.WORLD_NAME}Cell (0, 0)" in text:
        problems.append("--start named the exterior cell, which a new game of Fallout must not load: the quest takes the "
                        "player where the game begins")
    found = [re.search(r"OFTEST\tnewgame[\t ]quest\t(.*)$", line) for line in text.splitlines()]
    quests = [match.group(1).strip() for match in found if match]
    if quests != [f"stage={plugin.NEW_GAME_LAST_STAGE} running=true"]:
        problems.append(f"the quest should be running at stage {plugin.NEW_GAME_LAST_STAGE} (its MenuMode block set stage "
                        f"0, the result of which set stage {plugin.NEW_GAME_LAST_STAGE}): {quests}")
    boxes = [line for line in text.splitlines() if re.search(r"OFTEST\tnewgame[\t ]box\t", line)]
    if len(boxes) != 1:
        problems.append(f"expected one 'newgame box' line (the box the quest shows), found {len(boxes)}")
    else:
        first, second = plugin.NEW_GAME_BUTTONS
        wanted = (f"buttons=2 first={first} second={second}\t"
                  "playercontrols=false playerfighting=false playerlooking=false playerviewswitch=false\t"
                  f"yaw={plugin.NEW_GAME_YAW:.3f}\tat={plugin.NEW_GAME_PLACE[0]:.0f},{plugin.NEW_GAME_PLACE[1]:.0f}"
                  "\tbounds=false,false")
        if not boxes[0].endswith(wanted):
            problems.append(f"while the box is open the player should have lost movement, fighting, looking and the view "
                            f"(DisablePlayerControls 1 1 1 1 1), stand at the marker and face the way of it, and the box "
                            f"should take no button it does not have: {boxes[0]!r} should end with {wanted!r}")
    stances = [match.group(1).strip() for match in
               (re.search(r"OFTEST\tnewgame[\t ]stance\t(.*)$", line) for line in text.splitlines()) if match]
    if stances != ["drawn\ttrue", "after\ttrue"]:
        problems.append("the player draws a weapon while the box is open, and DisablePlayerControls 0 0 1 (fighting alone) "
                        f"must put it away: {stances}")
    found = [re.search(r"OFTEST\tnewgame[\t ]controls\t(.*)$", line) for line in text.splitlines()]
    controls = [match.group(1).strip() for match in found if match]
    if controls != ["playercontrols=true playerfighting=true playerlooking=true playerviewswitch=true"]:
        problems.append(f"EnablePlayerControls should have given all the controls back: {controls}")
    automatic = re.findall(r"AUTOPLAY\t(box press\t2\ttrue|chargen finish\tname\tname=" + plugin.NEW_GAME_NAME + r")",
                           text)
    if len(automatic) != 2:
        problems.append(f"autoplay.lua should have pressed the last button of the box (the table of gates names the message) and "
                        f"finished the screen of the name "
                        f"with the name {plugin.NEW_GAME_NAME}: {automatic}")
    for stage in (plugin.NEW_GAME_WAIT_STAGE, plugin.NEW_GAME_LAST_STAGE):
        if f"AUTOPLAY\tquest\t{plugin.NEW_GAME_QUEST_NAME}\tstage={stage} running=true" not in text:
            problems.append(f"autoplayworld.lua should have logged stage {stage} of the quest")
    # the quest stands at its last stage, so the two gates of the table are tried once each, in the order of the table
    for how, ref in (("walk", plugin.TRIGGER_REF_ID), ("activate", plugin.SCRIPTED_REF_IDS["interior"])):
        wanted = (f"AUTOPLAY\tgate\t{how}\t{plugin.NEW_GAME_QUEST_NAME}\t"
                  f"stage={plugin.NEW_GAME_LAST_STAGE} ref={ref:06X} try=1\tok")
        if wanted not in text:
            problems.append(f"autoplayworld.lua should have logged the gate {how!r} as {wanted!r}")
    if text.count("AUTOPLAY\tgate\t") != 2:
        problems.append(f"each gate should be tried once, found {text.count('AUTOPLAY' + chr(9) + 'gate' + chr(9))} tries")
    gates = [match.group(1) for match in
             (re.search(r"OFTEST\tnewgame[\t ]gates\t(.*)$", line) for line in text.splitlines()) if match]
    if gates != ["enters=1 any=2 activated=1"]:
        problems.append("the walk gate should have put the player in the trigger (OnTriggerEnter for the player once, and "
                        "for anyone twice: the player and the person that the script makes) and the activate gate should "
                        f"have run the OnActivate block of the scripted object once: {gates}")
    found = [re.search(r"OFTEST\tnewgame[\t ]player\t(\w+)\t(.*)\t([-\d.]+),([-\d.]+),([-\d.]+)$", line)
             for line in text.splitlines()]
    players = [match.groups() for match in found if match]
    if len(players) != 1:
        problems.append(f"expected one 'newgame player' line, found {len(players)}")
    else:
        exterior, name, x, y, _ = players[0]
        wanted = plugin.TRIGGER_PLACE
        if exterior != "false" or name != plugin.CELL_NAME or abs(float(x) - wanted[0]) > MOVE_SLACK \
                or abs(float(y) - wanted[1]) > MOVE_SLACK:
            problems.append(f"the player should stand at the trigger {wanted[:2]} of {plugin.CELL_NAME} (interior), where "
                            f"the walk gate of autoplay put them after the marker: {players[0]}")
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
    return problems


def check_quests(text):
    """The problems with the quests: the script of quest A must have set its stage, whose result script sets the stage of
    quest B, which completes, and changes the global and the objective of B; the journals have the text of the stages;
    asking for a stage again, or for one that is not there, changes nothing; and the Lua side can set the global. Stage 20
    of B has two log entries, and only the one whose condition holds (about the stage of A, named by a form that the
    reader must have given the index of the plugin in the load order) is told and runs its script: the other would add a
    thousand to the global and put a text in the journal."""
    problems = []

    def lines(kind):
        return [line.split("\t")[3:] for line in text.splitlines() if f"OFTEST\tquest {kind}\t" in line]

    def one(kind):
        found = lines(kind)
        if len(found) != 1:
            problems.append(f"expected one 'quest {kind}' line from the Lua script, found {len(found)}")
            return None
        return found[0]

    def journal_of(stage, text_):
        return f"journal=[{stage}={text_}]"

    for kind, name, title, stage, journal, running, completed in (
            ("a", plugin.QUEST_A_NAME, "OpenFallout Test Quest A", plugin.QUEST_A_STAGE,
             journal_of(plugin.QUEST_A_STAGE, plugin.QUEST_A_TEXT), "true", "false"),
            ("b", plugin.QUEST_B_NAME, "OpenFallout Test Quest B", plugin.QUEST_B_STAGE,
             journal_of(plugin.QUEST_B_STAGE, plugin.QUEST_B_TEXT), "true", "true")):
        found = one(kind)
        if found is None:
            continue
        line = found[0]
        want = [f"{name} id=", f"name={title} stage={stage} running={running} completed={completed} failed=false",
                journal]
        if not all(part in line for part in want):
            problems.append(f"quest {kind.upper()} should be '{name}' at stage {stage} (running={running}, "
                            f"completed={completed}) with the journal '{journal}': {line}")

    found = one("objective")
    if found is not None and found != ["true", "true", "false"]:
        problems.append("objective of B should be displayed and completed (set by the result scripts of the stages) and "
                        f"the next one should not be displayed: {found}")
    found = one("by id")
    if found is not None and found != ["true", "true"]:
        problems.append(f"quest A should be found again by the id it reports and by 0x and hexadecimal digits: {found}")
    found = one("again")
    if found is not None and found != ["false", "false", "true", "false"]:
        problems.append("setting the stage that was reached, or one that quest A does not have, should return false "
                        f"and the stages done should be 10 and not 99: {found}")
    found = one("missing")
    if found is not None and found != ["nil"]:
        problems.append(f"a quest that is not in the plugin should be nil: {found}")
    found = one("counter")
    if found is not None and found != [str(plugin.COUNTER_ADDS), "nil"]:
        problems.append(f"the global should be what the result script of the stage added to it ({plugin.COUNTER_ADDS}) "
                        f"and one that is not there should be nil: {found}")
    found = one("counter set")
    if found is not None and found != ["100", "false"]:
        problems.append("setting the global to 100.9 from Lua should keep the whole part, and setting one that is not "
                        f"there should raise an error: {found}")
    # What the player is told: the engine logs the text of each log entry it puts on the screen
    for name, stage, words in ((plugin.QUEST_A_NAME, plugin.QUEST_A_STAGE, plugin.QUEST_A_TEXT),
                               (plugin.QUEST_B_NAME, plugin.QUEST_B_STAGE, plugin.QUEST_B_TEXT)):
        told = [line for line in text.splitlines() if f"Journal: {name} stage {stage}: {words}" in line]
        if len(told) != 1:
            problems.append(f"the player should be told '{words}' once for stage {stage} of {name}, not {len(told)} times")
    if "cannot be run" in text:
        problems.append("a script of a quest could not be run: " + next(
            line for line in text.splitlines() if "cannot be run" in line))
    return problems


def check_objects(text, scenario):
    """The problems with the script of the scripted object: it must have run its OnLoad block once (it disables the cube
    next to it) and its GameMode block every frame (frames counted, the distance to the player measured, the disabled
    cube seen), its OnActivate block must count the activation by the player and not the one by a person, take the
    activation from the default one except where it calls Activate, and when the
    Lua script enables the cube again the script must see it."""
    problems = []

    def lines(kind):
        return [line.split("\t")[3:] for line in text.splitlines() if f"OFTEST\tobject {kind}\t" in line]

    def one(kind):
        found = lines(kind)
        if len(found) != 1:
            problems.append(f"expected one 'object {kind}' line from the Lua script, found {len(found)}")
            return None
        return found[0]

    if lines("missing"):
        problems.append(f"the scripted object, the cube or the person was not found: {lines('missing')[0]}")
        return problems
    found = one("state")
    if found is not None:
        enabled, loaded_once, frames, disabled_seen, activated, distances = found
        if enabled != "false" or loaded_once != "true":
            problems.append("OnLoad should have run once and disabled the cube (enabled=false, loads=1): " + str(found))
        if frames != "true" or disabled_seen != "1":
            problems.append("GameMode should have run every frame and seen the cube disabled: " + str(found))
        if activated != "0":
            problems.append("nothing should have activated the object yet: " + str(found))
        measured, expected = (int(part) for part in distances.split())
        if abs(measured - expected) > OBJECT_DISTANCE_SLACK:
            problems.append(f"GetDistance should be within {OBJECT_DISTANCE_SLACK} units of the distance Lua sees: "
                            + str(found))
    found = one("followers")
    if found is not None and found != ["false", "true"]:
        problems.append("the cubes that have the cube the script disabled for their enable parent should follow it: the "
                        "one enabled with it should be disabled and the one with the inverse flag enabled (false, "
                        "true): " + str(found))
    found = one("activated")
    if found is not None and found != ["1", "2"]:
        problems.append("OnActivate should count each of the two activations by the player (1 then 2): " + str(found))
    found = one("made")
    if found is not None and found[0] != "true":
        problems.append("an object made by Lua should be put in the cell and activated without an error: " + str(found))
    found = one("victim again")
    if found is not None and found != ["true", "0"]:
        problems.append("the cube enabled by Lua should be enabled, and GetDisabled in the script should see it (true, "
                        + "0): " + str(found))
    found = one("defaults")
    if found is not None and found != ["1"]:
        problems.append("of the two activations only the second, which calls Activate, should reach the handlers of "
                        "Lua (1): " + str(found))
    found = one("by object")
    if found is not None and found != ["1"]:
        problems.append("the third activation, in which the scripted object activates the mover, should reach the handlers "
                        "of Lua with the object as the activator (1): " + str(found))
    found = one("idler")
    expected = [str(plugin.IDLER_ITEMS + plugin.IDLER_ADDS - plugin.IDLER_TAKES), str(plugin.IDLER_RANKS[0]), "0", "1"]
    if found is not None and found != expected:
        problems.append("the script should see the idler with the suits of its record changed by its own calls, the "
                        f"rank its record gives in the first faction, out of the second and dead ({expected}): "
                        + str(found))
    found = one("player")
    if found is not None and found != [str(plugin.PLAYER_RANK)]:
        problems.append("the script should see the player in the first faction with the rank that the record of the "
                        f"player gives ({plugin.PLAYER_RANK}): " + str(found))
    found = one("listed")
    if found is not None and found != [str(plugin.CREATURE_ITEMS)]:
        problems.append("the script should see the creature that is placed as a levelled list with the suits of the "
                        f"creature that the list gives ({plugin.CREATURE_ITEMS}): " + str(found))
    found = one("locks")
    expected = ["1", str(plugin.LOCK_LEVEL), "0", "1", str(plugin.RELOCK_LEVEL)]
    if found is not None and found != expected:
        problems.append("the script should see the cube that its record locks as locked at the level of the record, "
                        "then open after UnLock, and the cube it locked at the level it gave "
                        f"(locked, level, locked after, locked, level: {expected}): " + str(found))
    found = one("doomed")
    if found is not None and found != ["1", "1", "false"]:
        problems.append("the cube that the script destroyed and marked for deletion should be seen as destroyed and "
                        "disabled, and Lua should see it disabled (1, 1, false): " + str(found))
    found = one("forms")
    if found is not None and found != ["1", "0", "1"]:
        problems.append("IsInList should find the marker by the record of the list, not the cube before the script "
                        "added it and the cube after (1, 0, 1): " + str(found))
    found = one("progress")
    if found is not None and found != ["0", "1", "1", "0", "0", "1", "1", "0"]:
        problems.append("the player should not have the perk before the script adds it, have it after two AddPerk "
                        "calls (also when HasPerk is asked for the teammates), and not have it after RemovePerk; the "
                        "note should be there only after AddNote; GetIsID should be 1 for the base form of the cube "
                        "and 0 for another (0, 1, 1, 0, 0, 1, 1, 0): " + str(found))
    found = one("world")
    # The script of the beast runs for the beast, for the creature that is placed as a levelled list (which stands for
    # the beast) and for the heir that takes the script from the beast as its template: 3 runs, and 2 without the third
    expected = ["1", "0", "1", "1" if scenario.exterior else "0", "3"]
    if found is not None and found != expected:
        problems.append("GetLinkedRef should tell the cube that the record links the locker to and 0 for the cube "
                        "that has no link, the two cubes should be in the same cell, GetInWorldspace should be 1 only "
                        "in the exterior cell, and the script of the beast should have run for the beast, for the "
                        "creature placed as a list that stands for it and for the creature that takes its script from "
                        f"it as its template (1, 0, 1, worldspace, 3): {found}")
    found = one("regions")
    # The exterior cell lists three regions and the player stands in the first of them, which names the worldspace of the
    # cell; in a building the player is in none. The log entries of quest D that the conditions let through say the same.
    if found is not None:
        want = [1.0, 0.0, 0.0, 0.0] if scenario.exterior else [0.0, 0.0, 0.0, 0.0]
        if len(found) != 4 or any(abs(float(a) - b) > 0.001 for a, b in zip(found, want)):
            problems.append("IsPlayerInRegion should tell 1 only for the region that the exterior cell lists, covers the "
                            "player and has the worldspace of the cell, not for one that the cell lists and does not "
                            "cover the player, one that covers the player and the cell does not list, or one of another "
                            "worldspace, and 0 for all in a building "
                            f"({', '.join(str(int(number)) for number in want)}): {found}")
    for name, words in plugin.REGION_TEXTS.items():
        told = [line for line in text.splitlines()
                if f"Journal: {plugin.QUEST_D_NAME} stage {plugin.QUEST_D_STAGE}: {words}" in line]
        wanted = 1 if scenario.exterior and name == "in" else 0
        if len(told) != wanted:
            problems.append(f"the condition PlayerInRegion should let the log entry '{words}' through {wanted} times, "
                            f"not {len(told)}")
    found = one("clock")
    if found is not None:
        hours = [float(value) for value in found]
        if (max(hours) - min(hours) > 0.001 or not 0.0 <= hours[0] < 24.0
                or abs(hours[0] - plugin.GAME_HOUR_RECORD_VALUE) < 1.0):
            problems.append("GetCurrentTime, the global GameHour and GetGlobalValue of it should all tell the clock of "
                            f"the world (an hour from 0 to 24), not the {plugin.GAME_HOUR_RECORD_VALUE:g} of the "
                            f"record: {found}")
    found = one("values")
    if found is not None:
        expected = [6.0, 0.75, 1.0, float(plugin.KARMA_REWARD), 100.0, float(plugin.PERSON_ATTRIBUTES[0]),
                    float(plugin.PERSON_ATTRIBUTES[6]), float(plugin.PERSON_SPEED), plugin.PERSON_KARMA, 1.0, 0.0,
                    float(plugin.PERSON_LEVEL), 1.0, 1.0, float(plugin.BEAST_LEVEL)]
        if len(found) != len(expected) or any(abs(float(a) - b) > 0.001 for a, b in zip(found, expected)):
            problems.append("The actor values should be 6 for the one that was set, changed and hurt, three quarters "
                            "of the health after 25 damage, dead after 100, the karma that was rewarded for the player, "
                            "100 for the health of a weapon, the strength, luck, speed and karma that the record "
                            "of the test person gives it, a plugin that is loaded is installed and one that is not "
                            "is not, the level that the record of the test person gives it, the first day of the game "
                            "is a Monday (1), the walker follows the package a script gave it (1), and the creature "
                            "that has a levelled list as its template has the level of the creature the list gives: "
                            f"{found}")
    found = one("placed")
    if found is not None and found != ["true", str(plugin.PLACE_COUNT), str(plugin.PLACE_COUNT)]:
        problems.append("PlaceAtMe should have told the reference of the first object it made, and made "
                        f"{plugin.PLACE_COUNT} cubes of the base form {plugin.PLACE_DISTANCE} units in front of the "
                        f"cube it was called on (true, {plugin.PLACE_COUNT}, {plugin.PLACE_COUNT}): {found}")
    # The cube the script moved to the target marker with offsets, as the script saw it and as the world has it, then the
    # cube at the height the script set and the player moved into the other kind of cell
    origin = plugin.EXTERIOR_CELL_SIZE / 2 if scenario.exterior else 0.0
    want_x = origin + plugin.MOVE_TARGET_PLACE[0] + plugin.MOVE_OFFSET[0]
    want_y = origin + plugin.MOVE_TARGET_PLACE[1] + plugin.MOVE_OFFSET[1]
    want_angle = math.degrees(plugin.TARGET_YAW)
    found = one("mover")
    if found is not None:
        seen, placed = ([float(number) for number in part.split()] for part in found[:2])
        near = float(found[2])
        want_near = math.hypot(plugin.MOVE_OFFSET[0], plugin.MOVE_OFFSET[1])
        if abs(near - want_near) > MOVE_SLACK:
            problems.append("GetDistance right after MoveToMarker should see the cube at the marker with the offsets "
                            f"({want_near:.1f} units from it, the move is not made yet): {found}")
        for what, values in (("saw", seen), ("was put", placed)):
            if (abs(values[0] - want_x) > MOVE_SLACK or abs(values[1] - want_y) > MOVE_SLACK
                    or angle_difference(math.radians(values[2]), math.radians(want_angle)) > math.radians(MOVE_SLACK)):
                problems.append(f"the cube the script moved to the marker should be where the script {what} it "
                                f"({want_x:g}, {want_y:g}, facing {want_angle:g} degrees): {found}")
    found = one("moved")
    if found is not None:
        height, exterior, name, where, asked = found
        x, y = (float(number) for number in where.split())
        other = plugin.EXTERIOR_CELL_SIZE / 2 if not scenario.exterior else 0.0
        if abs(float(height) - plugin.MOVE_HEIGHT) > MOVE_SLACK:
            problems.append(f"SetPos should have put the cube at height {plugin.MOVE_HEIGHT}: {found}")
        if exterior != str(not scenario.exterior).lower() or (scenario.exterior and name != plugin.CELL_NAME):
            problems.append("MoveToMarker should have moved the player to the other kind of cell "
                            f"(exterior={str(not scenario.exterior).lower()}, the interior {plugin.CELL_NAME}): "
                            + str(found))
        if abs(x - other) > MOVE_SLACK or abs(y - other) > MOVE_SLACK:
            problems.append(f"the player should stand at the marker of the other cell ({other:g}, {other:g}): "
                            + str(found))
        if asked != "0":
            problems.append("the script should have taken back the request of the move: " + str(found))
    # The cube that the script of the other cell sent to a marker while that cell was not loaded stands there now that it
    # is; the one of the cell the run started in was not sent anywhere by a script that ran
    other_cell = str(not scenario.exterior).lower()
    sleepers = [line for line in lines("sleeper") if line[0] == other_cell]
    if len(sleepers) != 1:
        problems.append(f"expected one 'object sleeper' line for the cube in the other cell, found {sleepers}")
    else:
        x, y = (float(number) for number in sleepers[0][1].split())
        other = plugin.EXTERIOR_CELL_SIZE / 2 if not scenario.exterior else 0.0
        want = (other + plugin.SLEEPER_TARGET_PLACE[0] + plugin.SLEEPER_OFFSET[0],
                other + plugin.SLEEPER_TARGET_PLACE[1] + plugin.SLEEPER_OFFSET[1])
        if abs(x - want[0]) > MOVE_SLACK or abs(y - want[1]) > MOVE_SLACK:
            problems.append(f"the cube a script sent to a marker while its cell was not loaded should be at "
                            f"{want} when the cell is loaded: {sleepers[0]}")
        if abs(float(sleepers[0][2]) - plugin.SLEEPER_PITCH) > MOVE_SLACK:
            problems.append("SetAngle on x after the move, while the cell was not loaded, should have turned the cube "
                            f"{plugin.SLEEPER_PITCH} degrees when the cell is loaded: {sleepers[0]}")
    found = one("setter")
    if found is not None:
        x, y = (float(number) for number in found[0].split())
        origin = plugin.EXTERIOR_CELL_SIZE / 2 if scenario.exterior else 0.0
        if abs(x - origin - plugin.SETTER_SET[0]) > MOVE_SLACK or abs(y - origin - plugin.SETTER_SET[1]) > MOVE_SLACK:
            problems.append("a stage of a quest set from Lua runs its script on the thread of Lua, and the second "
                            "SetPos on the cube must start from the place the first left it (both coordinates set: "
                            f"{origin + plugin.SETTER_SET[0]:g}, {origin + plugin.SETTER_SET[1]:g}): {found}")
    found = one("hopped")
    if found is not None:
        exterior, name, x, asked, setter_at = found
        setter_x, setter_y, setter_yaw = (float(number) for number in setter_at.split())
        origin_set = plugin.EXTERIOR_CELL_SIZE / 2 if scenario.exterior else 0.0
        if (abs(setter_x - origin_set - plugin.SETTER_SET[0]) > MOVE_SLACK
                or abs(setter_y - origin_set - plugin.SETTER_SET[1]) > MOVE_SLACK
                or angle_difference(math.radians(setter_yaw), math.radians(plugin.SETTER_TURN)) > math.radians(MOVE_SLACK)):
            problems.append("SetAngle alone should turn the cube and leave it where the result script of quest C put it "
                            f"({origin_set + plugin.SETTER_SET[0]:g}, {origin_set + plugin.SETTER_SET[1]:g}, facing "
                            f"{plugin.SETTER_TURN} degrees): {found}")
        origin = plugin.EXTERIOR_CELL_SIZE / 2 if scenario.exterior else 0.0
        if (exterior != str(scenario.exterior).lower() or (not scenario.exterior and name != plugin.CELL_NAME)
                or abs(float(x) - origin - plugin.HOP_X) > MOVE_SLACK or asked != "0"):
            problems.append("SetPos on the player should have moved it to the new x in the cell it was in "
                            f"({origin + plugin.HOP_X:g}) and the script should have taken back the request: "
                            + str(found))
    found = one("crosser")
    if found is not None:
        saw, exterior, grid, x = found
        # An x in the next cell puts the cube in the cell next to the one it is in: the script sees that at once (the
        # world makes the move at the start of the next frame), and the world then puts it there. The interior cell has
        # no cell next to it.
        want_grid = f"{plugin.DRY_CELL[0]} {plugin.DRY_CELL[1]}" if scenario.exterior else "none"
        if (float(saw) != (1.0 if scenario.exterior else 0.0) or exterior != str(scenario.exterior).lower()
                or grid != want_grid or abs(float(x) - plugin.CROSS_X) > MOVE_SLACK):
            problems.append("SetPos on x into the next exterior cell should put the cube in that cell for GetInCell "
                            f"right away ({'1' if scenario.exterior else '0'}) and the world should put it there "
                            f"(grid {want_grid}, x {plugin.CROSS_X}): {found}")
    found = one("traveller")
    if found is not None:
        exterior, where = found
        x, y = (float(number) for number in where.split())
        other = plugin.EXTERIOR_CELL_SIZE / 2 if not scenario.exterior else 0.0
        if exterior != str(not scenario.exterior).lower() or abs(x - other) > MOVE_SLACK or abs(y - other) > MOVE_SLACK:
            problems.append("the cube that a script sent to the marker of the other kind of cell before it moved the "
                            f"player should be at that marker ({other:g}, {other:g}) in that cell: {found}")
    found = one("far mover")
    if found is not None:
        exterior, name, where, seen = found
        x, y = (float(number) for number in where.split())
        other = plugin.EXTERIOR_CELL_SIZE / 2 if not scenario.exterior else 0.0
        if exterior != str(not scenario.exterior).lower() or (scenario.exterior and name != plugin.CELL_NAME):
            problems.append("the cube sent to the marker of the other kind of cell should be in that cell "
                            f"(exterior={str(not scenario.exterior).lower()}): " + str(found))
        if abs(x - other - plugin.FAR_OFFSET[0]) > MOVE_SLACK or abs(y - other - plugin.FAR_OFFSET[1]) > MOVE_SLACK:
            problems.append(f"the cube should be at the marker of the other cell with the offsets "
                            f"({other + plugin.FAR_OFFSET[0]:g}, {other + plugin.FAR_OFFSET[1]:g}): " + str(found))
        if abs(float(seen) - other - plugin.FAR_OFFSET[0]) > MOVE_SLACK:
            problems.append("GetPos in the script should have seen the cube at its new place, in the other cell: "
                            + str(found))
    told = [line for line in text.splitlines() if f"Message: {plugin.MESSAGE_TEXT}" in line]
    # once when the cell of the start is loaded, and once more when MoveToMarker takes the player to the other cell, whose
    # scripted object has the same script
    if len(told) != 2:
        problems.append(f"ShowMessage in OnLoad should have told the player '{plugin.MESSAGE_TEXT}' in each of the two "
                        f"cells, not {len(told)} times")
    for line in text.splitlines():
        if re.search(r"The script 0x[0-9a-f]+ of an object|The script command .* is not implemented", line):
            problems.append("a script of an object could not run: " + line)
    return problems


def angle_difference(a, b):
    """How far apart two angles are, radians, 0 to pi."""
    return abs((a - b + math.pi) % (2 * math.pi) - math.pi)


def check_followers(text, scenario, samples, animated, where):
    """The problems with the characters that follow the player: they stand still until it is time (the companion is told
    to follow by a script, the follower has a package), and later stay within their distance of the player (and a little
    farther, for the slack) facing it. Without the animation to walk with they are not moved."""
    problems = []
    player = [(float(m.group(1)), float(m.group(4)), float(m.group(5)))
              for m in map(SAMPLE.search, text.splitlines()) if m]
    if "companion told to follow" not in text:
        problems.append("the script did not tell the companion to follow the player")
    _, px, py = player[-1]
    for name, distance in plugin.FOLLOW_DISTANCES.items():
        rows = samples[name]
        home = where(name)
        _, (x, y, z), yaw = rows[-1]
        away = math.hypot(x - px, y - py)
        if not animated:
            moved = max(math.hypot(r[1][0] - home[0], r[1][1] - home[1]) for r in rows)
            if moved > UNMOVED_RANGE:
                problems.append(f"the {name} was moved {moved:.1f} units, but it has no animation to walk with")
            continue
        if name == "companion":
            early = [r for r in rows if r[0] < plugin.COMPANION_FROM - 0.5]
            if any(math.hypot(r[1][0] - home[0], r[1][1] - home[1]) > UNMOVED_RANGE for r in early):
                problems.append("the companion was moved before the script told it to follow")
        if not 60.0 <= away <= distance + FOLLOW_SLACK:
            problems.append(f"the {name} ended {away:.0f} units from the player, expected it within {distance} units "
                            f"and the slack ({distance + FOLLOW_SLACK:.0f}) and not on top of the player")
        if angle_difference(yaw, math.atan2(px - x, py - y)) > FOLLOW_YAW_RANGE:
            problems.append(f"the {name} ended facing {yaw:.2f}, expected it to face the player "
                            f"({math.atan2(px - x, py - y):.2f})")
        if math.hypot(x - home[0], y - home[1]) < 20.0:
            problems.append(f"the {name} did not move from where it was put")
    return problems


def check_packages(text, scenario, animated):
    """The problems with the characters that have AI packages: the walker goes to the marker of its package that is on and
    turns the way the marker faces, the idler stays, the roamer goes about the radius of its package. Without the
    animation to walk with none of them is moved."""
    problems = []
    samples = {}
    for match in map(ACTOR_LINE.search, text.splitlines()):
        if match:
            samples.setdefault(match.group(1), []).append(
                (float(match.group(2)), tuple(float(match.group(i)) for i in (3, 4, 5)), float(match.group(6))))
    for name in tuple(PACKAGE_START) + tuple(plugin.FOLLOW_PLACES) + tuple(GATE_START):
        if len(samples.get(name, [])) < WALK_SECONDS - 5:
            problems.append(f"the script logged {len(samples.get(name, []))} positions of the '{name}', expected at least "
                            f"{WALK_SECONDS - 5}")
            return problems

    def where(name):
        x, y = {**PACKAGE_START, **plugin.FOLLOW_PLACES, **GATE_START}[name]
        return scenario.origin[0] + x, scenario.origin[1] + y

    def away(name, position):
        home = where(name)
        return math.hypot(position[0] - home[0], position[1] - home[1])

    for name, rows in samples.items():
        for time_, (x, y, z), _ in rows:
            if not -2.0 <= z <= 4.0:
                problems.append(f"at {time_:.0f} s the '{name}' is {z:.1f} units high, not on the floor at 0")
                break
    for time_, position, _ in samples["idler"]:
        if away("idler", position) > UNMOVED_RANGE:
            problems.append(f"the idler, which has no package, was moved {away('idler', position):.1f} units at "
                            f"{time_:.0f} s")
            break
    target = (scenario.origin[0] + plugin.PACKAGE_PLACES["target"][0], scenario.origin[1] + plugin.PACKAGE_PLACES["target"][1])
    night = (scenario.origin[0] + plugin.PACKAGE_PLACES["night"][0], scenario.origin[1] + plugin.PACKAGE_PLACES["night"][1])
    walker = samples["walker"]
    problems += check_followers(text, scenario, samples, animated, where)
    problems += check_scripted_packages(text, scenario, samples, animated, where)
    if not animated:
        # Its Travel package tells scripts when it is done, which a quest may wait for: a character that cannot walk there
        # is put at the marker instead (the second time it finds no way)
        _, (x, y, _), _ = walker[-1]
        if math.hypot(x - target[0], y - target[1]) > UNMOVED_RANGE:
            problems.append(f"the walker, which has no animation to walk with and a package that scripts wait for, ended "
                            f"at {x:.0f},{y:.0f}, not at the marker at {target[0]:.0f},{target[1]:.0f}")
        moved = max(away("roamer", position) for _, position, _ in samples["roamer"])
        if moved > UNMOVED_RANGE:
            problems.append(f"the roamer was moved {moved:.1f} units, but it has no animation to walk with")
        return problems

    _, (x, y, _), yaw = walker[-1]
    arrived_at = math.hypot(x - target[0], y - target[1])
    if abs(arrived_at - plugin.TRAVEL_RADIUS) > TRAVEL_SLACK:
        problems.append(f"the walker ended at {x:.0f},{y:.0f}, {arrived_at:.0f} units from the marker of its package at "
                        f"{target[0]:.0f},{target[1]:.0f}, expected it at the edge of the radius of the package, "
                        f"{plugin.TRAVEL_RADIUS} units from it")
    if angle_difference(yaw, plugin.TARGET_YAW) > YAW_RANGE:
        problems.append(f"the walker ended facing {yaw:.2f}, expected the way the marker faces, {plugin.TARGET_YAW:.2f}")
    if any(math.hypot(x - night[0], y - night[1]) < ARRIVAL_RANGE * 3 for _, (x, y, _), _ in walker):
        problems.append("the walker went to the marker of the package for the night, which is not on")
    # on its way north it faces north
    start_y = where("walker")[1]
    walking = [yaw for _, (x, y, _), yaw in walker
               if start_y + 40.0 < y < target[1] - plugin.TRAVEL_RADIUS - TRAVEL_SLACK]
    if not walking:
        problems.append("the walker was never seen between its start and the marker")
    elif any(angle_difference(yaw, 0.0) > WALK_YAW_RANGE for yaw in walking):
        problems.append(f"the walker did not face the way it walked (north) between its start and the marker: "
                        f"{[round(yaw, 2) for yaw in walking]}")
    roamer = samples["roamer"]
    farthest = max(away("roamer", position) for _, position, _ in roamer)
    if farthest > plugin.ROAM_RADIUS + ROAM_SLACK:
        problems.append(f"the roamer went {farthest:.0f} units from where it was put, its package says "
                        f"{plugin.ROAM_RADIUS}")
    if farthest < 5.0:
        problems.append("the roamer, which has a package that sends it about, did not move")
    return problems


def check_scripted_packages(text, scenario, samples, animated, where):
    """The problems with the two characters whose packages depend on scripts. The conditional one has a package whose
    condition never holds (it must not go to its marker) and one that holds from the time the test sets a stage of
    quest C: it stays where it is put until then and goes to the marker of the second. The scripted one has no package
    of its own, and the script of the scripted object gives it one: it goes to the marker of that. Without the
    animation to walk with none of them is moved."""
    problems = []
    if "OFTEST\tgate opened\ttrue" not in text:
        problems.append("the stage of quest C that opens the gate was not set from Lua")
    problems += check_package_events(text, scenario, animated)

    def marker(name):
        x, y = plugin.GATE_PLACES[name]
        return scenario.origin[0] + x, scenario.origin[1] + y

    def distance(position, place):
        return math.hypot(position[0] - place[0], position[1] - place[1])

    def edge_of(name, rows):
        _, (x, y, _), _ = rows[-1]
        arrived_at = distance((x, y), marker(name))
        if abs(arrived_at - plugin.TRAVEL_RADIUS) > TRAVEL_SLACK:
            return (f"ended at {x:.0f},{y:.0f}, {arrived_at:.0f} units from the marker at {marker(name)[0]:.0f},"
                    f"{marker(name)[1]:.0f}, expected it at the edge of the radius of the package, "
                    f"{plugin.TRAVEL_RADIUS} units from it")
        return None

    conditional, scripted = samples["conditional"], samples["scripted"]
    if not animated:
        for name, rows in (("conditional", conditional), ("scripted", scripted)):
            if max(distance(position[:2], where(name)) for _, position, _ in rows) > UNMOVED_RANGE:
                problems.append(f"the {name} character was moved, but it has no animation to walk with")
        return problems

    before = [(time_, position) for time_, position, _ in conditional if time_ < plugin.GATE_FROM - GATE_MARGIN]
    if not before:
        problems.append("the conditional character was never seen before the gate opened")
    for time_, position in before:
        if distance(position[:2], where("conditional")) > UNMOVED_RANGE:
            problems.append(f"the conditional character, whose packages do not hold yet, was moved "
                            f"{distance(position[:2], where('conditional')):.1f} units at {time_:.0f} s, before the gate "
                            f"opened at {plugin.GATE_FROM:.0f} s")
            break
    if (message := edge_of("gated", conditional)) is not None:
        problems.append("the conditional character, whose second package holds from the gate on, " + message)
    if any(distance(position[:2], marker("blocked")) < ARRIVAL_RANGE * 3 for _, position, _ in conditional):
        problems.append("the conditional character went to the marker of the package whose condition never holds")
    if (message := edge_of("scripted_marker", scripted)) is not None:
        problems.append("the scripted character, which a script gave a package, " + message)
    return problems


def check_package_events(text, scenario, animated):
    """The problems with what the packages tell scripts. The Travel package of the walker is begun once and done once
    (the walker walks there, or is put there when it cannot walk), and changed once after that, as nothing follows it;
    the package of the conditional character that holds until the gate
    opens is begun once and changed when the gate opens (never done); the blocks of the script of the walker that name the
    Travel package, and the one that names none, run for its events, and the one that names the package for the night,
    which is not on, does not. The speaker of the interior has a Dialogue package: it walks to the player, says its line and
    its package is done when the line is over (it does not walk without the animation, and the exterior has none). The
    character of the interior whose Dialogue package targets the intercom (a talking activator) talks to it where it
    stands, and its package is done when the line is over."""
    names = list(plugin.PACKAGE_EVENT_GLOBALS)
    match = re.search(r"OFTEST\tpackage events\t" + r"\t".join([r"(-?\d+)"] * len(names)), text)
    if not match:
        return ["the script did not log what the events of the packages counted"]
    got = dict(zip(names, map(int, match.groups())))
    expected = {"travel_begin": 1, "travel_end": 1, "travel_change": 1, "early_begin": 1, "early_end": 0,
                "early_change": 1, "block_start": 1, "block_done": 1, "block_other": 0, "block_any": 1,
                "speaker_done": 1 if not scenario.exterior and animated else 0,
                "intercom_done": 0 if scenario.exterior else 1}
    problems = [f"the global {plugin.PACKAGE_EVENT_GLOBALS[name][1]} counted {got[name]} events of packages, expected "
                f"{want}" for name, want in expected.items() if got[name] != want]
    if not scenario.exterior and animated:
        if not re.search(r"Package OFTestSpeak of .*: it talks to the player", text):
            problems.append("the speaker, whose Dialogue package sends it to the player, did not talk to the player")
        if not re.search(r"Say: .* is to say topic OFTestSpeakTopic .*: 1 line\(s\)", text):
            problems.append("the topic of the Dialogue package of the speaker was not said")
    intercom = re.search(r"OFTEST\tintercom\t(\w+)\t(\w+)", text)
    if not intercom:
        problems.append("the script did not log whether the talking activator is an object of the world")
    elif (intercom.group(1), intercom.group(2)) != (("false", "false") if scenario.exterior else ("true", "true")):
        problems.append("the talking activator should be an object of its own type in the interior and not be in the "
                        f"exterior: {intercom.group(0)}")
    if not scenario.exterior:
        if not re.search(r"Package OFTestIntercomTalk of .*: it talks to a character", text):
            problems.append("the character whose Dialogue package targets the talking activator did not talk to it")
    return problems


def check_creatures(text, scenario, found, animated):
    """The problems with the creatures: the body model of each is drawn, its foot is where the animation put it, it is
    a solid body, and the one placed as a levelled list has the record of the entry of the list."""
    problems = []
    for line in text.splitlines():
        if re.search(r" [EW]\] .*(?:body model of the creature|oftestbeast|OFTestBeast)", line):
            problems.append(line)
    actor_rays = {m.group(1): m for m in map(CREATURE_ACTOR_RAY.search, text.splitlines()) if m}
    for name, (x, y) in plugin.CREATURE_POSITIONS.items():
        creature_x, creature_y = scenario.origin[0] + x, scenario.origin[1] + y
        top = found.get(f"creaturetop_{name}")
        if top is None:
            problems.append(f"the script did not log the ray from above at what is drawn of the creature '{name}'")
        elif top.group(2) != "true" or abs(float(top.group(5)) - CREATURE_TOP) > 1.5:
            problems.append(f"the ray from above at the creature '{name}' should hit the top of its body at "
                            f"{CREATURE_TOP:.1f}: {top.group(0)}")
        elif abs(float(top.group(3)) - creature_x) > 1.5 or abs(float(top.group(4)) - creature_y) > 1.5:
            problems.append(f"the ray from above at the creature '{name}' hit it somewhere else than where it stands "
                            f"({creature_x:.0f},{creature_y:.0f}): {top.group(0)}")
        foot = found.get(f"creaturefoot_{name}")
        want = creature_x + (FOOT_PLAYED_FACE if animated else FOOT_REST_FACE)
        if foot is None:
            problems.append(f"the script did not log the ray at the foot of the creature '{name}'")
        elif foot.group(2) != "true" or abs(float(foot.group(3)) - want) > 1.5:
            problems.append(f"the ray at the foot of the creature '{name}' should hit the box that is skinned to it at "
                            f"x={want:.1f}: {foot.group(0)}")
        actor = actor_rays.get(name)
        if actor is None:
            problems.append(f"the script did not log the ray at the actors for the creature '{name}'")
            continue
        form_id = re.search(r"0x([0-9a-f]+)", actor.group(6), re.I)
        if (actor.group(2) != "true" or abs(float(actor.group(3)) - (creature_x + CREATURE_RADIUS)) > 5.0
                or not form_id or int(form_id.group(1), 16) & 0xFFFFFF != CREATURE_RECORDS[name]):
            problems.append(f"the ray at the actors from the east at the creature '{name}' should hit its body at "
                            f"x={creature_x + CREATURE_RADIUS:.0f} and find the record {CREATURE_RECORDS[name]:#x}: "
                            f"{actor.group(0)}")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, type=Path, help="build directory with the openfallout program")
    parser.add_argument("--seconds", type=float, default=90, help="how long to give the engine before stopping it")
    parser.add_argument("--keep", action="store_true", help="keep the temporary directory and print its path")
    parser.add_argument("--no-animation", action="store_true",
                        help="leave the animation file of the person out, which must leave its foot where the skeleton "
                             "has it: the control of the check that the animation moves it")
    parser.add_argument("--key-animation", action="store_true",
                        help="move the foot with two translation keys, not the control points of a B-spline")
    parser.add_argument("--only", choices=[scenario.name for scenario in SCENARIOS] + ["dialogue", "chargen", "newgame", "triggers", "arena", "autoplay", "saves", "savesprobe", "mods", "activation", "archives"],
                        help="run only this start (or the conversation, the making of the character or the new game), "
                             "not all")
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
    plugin.write(data, animation=not args.no_animation, spline=not args.key_animation)
    (data / "scripts").mkdir()
    (data / "scripts" / "walktest.lua").write_text(WALK_SCRIPT, encoding="ascii")
    (data / "scripts" / "personwalk.lua").write_text(PERSON_SCRIPT, encoding="ascii")
    (data / "scripts" / "packagetrack.lua").write_text(PACKAGE_SCRIPT, encoding="ascii")
    (data / "scripts" / "followtest.lua").write_text(FOLLOW_SCRIPT, encoding="ascii")
    (data / "scripts" / "questtest.lua").write_text(QUEST_SCRIPT, encoding="ascii")
    (data / "scripts" / "objecttest.lua").write_text(OBJECT_SCRIPT, encoding="ascii")
    (data / "scripts" / "gatetest.lua").write_text(GATE_SCRIPT, encoding="ascii")
    (data / "scripts" / "dialoguetest.lua").write_text(DIALOGUE_SCRIPT, encoding="ascii")
    (data / "scripts" / "dialoguehost.lua").write_text(DIALOGUE_HOST_SCRIPT, encoding="ascii")
    (data / "dialoguetest.omwscripts").write_text(
        "PLAYER: scripts/dialoguetest.lua\nGLOBAL: scripts/dialoguehost.lua\n", encoding="ascii")
    (data / "scripts" / "arenatest.lua").write_text(ARENA_SCRIPT, encoding="ascii")
    (data / "scripts" / "arenaplayer.lua").write_text(ARENA_PLAYER_SCRIPT, encoding="ascii")
    (data / "arenatest.omwscripts").write_text(
        "PLAYER: scripts/arenaplayer.lua\nGLOBAL: scripts/arenatest.lua\n", encoding="ascii")
    (data / "scripts" / "savesworld.lua").write_text(SAVES_WORLD_SCRIPT, encoding="ascii")
    (data / "scripts" / "savesmenu.lua").write_text(SAVES_MENU_SCRIPT, encoding="ascii")
    (data / "scripts" / "saveslocal.lua").write_text(SAVES_LOCAL_SCRIPT, encoding="ascii")
    (data / "savestest.omwscripts").write_text(
        "GLOBAL: scripts/savesworld.lua\nMENU: scripts/savesmenu.lua\nCUSTOM: scripts/saveslocal.lua\n", encoding="ascii")
    (data / "scripts" / "modstest.lua").write_text(MODS_SCRIPT, encoding="ascii")
    (data / "modstest.omwscripts").write_text("GLOBAL: scripts/modstest.lua\n", encoding="ascii")
    (data / "scripts" / "archivestest.lua").write_text(ARCHIVES_SCRIPT, encoding="ascii")
    (data / "archivestest.omwscripts").write_text("GLOBAL: scripts/archivestest.lua\n", encoding="ascii")
    (data / (Path(plugin.MOD_ADDON).stem + ".NAM")).write_bytes(b"")
    (data / "scripts" / "triggertest.lua").write_text(TRIGGER_SCRIPT, encoding="ascii")
    (data / "triggertest.omwscripts").write_text("GLOBAL: scripts/triggertest.lua\n", encoding="ascii")
    (data / "scripts" / "chargentest.lua").write_text(CHARGEN_SCRIPT, encoding="ascii")
    (data / "scripts" / "chargenhost.lua").write_text(CHARGEN_HOST_SCRIPT, encoding="ascii")
    (data / "chargentest.omwscripts").write_text(
        "PLAYER: scripts/chargentest.lua\nGLOBAL: scripts/chargenhost.lua\n", encoding="ascii")
    (data / "scripts" / "newgametest.lua").write_text(NEW_GAME_SCRIPT, encoding="ascii")
    (data / "scripts" / "newgameplayer.lua").write_text(NEW_GAME_PLAYER_SCRIPT, encoding="ascii")
    # the tool that plays the intro on real data does the answering here
    for name in ("autoplay.lua", "autoplayworld.lua"):
        shutil.copy(AUTOPLAY / "scripts" / name, data / "scripts" / name)
    (data / "scripts" / "autoplaygates.lua").write_text(NEW_GAME_GATES, encoding="ascii")
    (data / "newgametest.omwscripts").write_text(
        "PLAYER: scripts/autoplay.lua\nPLAYER: scripts/newgameplayer.lua\nGLOBAL: scripts/autoplayworld.lua\n"
        "GLOBAL: scripts/newgametest.lua\n", encoding="ascii")
    (data / "walktest.omwscripts").write_text(
        "PLAYER: scripts/walktest.lua\nPLAYER: scripts/packagetrack.lua\nGLOBAL: scripts/personwalk.lua\n"
        "GLOBAL: scripts/followtest.lua\nGLOBAL: scripts/questtest.lua\nGLOBAL: scripts/objecttest.lua\n"
        "GLOBAL: scripts/gatetest.lua\n", encoding="ascii")

    xvfb = start_xvfb()
    for scenario in SCENARIOS:
        if args.only not in (None, scenario.name):
            continue
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
        found += check(text, scenario, colours, animated=not args.no_animation)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print(f"PROBLEM ({scenario.name}):", problem)
        problems += found
    if args.only in (None, "dialogue"):
        home = work / "dialogue" / "home"
        home.mkdir(parents=True)
        runtime = work / "dialogue" / "runtime"
        runtime.mkdir(mode=0o700)
        print("== dialogue: --start", plugin.CELL_NAME)
        # The response that ends the conversation shows a message box with buttons: Tab goes to the next button, which is
        # the third (the second is not shown), and Return picks it
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.CELL_NAME, [],
                               "dialoguetest.omwscripts",
                               keys=(r"Message: " + re.escape(plugin.BUTTON_MESSAGE_TEXT), [KEY_TAB, KEY_RETURN], 1.5))
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_dialogue(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (dialogue):", problem)
        problems += found
    if args.only in (None, "chargen"):
        home = work / "chargen" / "home"
        home.mkdir(parents=True)
        runtime = work / "chargen" / "runtime"
        runtime.mkdir(mode=0o700)
        print("== chargen: --start", plugin.CELL_NAME)
        # The name is typed on the keyboard when its screen has opened: three letters and Return
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.CELL_NAME, [],
                               "chargentest.omwscripts",
                               keys=(r"OFTEST\tchargen\tscreen name", [0x61, 0x64, 0x61, KEY_RETURN], 1.5))
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_chargen(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (chargen):", problem)
        problems += found
    if args.only in (None, "triggers"):
        home = work / "triggers" / "home"
        home.mkdir(parents=True)
        runtime = work / "triggers" / "runtime"
        runtime.mkdir(mode=0o700)
        print("== triggers: --start", plugin.CELL_NAME)
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.CELL_NAME, [],
                               "triggertest.omwscripts")
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_triggers(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (triggers):", problem)
        problems += found
    if args.only in (None, "arena"):
        home = work / "arena" / "home"
        home.mkdir(parents=True)
        runtime = work / "arena" / "runtime"
        runtime.mkdir(mode=0o700)
        print("== arena: --start", plugin.ARENA_CELL_NAME)
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.ARENA_CELL_NAME, [],
                               "arenatest.omwscripts")
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_arena(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (arena):", problem)
        problems += found
    if args.only in (None, "autoplay"):
        # The gates of autoplay have a table of their own (a data folder of their own, since the table is a script of it)
        home = work / "autoplay" / "home"
        home.mkdir(parents=True)
        runtime = work / "autoplay" / "runtime"
        runtime.mkdir(mode=0o700)
        gate_data = work / "autoplay" / "data"
        plugin.write(gate_data, animation=not args.no_animation, spline=not args.key_animation)
        (gate_data / "scripts").mkdir()
        for name in ("autoplay.lua", "autoplayworld.lua"):
            shutil.copy(AUTOPLAY / "scripts" / name, gate_data / "scripts" / name)
        (gate_data / "scripts" / "autoplaygates.lua").write_text(AUTOPLAY_GATES, encoding="ascii")
        (gate_data / "scripts" / "autoplaytest.lua").write_text(AUTOPLAY_SCRIPT, encoding="ascii")
        (gate_data / "autoplaytest.omwscripts").write_text(
            "PLAYER: scripts/autoplay.lua\nGLOBAL: scripts/autoplayworld.lua\nGLOBAL: scripts/autoplaytest.lua\n", encoding="ascii")
        print("== autoplay: --start", plugin.ARENA_CELL_NAME)
        status, _ = run_engine(program, build / "resources", gate_data, home, runtime, args.seconds, plugin.ARENA_CELL_NAME, [],
                               "autoplaytest.omwscripts")
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_autoplay(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line or "AUTOPLAY" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (autoplay):", problem)
        problems += found
    if args.only in (None, "saves"):
        home = work / "saves" / "home"
        home.mkdir(parents=True)
        for name in ("runtime", "runtime2", "runtime3"):
            (work / "saves" / name).mkdir(mode=0o700)
        log = home / ".config" / "openfallout" / "openfallout.log"
        print("== saves: --start", plugin.CELL_NAME)
        status, _ = run_engine(program, build / "resources", data, home, work / "saves" / "runtime", args.seconds,
                               plugin.CELL_NAME, [], "savestest.omwscripts")
        first = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        second = third = ""
        saved = sorted(home.rglob("*.omwsave"))
        if len(saved) != 1:
            found.append(f"expected one saved game, found {[str(path) for path in saved]}")
        else:
            print(f"== saves: --load-savegame {saved[0].name}")
            log.rename(log.with_name("saving.log"))
            status, _ = run_engine(program, build / "resources", data, home, work / "saves" / "runtime2", args.seconds,
                                   None, [], "savestest.omwscripts", load=saved[0])
            second = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
            if status != 0:
                found.append(f"the engine started on the save exited with status {status}")
            # Once more with a plugin before OFTest.esm: every form ID of the save has another top byte in this load order
            print(f"== saves: --load-savegame {saved[0].name}, {plugin.EMPTY_ADDON} first")
            log.rename(log.with_name("loading.log"))
            status, _ = run_engine(program, build / "resources", data, home, work / "saves" / "runtime3", args.seconds,
                                   None, [], "savestest.omwscripts", load=saved[0], before=[plugin.EMPTY_ADDON])
            third = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
            if status != 0:
                found.append(f"the engine started on the save with another plugin first exited with status {status}")
        found += check_saves(first, second, third)
        for line in (line for text in (first, second, third) for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (saves):", problem)
        problems += found
    if args.only in (None, "savesprobe"):
        # The probe has a table of its own (a data folder of its own, since the table is a script of it)
        home = work / "savesprobe" / "home"
        home.mkdir(parents=True)
        for name in ("runtime", "runtime2"):
            (work / "savesprobe" / name).mkdir(mode=0o700)
        probe_data = work / "savesprobe" / "data"
        plugin.write(probe_data, animation=not args.no_animation, spline=not args.key_animation)
        (probe_data / "scripts").mkdir()
        for name in ("autoplay.lua", "autoplayworld.lua", "autoplayprobe.lua", "autoplaysaves.lua"):
            shutil.copy(AUTOPLAY / "scripts" / name, probe_data / "scripts" / name)
        (probe_data / "scripts" / "autoplaygates.lua").write_text(SAVES_PROBE_GATES, encoding="ascii")
        (probe_data / "scripts" / "savesprobetest.lua").write_text(SAVES_PROBE_SCRIPT, encoding="ascii")
        shutil.copy(AUTOPLAY / "autoplaysaves.omwscripts", probe_data / "autoplaysaves.omwscripts")
        (probe_data / "savesprobetest.omwscripts").write_text(
            "PLAYER: scripts/autoplay.lua\nGLOBAL: scripts/autoplayworld.lua\nGLOBAL: scripts/autoplayprobe.lua\n"
            "MENU: scripts/autoplaysaves.lua\nGLOBAL: scripts/savesprobetest.lua\n", encoding="ascii")
        log = home / ".config" / "openfallout" / "openfallout.log"
        print("== savesprobe: --start", plugin.CELL_NAME)
        status, _ = run_engine(program, build / "resources", probe_data, home, work / "savesprobe" / "runtime",
                               args.seconds, plugin.CELL_NAME, [], "savesprobetest.omwscripts")
        first = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        second = ""
        # The save of the probe is the file that its log names; the second engine is started on it
        named = re.search(r"AUTOPLAY\tsave\tautoplay-save\tok\tdir=(\S+)\tfiles=(\S+)", first)
        saved = [path for path in home.rglob("*.omwsave") if named and path.name == named.group(2).split(",")[0]]
        if len(saved) != 1:
            found.append(f"expected the saved game {named.group(2) if named else None}, found {[str(p) for p in saved]}")
        else:
            print(f"== savesprobe: --load-savegame {saved[0].name}")
            log.rename(log.with_name("probing.log"))
            status, _ = run_engine(program, build / "resources", probe_data, home, work / "savesprobe" / "runtime2",
                                   args.seconds, None, [], "savesprobetest.omwscripts", load=saved[0])
            second = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
            if status != 0:
                found.append(f"the engine started on the save exited with status {status}")
        found += check_savesprobe(first, second)
        for line in (line for text in (first, second) for line in text.splitlines() if "AUTOPLAY" in line):
            print(line.split("]", 1)[-1].strip())
        for path in sorted(home.rglob("*.omwsave")):
            print(f"saved game {path.relative_to(home)}")
        for problem in found:
            print("PROBLEM (savesprobe):", problem)
        problems += found
    if args.only in (None, "mods"):
        home = work / "mods" / "home"
        home.mkdir(parents=True)
        runtime = work / "mods" / "runtime"
        runtime.mkdir(mode=0o700)
        print("== mods: --start", plugin.CELL_NAME, f"with {plugin.MOD_ADDON} named before OFTest.esm")
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.CELL_NAME, [],
                               "modstest.omwscripts", before=[plugin.MOD_ADDON])
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_mods(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line or "Load order" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (mods):", problem)
        problems += found
    if args.only in (None, "activation"):
        home = work / "activation" / "home"
        home.mkdir(parents=True)
        runtime = work / "activation" / "runtime"
        runtime.mkdir(mode=0o700)
        ini = work / "activation" / "Fallout.ini"
        ini.write_text("[General]\nsTestFile1=OFTest.esm\n", encoding="ascii")
        print("== activation: no plugin named; sTestFile1 names OFTest.esm and", plugin.MOD_ADDON, "has a .NAM file")
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.CELL_NAME, [],
                               "modstest.omwscripts", plugins=False, extra=("--game-ini", str(ini)))
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_activation(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line or "Load order" in line
                     or "make active" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (activation):", problem)
        problems += found
    if args.only in (None, "archives"):
        # Two archives made with bsatool: OFTestArchive.bsa has oftest/shared.txt (a loose copy sits beside it),
        # oftest/hidden.txt and oftest/archiveonly.txt; "OFTestMod - Main.bsa" has oftest/modarchive.txt. The archive
        # directory is a second data directory.
        stage = work / "archives_stage"
        extra = work / "archives_data"
        (stage / "oftest").mkdir(parents=True)
        (extra / "oftest").mkdir(parents=True)
        bsatool = build / "bsatool"

        def make_archive(name, files):
            archive = extra / name
            subprocess.run([str(bsatool), "create", str(archive)], check=True, cwd=stage, stdout=subprocess.DEVNULL)
            for relative, text in files.items():
                (stage / relative).write_text(text, encoding="ascii")
                subprocess.run([str(bsatool), "add", str(archive), relative], check=True, cwd=stage,
                               stdout=subprocess.DEVNULL)

        make_archive("OFTestArchive.bsa", {"oftest/shared.txt": "archive", "oftest/hidden.txt": "archive",
                                            "oftest/archiveonly.txt": "archive"})
        make_archive("OFTestMod - Main.bsa", {"oftest/modarchive.txt": "modarchive"})
        (extra / "oftest" / "shared.txt").write_text("loose", encoding="ascii")
        (extra / "ArchiveInvalidation.txt").write_text("hidden.txt\r\n", encoding="ascii")
        for loose_wins in (True, False):
            name = "archives" if loose_wins else "archivesold"
            home = work / name / "home"
            home.mkdir(parents=True)
            runtime = work / name / "runtime"
            runtime.mkdir(mode=0o700)
            ini = work / name / "Fallout.ini"
            ini.write_text(f"[Archive]\nbInvalidateOlderFiles={1 if loose_wins else 0}\n", encoding="ascii")
            print(f"== {name}: bInvalidateOlderFiles={1 if loose_wins else 0}, the archives next to a loose copy")
            status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, plugin.CELL_NAME, [],
                                   "archivestest.omwscripts", before=[plugin.MOD_ADDON],
                                   extra=("--data", str(extra), "--fallback-archive", "OFTestArchive.bsa", "--game-ini",
                                          str(ini)))
            log = home / ".config" / "openfallout" / "openfallout.log"
            text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
            print(f"log: {log}")
            found = []
            if status != 0:
                found.append(f"the engine exited with status {status}")
            found += check_archives(text, loose_wins)
            for line in (line for line in text.splitlines() if "OFTEST" in line or "Archive order" in line
                         or "ArchiveInvalidation" in line):
                print(line.split("]", 1)[-1].strip())
            for problem in found:
                print(f"PROBLEM ({name}):", problem)
            problems += found
    if args.only in (None, "newgame"):
        home = work / "newgame" / "home"
        home.mkdir(parents=True)
        runtime = work / "newgame" / "runtime"
        runtime.mkdir(mode=0o700)
        print(f"== newgame: --new-game --start {NEW_GAME_START_ELSEWHERE}")
        # --start names another cell, which a new game must not go to: the quest takes the player where the game begins
        status, _ = run_engine(program, build / "resources", data, home, runtime, args.seconds, NEW_GAME_START_ELSEWHERE,
                               [], "newgametest.omwscripts", new_game=True)
        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check_new_game(text)
        for line in (line for line in text.splitlines() if "OFTEST" in line):
            print(line.split("]", 1)[-1].strip())
        for problem in found:
            print("PROBLEM (newgame):", problem)
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
