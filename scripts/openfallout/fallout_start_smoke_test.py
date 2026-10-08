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
a package that sends it about the place where it stands must stay within the radius of the package and move. A
character with no animation to walk with (`--no-animation`) is not moved at all.

    scripts/openfallout/fallout_start_smoke_test.py --build build

It needs no game data and no data of Bethesda. The engine's configuration and logs go to a temporary home directory.
The exit status is 0 when every check holds. It also checks that files/data/meshes/placeholder_skeleton.nif is what
placeholder_skeleton.py writes.
"""
import argparse
import math
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
              ("water", HORIZON_TIME, WATER_PIXEL, plugin.WATER_SHALLOW, "greener"),
              ("ground", HORIZON_TIME, GROUND_PIXEL, plugin.GROUND_COLOUR, "tinted"),
              ("sky and clouds", ZENITH_TIME, SKY_PIXEL,
               clouded_sky(plugin.WEATHER_SKY[1], plugin.WEATHER_FOG[1], plugin.CLOUD_ALPHA))]),
]

WALK_SECONDS = 24
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

return {
    engineHandlers = {
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
       CREATURE_BODY_HEIGHT, FOOT_RAY_FROM, FOOT_HEIGHT, FOOT_HEIGHT, WALK_SECONDS)

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
            if elapsed < nextLog then return end
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
""".replace("@ACTORS@", ", ".join("{ '%s', { %d, %d } }" % (name, *plugin.PACKAGE_REF_IDS[name])
                                  for name in ("walker", "idler", "roamer")) + ", " + ", ".join(
                                      "{ '%s', { %d, %d } }" % (name, *plugin.FOLLOW_REF_IDS[name])
                                      for name in ("follower", "companion")))

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

# Where the three characters start relative to the player, and where the walker should end
PACKAGE_START = {name: plugin.PACKAGE_PLACES[name] for name in ("walker", "idler", "roamer")}
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


def check(text, scenario, colours, animated=True):
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
    if "Quitting peacefully" not in text:
        problems.append("the log does not end with 'Quitting peacefully'")
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
    for name in tuple(PACKAGE_START) + tuple(plugin.FOLLOW_PLACES):
        if len(samples.get(name, [])) < WALK_SECONDS - 5:
            problems.append(f"the script logged {len(samples.get(name, []))} positions of the '{name}', expected at least "
                            f"{WALK_SECONDS - 5}")
            return problems

    def where(name):
        x, y = {**PACKAGE_START, **plugin.FOLLOW_PLACES}[name]
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
    if not animated:
        moved = max(away("walker", position) for _, position, _ in walker)
        if moved > UNMOVED_RANGE:
            problems.append(f"the walker was moved {moved:.1f} units, but it has no animation to walk with")
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
    parser.add_argument("--only", choices=[scenario.name for scenario in SCENARIOS],
                        help="run only this start, not both")
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
    (data / "walktest.omwscripts").write_text(
        "PLAYER: scripts/walktest.lua\nPLAYER: scripts/packagetrack.lua\nGLOBAL: scripts/personwalk.lua\n"
        "GLOBAL: scripts/followtest.lua\n", encoding="ascii")

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
