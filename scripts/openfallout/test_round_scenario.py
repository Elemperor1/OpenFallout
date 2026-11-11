#!/usr/bin/env python3
"""Tests of the scenario player of the rounds (round/scripts/roundworld.lua and roundplayer.lua), run with lupa (skipped where it
is not installed) against stand-ins of the engine's Lua modules: the order of the steps, the waits, the answers of the player
script, the end of the scenario, its errors and the time limit."""
import unittest
from pathlib import Path

try:
    from lupa.luajit21 import LuaRuntime
except ImportError:  # pragma: no cover
    try:
        from lupa import LuaRuntime
    except ImportError:
        LuaRuntime = None

SCRIPTS = Path(__file__).resolve().parent / "round" / "scripts"

PRELUDE = """
clock = 0
printed = {}
quit_called = false
paused = false
sent_to_player = {}
globals_table = { GameHour = 8 }
teleports = {}
object_cells = {}
real_print = print
print = function(...)
    local parts = {}
    for i = 1, select('#', ...) do parts[#parts + 1] = tostring((select(i, ...))) end
    printed[#printed + 1] = table.concat(parts, '\\t')
end
local player = {
    position = { x = 10, y = 20, z = 30 },
    cell = { isExterior = true, gridX = 0, gridY = 0, worldSpaceId = 'w' },
    sendEvent = function(self, name, data) sent_to_player[#sent_to_player + 1] = { name = name, data = data } end,
    teleport = function(self, cell, pos, options) teleports[#teleports + 1] = { cell = cell, pos = pos, options = options } end,
}
package.preload['openfallout.core'] = function()
    return {
        getRealTime = function() return clock end,
        quit = function() quit_called = true end,
        isWorldPaused = function() return paused end,
        getFormId = function(file, id) return 'FormId:' .. file .. ':' .. id end,
    }
end
package.preload['openfallout.util'] = function()
    return { vector3 = function(x, y, z) return { x = x, y = y, z = z } end,
             transform = { rotateZ = function(a) return { yaw = a } end } }
end
package.preload['openfallout.world'] = function()
    return {
        players = { player },
        globals = globals_table,
        quests = { find = function(name) if name == 'Q' then return { stage = 5, running = true, setStage = function(self, s) self.stage = s end } end end },
        getExteriorCell = function(gx, gy, ref) return { grid = gx .. ',' .. gy, via = ref } end,
        getCellByName = function(name) return { name = name } end,
        getCellById = function(id) return { id = id } end,
        getObjectByFormId = function(id) return { id = id, recordId = 'rec', cell = object_cells[id] or player.cell,
                                                   position = player.position, enabled = true, activateBy = function() end } end,
    }
end
function load_world(scenario_source, ...)
    package.loaded['scripts.roundscenario'] = nil
    package.preload['scripts.roundscenario'] = load(scenario_source)
    return load(world_source)()
end
"""


def make_runtime(world_source):
    lua = LuaRuntime(unpack_returned_tuples=True)
    lua.globals().world_source = world_source
    lua.execute(PRELUDE)
    return lua


