#!/usr/bin/env python3
"""Start the engine in an interior cell and in an exterior cell of a synthetic Fallout 3 format plugin, walk the
player into a wall and check what happened.

A content list made only of Fallout plugins has no player, race, class, skills or settings in the record format of the
engine, so the world has to make placeholders to start. The unit tests cover those records one by one; this runs them
together with the rest of the engine: the plugin and its mesh come from synthetic_fallout_plugin.py, the engine is
started twice with `--skip-menu`, once with `--start OFTestCell` (an interior cell) and once with
`--start OFTestWorld:0,0` (the exterior cell 0,0 of a worldspace), under a virtual display (Xvfb with Mesa software
rendering is enough), and a Lua player script walks north from the start for a few seconds, casts a ray down and
quits. The checks are on the log: the cell is the one asked for, in the interior it is lit with its own ambient and sun colours and with the fog colour and far
distance of its lighting template, the player stands on the floor the whole time, the camera is at eye height above the
player, the player stops at the pillar that is in the way, nothing logs an error from Lua, and the engine quits by
itself.

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
# start and a cube wide, the player a box 40 wide, so it comes to a halt about 20 units short of the pillar's south face.
PILLAR_FACE = plugin.PILLAR_DISTANCE - plugin.CUBE / 2
STOP_RANGE = (PILLAR_FACE - 60.0, PILLAR_FACE)


class Scenario:
    """One start of the engine: the text after --start, how the engine names the cell in its log, whether the cell is an
    exterior one, where in the world the player starts (the entry marker of the cell) and whether the cell has the
    lighting that the plugin gives the interior cell."""

    def __init__(self, name, start, loaded, exterior, origin, lit):
        self.name, self.start, self.loaded, self.exterior, self.origin, self.lit = (
            name, start, loaded, exterior, origin, lit)


SCENARIOS = [
    Scenario("interior", plugin.CELL_NAME, plugin.CELL_NAME, False, (0.0, 0.0), True),
    Scenario("exterior", f"{plugin.WORLD_NAME}:0,0", f"{plugin.WORLD_NAME}Cell (0, 0)", True,
             (plugin.EXTERIOR_CELL_SIZE / 2, plugin.EXTERIOR_CELL_SIZE / 2), False),
]

WALK_SECONDS = 12

# What the engine logs about the lighting of the cell: its own ambient and sun colours and fog near distance, and the
# fog colour and far distance that it takes from its lighting template.
LIGHTING_LINE = ("Cell lighting: ambient {}, directional {}, fog {}, fog range {:.6f} to {:.6f}".format(
    *(",".join(map(str, colour)) for colour in (plugin.AMBIENT, plugin.DIRECTIONAL, plugin.TEMPLATE_FOG)),
    plugin.FOG_NEAR, plugin.TEMPLATE_FOG_FAR))

# Height of the camera above the feet of the player, in game units: the head node of the placeholder skeleton is at 124.
EYE_HEIGHT = (100.0, 140.0)

# Waits for the player to settle, then walks north (the player faces north at the start) and logs once a second.
WALK_SCRIPT = """\
local self = require('openfallout.self')
local nearby = require('openfallout.nearby')
local util = require('openfallout.util')
local core = require('openfallout.core')
local I = require('openfallout.interfaces')
local camera = require('openfallout.camera')

local elapsed = 0
local nextLog = 0
local rayDone = false
local function log(...) print('OFTEST', ...) end
local function fmt(v) return string.format('%%.1f,%%.1f,%%.1f', v.x, v.y, v.z) end

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            if elapsed >= 3 then
                I.Controls.overrideMovementControls(true)
                self.controls.movement = 1
                self.controls.run = true
                self.controls.yawChange = 0
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
            end
            if elapsed >= %d then core.quit() end
        end,
    },
}
""" % WALK_SECONDS

NUMBER = r"(-?[\d.]+)"
VECTOR = ",".join([NUMBER] * 3)
SAMPLE = re.compile(r"OFTEST\tt=([\d.]+) exterior=(\w+) name=(.*) pos=" + VECTOR + " cam=" + VECTOR)
RAY = re.compile(r"OFTEST\tray down hit=\t(\w+)\t(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)")


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


def run_engine(program, resources, data, home, runtime, seconds, start):
    env = dict(os.environ, HOME=str(home), XDG_RUNTIME_DIR=str(runtime), LIBGL_ALWAYS_SOFTWARE="1")
    for name in ("XDG_CONFIG_HOME", "XDG_DATA_HOME"):
        env.pop(name, None)
    command = [str(program), "--resources", str(resources), "--data", str(data), "--content", "OFTest.esm",
               "--content", "walktest.omwscripts", "--skip-menu", "--start", start, "--no-grab"]
    # The engine shows a dialog instead of logging a fatal error when stdin is not a terminal, so give it one.
    master, slave = pty.openpty()
    process = subprocess.Popen(command, stdin=slave, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    os.close(slave)
    try:
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
    return process.returncode


def check(text, scenario):
    """The problems the log shows, as a list of sentences; empty when the run did what it should."""
    problems = []
    for line in text.splitlines():
        if re.search(r"Can't start|Failed to start|Lua error|Fatal error|Failed to load Lua", line, re.I):
            problems.append(line)
    if "using placeholder records" not in text:
        problems.append("the engine did not make placeholder records, so the content had a player record")
    if f"Loading cell {scenario.loaded}" not in text:
        problems.append(f"the engine did not load the cell {scenario.loaded}")
    if scenario.lit and LIGHTING_LINE not in text:
        lines = [line.split("]", 1)[-1].strip() for line in text.splitlines() if "Cell lighting" in line]
        problems.append(f"the log has no line '{LIGHTING_LINE}', the lighting of the cell is: {lines or 'not logged'}")
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
    x, y, z = samples[-1][3]
    origin_x, origin_y = scenario.origin
    if not origin_y + STOP_RANGE[0] <= y <= origin_y + STOP_RANGE[1]:
        problems.append(f"the player ended at y={y:.1f}, expected {origin_y + STOP_RANGE[0]:.0f} to "
                        f"{origin_y + STOP_RANGE[1]:.0f} (stopped by the pillar)")
    if abs(x - origin_x) > 5.0:
        problems.append(f"the player drifted sideways to x={x:.1f}")
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
        status = run_engine(program, build / "resources", data, home, runtime, args.seconds, scenario.start)

        log = home / ".config" / "openfallout" / "openfallout.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(f"log: {log}")
        found = []
        if status != 0:
            found.append(f"the engine exited with status {status}")
        found += check(text, scenario)
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
