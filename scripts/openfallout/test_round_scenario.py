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
asked_log = {}
globals_table = { GameHour = 8 }
teleports = {}
object_cells = {}
real_print = print
print = function(...)
    local parts = {}
    for i = 1, select('#', ...) do parts[#parts + 1] = tostring((select(i, ...))) end
    printed[#printed + 1] = table.concat(parts, '\\t')
end
local vector_methods = {}
local vec3
vec3 = function(x, y, z)
    return setmetatable({ x = x, y = y, z = z }, {
        __add = function(a, b) return vec3(a.x + b.x, a.y + b.y, a.z + b.z) end,
        __sub = function(a, b) return vec3(a.x - b.x, a.y - b.y, a.z - b.z) end,
        __mul = function(a, k) return vec3(a.x * k, a.y * k, a.z * k) end,
    })
end
local player = {
    position = vec3(10, 20, 30),
    cell = { isExterior = true, gridX = 0, gridY = 0, worldSpaceId = 'w' },
    sendEvent = function(self, name, data)
        sent_to_player[#sent_to_player + 1] = { name = name, data = data }
        asked_log[#asked_log + 1] = data.what
    end,
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
    return { vector3 = vec3, transform = { rotateZ = function(a) return { yaw = a } end } }
end
created = {}
package.preload['openfallout.types'] = function()
    return { Actor = { inventory = function(actor) return { owner = actor } end } }
end
package.preload['openfallout.world'] = function()
    return {
        players = { player },
        globals = globals_table,
        quests = { find = function(name)
            if name == 'Q' then return { stage = 5, running = true, setStage = function(self, s) self.stage = s end } end
            -- setStage answers false for a stage the quest does not have, and false for a stage that was done already
            if name == 'NoStage' then return { stage = 5, running = true, setStage = function(self, s) return false end } end
            if name == 'Done' then return { stage = 20, running = true, setStage = function(self, s) return false end } end
        end },
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

    def test_the_game_waits_long_to_quit_when_pictures_are_to_be_written(self):
        # two seconds when there is no picture; thirty when there are (round.py stops the game as soon as they are saved)
        self.start("return function(r) r.shot('a') r.shot('b') end")
        for _ in range(20):
            self.tick(1)
            if any(line.startswith("ROUND\tdone\tend") for line in self.lines()):
                break
        self.assertTrue(any(line.startswith("ROUND\tdone\tend") for line in self.lines()))
        self.assertFalse(self.lua.eval("quit_called"))
        self.tick(29)
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

    def play_player(self, scenario, reply, ticks=60, step=0.3):
        """Runs a scenario with a player script played by the test: `reply(what)` gives the result table for each question
        (a dict); gives the questions in the order they were asked."""
        self.start(scenario)
        seen = []
        self.tick(3.5, answer=False)
        for _ in range(ticks):
            sent = self.lua.eval("sent_to_player")
            events = [sent[index] for index in range(1, len(sent) + 1)]
            self.lua.execute("sent_to_player = {}")
            for event in events:
                seen.append(event.data.what)
                self.module.eventHandlers.RoundAnswer(
                    self.lua.table_from({"id": event.data.id, "result": self.lua.table_from(reply(event.data.what))}))
            self.tick(step, answer=False)
        return seen

    def test_a_shot_waits_for_the_message_boxes_to_come_to_rest(self):
        replies = [dict(open=True), dict(open=False, quiet=1.0), dict(open=False, quiet=3.5), dict(open=False, quiet=6.5)]
        seen = self.play_player("return function(r) r.shot('s') r.log('after') end",
                                lambda what: replies.pop(0) if what == "quiet" and replies else {})
        self.assertEqual(seen, ["quiet", "quiet", "quiet", "quiet", "shot"])
        self.assertIn("ROUND\tnote\tafter", self.lines())

    def test_settle_gives_up_after_its_timeout_and_the_scenario_goes_on(self):
        seen = self.play_player("return function(r) r.log(tostring(r.settle(3, 2))) r.log('after') end",
                                lambda what: dict(open=True), ticks=30)
        self.assertGreater(len(seen), 3)
        self.assertTrue(any(line.startswith("ROUND\tsettle\ttimeout") for line in self.lines()))
        self.assertIn("ROUND\tnote\tfalse", self.lines())
        self.assertIn("ROUND\tnote\tafter", self.lines())
        self.assertFalse(any(line.startswith("ROUND\terror") for line in self.lines()))

    def test_a_shot_without_settling_does_not_ask_about_boxes(self):
        seen = self.play_player("return function(r) r.shot('s', false) end", lambda what: {}, ticks=6)
        self.assertEqual(seen, ["shot"])

    def test_shot_waits_a_little_for_the_file(self):
        self.start("return function(r) r.shot('s', false) r.log('after') end")
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

    INSIDE = "require('openfallout.world').players[1].cell = { isExterior = false, id = 'inside', worldSpaceId = 'inside' }\n"

    def test_the_mountain_scenario_goes_to_the_mojave_from_an_interior(self):
        # the save is in an interior; a cell of the Mojave in the record store names the worldspace
        lines = self.run_mountains(
            self.INSIDE +
            "require('openfallout.world').getCellById = function(id) return { id = id, isExterior = true, worldSpaceId = 'mojave' } end")
        self.assertFalse(any(line.startswith("ROUND\terror") for line in lines), lines)
        self.assertIn("ROUND\tnote\tanchor\tcell 0xdaeb9\tworldspace=mojave exterior=true", lines)
        self.assertEqual(self.lua.eval("#teleports"), 2)
        self.assertEqual(self.lua.eval("teleports[1].cell.via.worldSpaceId"), "mojave")
        self.assertEqual(self.lua.eval("teleports[2].cell.via.worldSpaceId"), "mojave")
        self.assertEqual((self.lua.eval("teleports[1].pos.x"), self.lua.eval("teleports[1].pos.z")), (-69319, 8381))

    def test_the_mountain_scenario_goes_on_to_the_next_cell_and_to_the_references(self):
        # a cell that cannot be read, then one that is an interior, then a reference whose cell is not loaded (reading its cell
        # raises an error, as the engine does) and one that is
        lines = self.run_mountains(
            self.INSIDE +
            "local world = require('openfallout.world')\n"
            "local cells = { [0xdaeb9] = function() error('Cell is not available') end,\n"
            "               [0xdaeb8] = function() return { isExterior = false, id = 'a house' } end }\n"
            "world.getCellById = function(id) local f = cells[tonumber(id:match(':(%d+)$'))] if f then return f() end end\n"
            "local refs = { [0xcdf51] = function() error('Object is not available: 0x10cdf51') end,\n"
            "              [0x10a201] = function() return { isExterior = true, worldSpaceId = 'second' } end }\n"
            "world.getObjectByFormId = function(id) local n = tonumber(id:match(':(%d+)$'))\n"
            "    return setmetatable({}, { __index = function(_, key) if key == 'cell' then return refs[n]() end end }) end")
        self.assertFalse(any(line.startswith("ROUND\terror") for line in lines), lines)
        self.assertTrue(any(line.startswith("ROUND\tnote\tanchor\tcell 0xdaeb9\terror ") for line in lines), lines)
        self.assertIn("ROUND\tnote\tanchor\tcell 0xdaeb8\tworldspace=nil exterior=false", lines)
        self.assertTrue(any(line.startswith("ROUND\tnote\tanchor\tref 0xcdf51\terror ") and "Object is not available" in line
                            for line in lines), lines)
        self.assertIn("ROUND\tnote\tanchor\tref 0x10a201\tworldspace=second exterior=true", lines)
        self.assertEqual(self.lua.eval("teleports[1].cell.via.worldSpaceId"), "second")
        self.assertEqual(self.lua.eval("#teleports"), 2)

    def test_the_mountain_scenario_falls_back_to_the_cell_of_a_player_in_the_open(self):
        lines = self.run_mountains(
            "local world = require('openfallout.world')\n"
            "world.getCellById = function() return nil end\n"
            "world.getObjectByFormId = function() return nil end")
        self.assertIn("ROUND\tnote\tanchor\tcell 0xdaeb9\tnot found", lines)
        self.assertIn("ROUND\tnote\tanchor\tplayer cell\tworldspace=w", lines)
        self.assertFalse(any(line.startswith("ROUND\terror") for line in lines), lines)
        self.assertEqual(self.lua.eval("#teleports"), 2)

    def test_the_mountain_scenario_stops_when_the_worldspace_cannot_be_found(self):
        lines = self.run_mountains(
            self.INSIDE +
            "require('openfallout.world').getObjectByFormId = function() return nil end")
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

    def test_a_quest_stage_that_is_not_applied_is_an_error_line(self):
        self.start("return function(r) r.quest('NoStage', 7) r.quest('Done', 20) r.log('after') end")
        self.tick(4)
        lines = self.lines()
        # setStage answers false (it does not throw) for a stage the quest does not have: a failed step, and the scenario goes on
        self.assertTrue(any(line.startswith("ROUND\terror\tquest\tNoStage\t") and "stage 7" in line and "at stage 5" in line
                            for line in lines))
        # a stage that is done already and the quest stands at it: nothing to apply, no error
        self.assertIn("ROUND\tquest\tDone\tstage=20 running=true", lines)
        self.assertIn("ROUND\tnote\tafter", lines)

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

    def test_the_player_is_0x14_and_placed_references_are_written_as_form_ids(self):
        # the combat calls name the player 0x14 (the games' own number), and a placed reference "0x..." as a form id
        self.start("return function(r) r.log(r.form(r.player()), r.form({ id = '0x1629a4' }), r.form({ id = 'FormId:0x55' }), "
                   "r.form(nil) == '') end")
        self.tick(4)
        self.assertIn("ROUND\tnote\tFormId:0x14\tFormId:0x1629a4\tFormId:0x55\ttrue", self.lines())

    def test_fight_starts_it_between_two_objects_and_says_the_state(self):
        self.start("return function(r) "
                   "local calls = {} "
                   "r.core.combat = { start = function(a, b) calls[#calls + 1] = a .. '>' .. b end, "
                   "inCombat = function() return true end, target = function() return 'FormId:0x14' end } "
                   "r.fight({ id = '0x1629a4' }, r.player(), 'g1') "
                   "r.log(calls[1]) end")
        self.tick(4)
        lines = self.lines()
        self.assertIn("ROUND\tfight\tok\tg1\tinCombat=true target=FormId:0x14", lines)
        self.assertIn("ROUND\tnote\tFormId:0x1629a4>FormId:0x14", lines)

    def test_place_puts_an_object_in_a_direction_and_faces_the_player(self):
        self.start("return function(r) "
                   "local o = { teleport = function(self, cell, pos, options) teleports[#teleports + 1] = { pos = pos, options = options } end } "
                   "r.place(o, { dist = 700, bearing = 0, label = 'v' }) "
                   "r.place(o, { dist = 100, bearing = math.pi / 2, yaw = 2 }) end")
        self.tick(4)
        self.assertEqual(self.lua.eval("#teleports"), 2)
        self.assertAlmostEqual(self.lua.eval("teleports[1].pos.y"), 20 + 700)   # north of the player at (10, 20)
        self.assertAlmostEqual(self.lua.eval("teleports[1].pos.x"), 10)
        self.assertAlmostEqual(self.lua.eval("teleports[1].options.rotation.yaw"), 3.14159265, places=5)   # it looks back at the player
        self.assertAlmostEqual(self.lua.eval("teleports[2].pos.x"), 110)       # east
        self.assertEqual(self.lua.eval("teleports[2].options.rotation.yaw"), 2)
        self.assertTrue(self.lua.eval("teleports[2].options.onGround"))
        self.assertIn("ROUND\tplace\tok\tv\tdist=700 bearing=0.00", self.lines())

    def test_put_at_goes_to_the_cell_of_the_position(self):
        self.start("return function(r) "
                   "local o = { teleport = function(self, cell, pos, options) teleports[#teleports + 1] = { cell = cell, pos = pos, options = options } end } "
                   "r.putAt(o, { pos = { 8200, -4100, 5 }, yaw = 1, label = 'there' }) end")
        self.tick(4)
        self.assertEqual(self.lua.eval("teleports[1].cell.grid"), "2,-2")
        self.assertEqual(self.lua.eval("teleports[1].pos.z"), 5)
        self.assertIn("ROUND\tputAt\tok\tthere\tpos=8200,-4100", self.lines())

    def test_the_questions_of_the_fight_go_to_the_player_script(self):
        self.start("return function(r) r.drawn(true) r.fire('x', nil) r.vatsInfo('v', nil) r.state('s', nil) end")
        self.tick(4)
        for _ in range(8):
            self.tick(0.5)
        log = self.lua.eval("asked_log")
        self.assertEqual([log[index] for index in range(1, len(log) + 1)], ["drawn", "fire", "vatsInfo", "actorState"])
        self.assertTrue(any(line.startswith("ROUND\tbegin") for line in self.lines()))
        self.assertTrue(any(line.startswith("ROUND\tdone\tend") for line in self.lines()))

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
local vector_methods = {
    length = function(a) return math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z) end,
    normalize = function(a) local n = math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z) return vec(a.x / n, a.y / n, a.z / n) end,
    dot = function(a, b) return a.x * b.x + a.y * b.y + a.z * b.z end,
}
vec = function(x, y, z) return setmetatable({ x = x, y = y, z = z }, { __add = function(a, b) return vec(a.x + b.x, a.y + b.y, a.z + b.z) end,
    __sub = function(a, b) return vec(a.x - b.x, a.y - b.y, a.z - b.z) end, __index = vector_methods }) end
function make_actor(x, y, z) return { position = vec(x, y, z), recordId = 'ganger' } end
yaw, pitch = 0, 0
package.preload['openfallout.core'] = function()
    return { getRealTime = function() return clock end, getRealFrameDuration = function() return 0.0001 end,
             sendGlobalEvent = function(name, data) answers[#answers + 1] = { name = name, data = data } end }
end
package.preload['openfallout.self'] = function()
    return { position = vec(100, 200, 300), cell = { isExterior = true, gridX = 1, gridY = 2, worldSpaceId = 'w' }, object = {} }
end
package.preload['openfallout.camera'] = function()
    return { getPosition = function() return vec(100, 200, 364) end, getYaw = function() return yaw end, getPitch = function() return pitch end,
             setYaw = function(v) yaw = v end, setPitch = function(v) pitch = v end, getMode = function() return 1 end,
             setMode = function() end, MODE = { FirstPerson = 1 }, viewportToWorldVector = function() return vec(0, 1, 0) end }
end
package.preload['openfallout.debug'] = function() return { takeScreenshot = function() takeScreenshot_calls = takeScreenshot_calls + 1 end } end
package.preload['openfallout.nearby'] = function()
    return { COLLISION_TYPE = { HeightMap = 4 }, actors = {},
             castRay = function(from, to, options) return { hit = true, hitPos = vec(from.x, from.y, 55) } end }
end
package.preload['openfallout.util'] = function() return { vector3 = vec, vector2 = function(x, y) return { x = x, y = y } end } end
package.preload['openfallout.input'] = function() return { CONTROLLER_BUTTON = { Y = 12, X = 11 } } end
package.preload['openfallout.animation'] = function() return { BONE_GROUP = {} } end
package.preload['openfallout.types'] = function() return { Actor = {} } end
package.preload['openfallout.interfaces'] = function()
    return { UI = { getMode = function() return 'none' end }, Controls = { overrideMovementControls = function() end } }
end
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
        for _ in range(6):
            self.lua.execute("clock = clock + 0.02")
            self.module.engineHandlers.onUpdate()
        self.ask("fps", label="f", seconds=1)
        self.assertTrue(any(line.startswith("ROUND\tfps\tf\tframes=5 mean=20.0ms") for line in self.lines()))

    def test_fps_is_measured_from_real_time_and_not_from_the_engine_frame_duration(self):
        # one frame of 350 ms: the engine's frame duration is clamped (200 ms) and scaled by the world speed, the clock is not
        for step in (0.02, 0.02, 0.35, 0.02):
            self.lua.execute("clock = clock + %s" % step)
            self.module.engineHandlers.onUpdate()
        self.ask("fps", label="s", seconds=5)
        line = next(line for line in self.lines() if line.startswith("ROUND\tfps\ts\t"))
        self.assertIn("frames=3", line)
        self.assertIn("max=350.0ms", line)

    def open_box(self, serial=1, buttons=1):
        self.lua.execute(
            "box_open = { serial = %d, form = 'FormId:0x1', buttons = {%s}, text = 'SECRET', body = 'SECRET' }; pressed = {}\n"
            "package.loaded['openfallout.core'].messageBox = { current = function() return box_open end,\n"
            "    press = function(n) pressed[#pressed + 1] = n; box_open = nil; return true end }"
            % (serial, ", ".join("'b'" for _ in range(buttons))))

    def tick_frames(self, seconds):
        for _ in range(int(seconds / 0.25)):
            self.lua.execute("clock = clock + 0.25")
            self.module.engineHandlers.onUpdate()

    def test_a_message_box_is_answered_after_a_second_and_its_text_is_not_logged(self):
        self.open_box(serial=4, buttons=2)
        self.tick_frames(0.5)
        self.assertEqual(self.lua.eval("#pressed"), 0)
        self.tick_frames(1.0)
        self.assertEqual(self.lua.eval("pressed[1]"), 1)
        self.assertEqual(self.lua.eval("#pressed"), 1)
        lines = self.lines()
        self.assertIn("ROUND\tbox\tserial=4\tbuttons=2\tform=FormId:0x1\topen", lines)
        self.assertIn("ROUND\tbox\tserial=4\tpress 1\ttrue", lines)
        self.assertFalse(any("SECRET" in line for line in lines))

    def test_a_shot_under_a_message_box_says_so(self):
        self.open_box(serial=9)
        self.ask("shot", name="e")
        self.assertIn("ROUND\tshot\te\tn=1\tbox=9", self.lines())

    def test_boxes_can_be_left_open(self):
        self.ask("boxes", answer=False)
        self.open_box(serial=2)
        self.tick_frames(3)
        self.assertEqual(self.lua.eval("#pressed"), 0)
        self.assertIn("ROUND\tbox\tserial=2\tbuttons=1\tform=FormId:0x1\tleft open", self.lines())
        self.ask("boxes", answer=True)
        self.tick_frames(1.5)
        self.assertEqual(self.lua.eval("#pressed"), 1)

    def quiet(self):
        self.ask("quiet")
        answers = self.lua.eval("answers")
        return answers[len(answers)].data.result

    def test_quiet_says_whether_a_box_is_open_and_how_long_none_has_been(self):
        self.assertEqual(self.quiet().open, False)
        self.assertGreater(self.quiet().quiet, 1e8)       # no box was ever open
        self.open_box(serial=1)
        self.tick_frames(0.25)
        self.assertEqual(self.quiet().open, True)
        self.tick_frames(1.5)                              # answered after a second
        result = self.quiet()
        self.assertEqual(result.open, False)
        self.assertLess(result.quiet, 1.0)
        self.tick_frames(3)
        self.assertGreaterEqual(self.quiet().quiet, 3.0)

    def test_quiet_ignores_boxes_that_are_left_open(self):
        self.ask("boxes", answer=False)
        self.open_box(serial=1)
        self.tick_frames(0.5)
        self.assertEqual(self.quiet().open, False)

    def vats_stand_in(self, hit):
        self.lua.execute(f"""
            local core = require('openfallout.core')
            core.vats = {{ state = function() return {{ usable = true, open = false, actionPoints = 80, maxActionPoints = 80,
                                                       cost = 17, range = 700 }} end,
                          canTarget = function() return true end }}
            local nearby = require('openfallout.nearby')
            nearby.castRay = function(from, to, options) return {{ hitObject = {hit} }} end
        """)
        actor = self.lua.eval("make_actor(100, 900, 300)")
        self.ask("vatsInfo", label="v", actor=actor)
        answers = self.lua.eval("answers")
        return answers[len(answers)].data.result

    def test_vats_info_says_when_the_ray_to_the_head_is_clear(self):
        result = self.vats_stand_in("nil")
        self.assertTrue(result.clear)
        self.assertTrue(result.canTarget)
        self.assertTrue(any(line.startswith("ROUND\tvatsInfo\tv\tusable=true") and "ap=80.0/80.0" in line for line in self.lines()))
        self.assertTrue(any(line.startswith("ROUND\tvatsInfo\ttarget\tcanTarget=true") and line.endswith("ray=clear")
                            for line in self.lines()))

    def test_vats_info_says_what_is_in_the_way(self):
        result = self.vats_stand_in("{ recordId = 'FormId:0x10243e' }")
        self.assertFalse(result.clear)
        self.assertTrue(any(line.endswith("ray=FormId:0x10243e") for line in self.lines()))

    def test_unknown_question_is_answered_with_an_error(self):
        self.ask("nonsense")
        self.assertTrue(any(line.startswith("ROUND\terror\tnonsense") for line in self.lines()))
        self.assertEqual(self.lua.eval("#answers"), 1)


class RoundFiles(unittest.TestCase):
    ROUNDS = SCRIPTS.parent / "rounds"
    SCENARIOS = SCRIPTS.parent / "scenarios"

    def test_every_round_names_a_scenario_that_exists_and_variants_with_names(self):
        import json
        files = sorted(self.ROUNDS.glob("*.json"))
        self.assertGreaterEqual(len(files), 5)
        for path in files:
            config = json.loads(path.read_text(encoding="utf-8"))
            if "scenario" in config:
                self.assertTrue((path.parent / config["scenario"]).resolve().is_file(), f"{path.name}: {config['scenario']}")
            names = [variant.get("name") for variant in config.get("variants", [])]
            self.assertTrue(all(names), f"{path.name}: a variant has no name")
            self.assertEqual(len(names), len(set(names)), f"{path.name}: two variants have one name")
            for variant in config.get("variants", []):
                for key, value in variant.get("env", {}).items():
                    self.assertIsInstance(value, str, f"{path.name}: {key}")

    @unittest.skipUnless(LuaRuntime, "needs lupa")
    def test_every_script_and_scenario_is_valid_lua(self):
        lua = LuaRuntime(unpack_returned_tuples=True)
        load = lua.eval("function(source, name) local f, err = load(source, name) return err end")
        for path in sorted(SCRIPTS.glob("*.lua")) + sorted(self.SCENARIOS.glob("*.lua")):
            self.assertIsNone(load(path.read_text(encoding="utf-8"), path.name), path.name)

    def test_the_arms_tuning_round_moves_the_arms_by_one_variable(self):
        import json
        config = json.loads((self.ROUNDS / "m14-arms-tune.json").read_text(encoding="utf-8"))
        for variant in config["variants"]:
            words = variant["env"]["OPENFALLOUT_FIRSTPERSON_ARMS"].split()
            self.assertEqual(len(words), 6, variant["name"])       # x y z yaw pitch roll
            [float(word) for word in words]


if __name__ == "__main__":
    unittest.main()