@unittest.skipUnless(LuaRuntime, "needs lupa")
class ScenarioPlayer(unittest.TestCase):
    SOURCE = (SCRIPTS / "roundworld.lua").read_text(encoding="utf-8")

    def start(self, scenario):
        self.lua = make_runtime(self.SOURCE)
        self.module = self.lua.eval("function(source) return load_world(source) end")(scenario)
        return self.module

    def tick(self, seconds, answer=True):
        """Advances the clock and runs one update; the player script is played by this test: it answers each question."""
        self.lua.execute(f"clock = clock + {seconds}")
        self.module.engineHandlers.onUpdate(0.016)
        if answer:
            sent = self.lua.eval("sent_to_player")
            for index in range(1, len(sent) + 1):
                event = sent[index]
                self.module.eventHandlers.RoundAnswer(self.lua.table_from({"id": event.data.id, "result": self.lua.table_from({})}))
            self.lua.execute("sent_to_player = {}")

    def lines(self):
        printed = self.lua.eval("printed")
        return [printed[i] for i in range(1, len(printed) + 1)]

    def test_waits_and_ends_and_quits_after_the_screenshots_are_written(self):
        self.start("return function(r) r.log('a') r.wait(5) r.log('b') end")
        self.tick(1)                       # the delay before the first step is 3 s
        self.assertEqual(self.lines(), [])
        self.tick(3)
        self.assertIn("ROUND\tbegin\treal=4.0", self.lines())
        self.assertIn("ROUND\tnote\ta", self.lines())
        self.assertNotIn("ROUND\tnote\tb", self.lines())
        self.tick(4)
        self.assertNotIn("ROUND\tnote\tb", self.lines())
        self.tick(2)
        self.assertIn("ROUND\tnote\tb", self.lines())
        self.assertTrue(any(line.startswith("ROUND\tdone\tend") for line in self.lines()))
        self.assertFalse(self.lua.eval("quit_called"))
        self.tick(1)
        self.assertFalse(self.lua.eval("quit_called"))
        self.tick(1)
        self.assertTrue(self.lua.eval("quit_called"))

    def test_the_world_must_not_be_paused(self):
        self.start("return function(r) r.log('x') end")
        self.lua.execute("paused = true")
        self.tick(10)
        self.assertEqual(self.lines(), [])
        self.lua.execute("paused = false")
        self.tick(1)
        self.assertIn("ROUND\tnote\tx", self.lines())

    def test_a_question_waits_for_the_answer(self):
        self.start("return function(r) r.eye('here') r.log('after') end")
        self.tick(4, answer=False)
        self.assertEqual(self.lua.eval("#sent_to_player"), 1)
        self.assertEqual(self.lua.eval("sent_to_player[1].name"), "RoundAsk")
        self.assertEqual(self.lua.eval("sent_to_player[1].data.what"), "eye")
        self.assertNotIn("ROUND\tnote\tafter", self.lines())
        self.tick(1)                       # the answer arrives between two frames, the next frame goes on
        self.assertNotIn("ROUND\tnote\tafter", self.lines())
        self.tick(0.1)
        self.assertIn("ROUND\tnote\tafter", self.lines())

    def test_a_question_that_is_not_answered_times_out(self):
        self.start("return function(r) r.eye('here') r.log('after') end")
        self.tick(4, answer=False)
        self.tick(21, answer=False)
        self.assertTrue(any("the player script did not answer" in line for line in self.lines()))
        self.assertIn("ROUND\tnote\tafter", self.lines())

    def test_shot_waits_a_little_for_the_file(self):
        self.start("return function(r) r.shot('s') r.log('after') end")
        self.tick(4)
        self.tick(0.1)                     # the answer is taken, the 0.6 s of the file begin
        self.assertNotIn("ROUND\tnote\tafter", self.lines())
        self.tick(0.7)
        self.assertIn("ROUND\tnote\tafter", self.lines())

    def test_teleport_in_the_open_and_into_a_cell(self):
        self.start("return function(r) r.teleport{ pos = { 5000, 9000 }, onGround = true, yaw = 1.5, label = 'x' } "
                   "r.teleport{ cell = 'Vault', pos = { 1, 2, 3 } } end")
        self.tick(4)
        self.assertEqual(self.lua.eval("#teleports"), 2)
        self.assertEqual(self.lua.eval("teleports[1].cell.grid"), "1,2")
        self.assertEqual(self.lua.eval("teleports[1].pos.z"), 30)       # the height of the player when none is given
        self.assertTrue(self.lua.eval("teleports[1].options.onGround"))
        self.assertEqual(self.lua.eval("teleports[1].options.rotation.yaw"), 1.5)
        self.assertEqual(self.lua.eval("teleports[2].cell.name"), "Vault")
        self.assertFalse(self.lua.eval("teleports[2].options.onGround"))

    def test_a_step_that_fails_is_an_error_line(self):
        # round.py fails the round on a line that starts with `error`
        self.start("return function(r) "
                   "r.world.getCellByName = function() error('no such cell') end "
                   "local ok = r.teleport{ cell = 'Nowhere', pos = { 1, 2, 3 }, label = 'there' } "
                   "r.world.globals = nil r.hour(14) "
                   "r.quest('missing', 3) r.log(tostring(ok)) end")
        self.tick(4)
        lines = self.lines()
        self.assertTrue(any(line.startswith("ROUND\terror\tteleport\t") and "no such cell" in line and "\tthere\t" in line
                            for line in lines))
        self.assertTrue(any(line.startswith("ROUND\terror\thour\t") for line in lines))
        self.assertTrue(any(line.startswith("ROUND\terror\tquest\tmissing\t") for line in lines))
        self.assertIn("ROUND\tnote\tfalse", lines)
        self.assertFalse(any(line.startswith("ROUND\tteleport\t") for line in lines))

    MOUNTAINS = (SCRIPTS.parent / "scenarios" / "m13_mountains.lua").read_text(encoding="utf-8")

    def run_mountains(self, setup=""):
        self.start(self.MOUNTAINS)
        self.lua.execute(setup or "")
        self.tick(4)
        for _ in range(12):
            self.tick(13)
        return self.lines()

    def test_the_mountain_scenario_goes_to_the_mojave_from_an_interior(self):
        # the save is in an interior; a reference of the Mojave names the worldspace
        lines = self.run_mountains(
            "require('openfallout.world').players[1].cell = { isExterior = false, id = 'inside', worldSpaceId = 'inside' }\n"
            "object_cells['FormId:FalloutNV.esm:' .. 0xcdf51] = { isExterior = true, worldSpaceId = 'mojave', gridX = 1, gridY = 2 }")
        self.assertFalse(any(line.startswith("ROUND\terror") for line in lines), lines)
        self.assertIn("ROUND\tnote\tanchor\t0xcdf51\tworldspace=mojave exterior=true", lines)
        self.assertEqual(self.lua.eval("#teleports"), 2)
        self.assertEqual(self.lua.eval("teleports[1].cell.via.worldSpaceId"), "mojave")
        self.assertEqual(self.lua.eval("teleports[2].cell.via.worldSpaceId"), "mojave")
        self.assertEqual((self.lua.eval("teleports[1].pos.x"), self.lua.eval("teleports[1].pos.z")), (-69319, 8381))

    def test_the_mountain_scenario_tries_the_next_reference_and_then_the_cell_of_the_player(self):
        lines = self.run_mountains(
            "object_cells['FormId:FalloutNV.esm:' .. 0xcdf51] = { isExterior = false, id = 'a house' }\n"
            "object_cells['FormId:FalloutNV.esm:' .. 0x10a201] = { isExterior = true, worldSpaceId = 'second' }")
        self.assertIn("ROUND\tnote\tanchor\t0xcdf51\tworldspace=nil exterior=false", lines)
        self.assertEqual(self.lua.eval("teleports[1].cell.via.worldSpaceId"), "second")
        # no reference found: the player is in the open, so its own cell names the worldspace
        lines = self.run_mountains("require('openfallout.world').getObjectByFormId = function() return nil end")
        self.assertIn("ROUND\tnote\tanchor\t0xcdf51\tnot found", lines)
        self.assertIn("ROUND\tnote\tanchor\tplayer cell\tworldspace=w", lines)
        self.assertFalse(any(line.startswith("ROUND\terror") for line in lines), lines)
        self.assertEqual(self.lua.eval("#teleports"), 2)

    def test_the_mountain_scenario_stops_when_the_worldspace_cannot_be_found(self):
        lines = self.run_mountains(
            "require('openfallout.world').getObjectByFormId = function() return nil end\n"
            "require('openfallout.world').players[1].cell = { isExterior = false, id = 'inside' }")
        self.assertTrue(any(line.startswith("ROUND\terror\tscenario") and "worldspace" in line for line in lines), lines)
        self.assertEqual(self.lua.eval("#teleports"), 0)

    def test_hour_global_and_quest(self):
        self.start("return function(r) r.hour(14) r.global('GameHour') r.quest('Q', 9) r.quest('missing') "
                   "r.object('FormId:0x1', 'thing') end")
        self.tick(4)
        self.assertEqual(self.lua.eval("globals_table.GameHour"), 14)
        lines = self.lines()
        self.assertIn("ROUND\thour\tok\t14.00", lines)
        self.assertIn("ROUND\tglobal\tGameHour\t14", lines)
        self.assertIn("ROUND\tquest\tQ\tstage=9 running=true", lines)
        self.assertIn("ROUND\tquest\tmissing\tmissing", lines)
        self.assertTrue(any(line.startswith("ROUND\tobject\tthing\t") and "record=rec" in line for line in lines))

    def test_wait_for_a_condition(self):
        self.start("return function(r) local ok = r.waitFor(function() return globals_table.Done end, 10, 'cond') r.log(tostring(ok)) end")
        self.tick(4)
        self.tick(2)
        self.assertNotIn("ROUND\tnote\ttrue", self.lines())
        self.lua.execute("globals_table.Done = true")
        self.tick(1)
        self.assertIn("ROUND\tnote\ttrue", self.lines())

    def test_wait_for_times_out(self):
        self.start("return function(r) local ok = r.waitFor(function() return false end, 3, 'never') r.log(tostring(ok)) end")
        self.tick(4)
        self.tick(4)
        self.assertIn("ROUND\tnote\tfalse", self.lines())
        self.assertTrue(any(line.startswith("ROUND\twaitFor\tnever\ttimeout") for line in self.lines()))

    def test_an_error_in_the_scenario_ends_it(self):
        self.start("return function(r) r.log('before') error('boom') end")
        self.tick(4)
        lines = self.lines()
        self.assertTrue(any(line.startswith("ROUND\terror\tscenario") and "boom" in line for line in lines))
        self.assertTrue(any(line.startswith("ROUND\tdone\terror") for line in lines))

    def test_the_time_limit(self):
        self.start("return { limit = 20, run = function(r) r.wait(1000) end }")
        self.tick(4)
        self.tick(20)
        self.assertTrue(any(line.startswith("ROUND\terror\ttime limit") for line in self.lines()))
        self.assertTrue(any(line.startswith("ROUND\tdone\tlimit") for line in self.lines()))
        self.tick(2)
        self.assertTrue(self.lua.eval("quit_called"))

    def test_without_a_scenario_it_ends(self):
        self.lua = make_runtime(self.SOURCE)
        self.module = self.lua.eval("function() package.preload['scripts.roundscenario'] = nil; return load(world_source)() end")()
        self.tick(4)
        self.assertTrue(any("no scenario" in line for line in self.lines()))


PLAYER_PRELUDE = """
clock = 0
printed, takeScreenshot_calls, answers = {}, 0, {}
print = function(...)
    local parts = {}
    for i = 1, select('#', ...) do parts[#parts + 1] = tostring((select(i, ...))) end
    printed[#printed + 1] = table.concat(parts, '\\t')
end
local vec = function(x, y, z) return setmetatable({ x = x, y = y, z = z }, { __add = function(a, b) return vec(a.x + b.x, a.y + b.y, a.z + b.z) end,
    __sub = function(a, b) return vec(a.x - b.x, a.y - b.y, a.z - b.z) end }) end
yaw, pitch = 0, 0
package.preload['openfallout.core'] = function()
    return { getRealTime = function() return clock end, getRealFrameDuration = function() return 0.02 end,
             sendGlobalEvent = function(name, data) answers[#answers + 1] = { name = name, data = data } end }
end
package.preload['openfallout.self'] = function()
    return { position = vec(100, 200, 300), cell = { isExterior = true, gridX = 1, gridY = 2, worldSpaceId = 'w' }, object = {} }
end
package.preload['openfallout.camera'] = function()
    return { getPosition = function() return vec(100, 200, 364) end, getYaw = function() return yaw end, getPitch = function() return pitch end,
             setYaw = function(v) yaw = v end, setPitch = function(v) pitch = v end, getMode = function() return 1 end,
             setMode = function() end, MODE = { FirstPerson = 1 } }
end
package.preload['openfallout.debug'] = function() return { takeScreenshot = function() takeScreenshot_calls = takeScreenshot_calls + 1 end } end
package.preload['openfallout.nearby'] = function()
    return { COLLISION_TYPE = { HeightMap = 4 }, actors = {},
             castRay = function(from, to, options) return { hit = true, hitPos = vec(from.x, from.y, 55) } end }
end
package.preload['openfallout.util'] = function() return { vector3 = vec } end
function load_player() return load(player_source)() end
"""


@unittest.skipUnless(LuaRuntime, "needs lupa")
class PlayerScript(unittest.TestCase):
    def setUp(self):
        self.lua = LuaRuntime(unpack_returned_tuples=True)
        self.lua.globals().player_source = (SCRIPTS / "roundplayer.lua").read_text(encoding="utf-8")
        self.lua.execute(PLAYER_PRELUDE)
        self.module = self.lua.eval("load_player")()

    def ask(self, what, **args):
        self.module.eventHandlers.RoundAsk(self.lua.table_from({"id": 7, "what": what, "args": self.lua.table_from(args)}))

    def lines(self):
        printed = self.lua.eval("printed")
        return [printed[i] for i in range(1, len(printed) + 1)]

    def test_shot_counts_and_names(self):
        self.ask("shot", name="a")
        self.ask("shot", name="b")
        self.assertEqual(self.lua.eval("takeScreenshot_calls"), 2)
        self.assertEqual(self.lines(), ["ROUND\tshot\ta\tn=1", "ROUND\tshot\tb\tn=2"])
        self.assertEqual(self.lua.eval("answers[1].data.id"), 7)

    def test_look_eye_and_ground(self):
        self.ask("look", yaw=1.5, pitch=-0.25)
        self.assertEqual(self.lua.eval("yaw"), 1.5)
        self.ask("eye", label="p")
        self.assertTrue(any(line.startswith("ROUND\teye\tp\tcell=w:1,2") and "yaw=1.50" in line for line in self.lines()))
        self.ask("ground", label="g", offsets=self.lua.table_from({1: self.lua.table_from({1: 64, 2: 0})}))
        self.assertIn("ROUND\tground\tg\tdx=0 dy=0\tz=55", self.lines())
        self.assertIn("ROUND\tground\tg\tdx=64 dy=0\tz=55", self.lines())

    def test_fps_reads_the_recent_frames(self):
        for _ in range(5):
            self.lua.execute("clock = clock + 0.02")
            self.module.engineHandlers.onUpdate()
        self.ask("fps", label="f", seconds=1)
        self.assertTrue(any(line.startswith("ROUND\tfps\tf\tframes=5 mean=20.0ms") for line in self.lines()))

    def test_unknown_question_is_answered_with_an_error(self):
        self.ask("nonsense")
        self.assertTrue(any(line.startswith("ROUND\terror\tnonsense") for line in self.lines()))
        self.assertEqual(self.lua.eval("#answers"), 1)


if __name__ == "__main__":
    unittest.main()
