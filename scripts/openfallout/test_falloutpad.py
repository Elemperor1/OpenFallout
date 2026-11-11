#!/usr/bin/env python3
"""Tests of files/data/scripts/omw/falloutpad.lua, the buttons of a game controller on the screens of Fallout, run with
lupa (skipped where it is not installed), and a syntax check of the scripts that read it. The module needs the key and
button tables of openfallout.input, which are given here with the values SDL gives the buttons, and the two functions
with which the engine keeps whether the pad holds a pick (the row that the D-pad lit, which A acts on)."""

import re
import unittest
from pathlib import Path

try:
    # The game runs its scripts on LuaJIT. The default runtime of lupa is the newest Lua, which refuses some code that
    # LuaJIT accepts, such as an assignment to the variable of a for loop.
    from lupa.luajit21 import LuaRuntime
except ImportError:  # pragma: no cover
    try:
        from lupa import LuaRuntime
    except ImportError:
        LuaRuntime = None

SCRIPTS = Path(__file__).resolve().parents[2] / "files" / "data" / "scripts" / "omw"

BUTTONS = {
    "A": 0, "B": 1, "X": 2, "Y": 3, "Back": 4, "Guide": 5, "Start": 6, "LeftStick": 7, "RightStick": 8,
    "LeftShoulder": 9, "RightShoulder": 10, "DPadUp": 11, "DPadDown": 12, "DPadLeft": 13, "DPadRight": 14,
    # the axes of the triggers, which the engine tells a screen as buttons of these numbers
    "LeftTrigger": 100, "RightTrigger": 101,
}
KEYS = {
    "UpArrow": 1, "DownArrow": 2, "LeftArrow": 3, "RightArrow": 4, "PageUp": 5, "PageDown": 6, "Enter": 7, "Space": 8,
    "D": 9, "R": 10, "V": 11, "LeftBracket": 12, "RightBracket": 13, "A": 14, "F": 15,
}
SCREENS = ["pipboy", "dialogue", "vats", "loot", "craft", "interaction"]


def pad_module(with_pick_functions=True):
    """The module, the lines it printed and the pick of the pad (a list of one: the engine keeps it, and lets it go when
    the stick moves the pointer, which a test does by setting it)."""
    lua = LuaRuntime(unpack_returned_tuples=True)
    printed = []
    pick = [False]
    lua.globals().print = lambda *words: printed.append(" ".join(str(word) for word in words))
    keys = lua.table_from(KEYS)
    buttons = lua.table_from(BUTTONS)
    lua.globals().test_keys = keys
    lua.globals().test_buttons = buttons
    lua.globals().test_pad_pick = lambda: pick[0]
    lua.globals().test_set_pad_pick = lambda value: pick.__setitem__(0, bool(value))
    lua.execute("package.preload['openfallout.input'] = function() "
                "local input = {KEY = test_keys, CONTROLLER_BUTTON = test_buttons} "
                + ("input._padPick = test_pad_pick input._setPadPick = test_set_pad_pick " if with_pick_functions else "")
                + "return input end")
    module = lua.eval("function(source) return load(source, 'falloutpad')() end")(
        (SCRIPTS / "falloutpad.lua").read_text(encoding="utf-8"))
    return lua, module, printed, pick


@unittest.skipUnless(LuaRuntime, "needs lupa")
class PadTest(unittest.TestCase):
    def setUp(self):
        self.lua, self.pad, self.printed, self.pick = pad_module()

    def key(self, button, own=None):
        return self.pad.key(BUTTONS[button], self.lua.table_from({BUTTONS[k]: v for k, v in own.items()}) if own else None)

    def test_the_dpad_stands_for_the_arrows(self):
        self.assertEqual(self.key("DPadUp"), KEYS["UpArrow"])
        self.assertEqual(self.key("DPadDown"), KEYS["DownArrow"])
        self.assertEqual(self.key("DPadLeft"), KEYS["LeftArrow"])
        self.assertEqual(self.key("DPadRight"), KEYS["RightArrow"])

    def test_the_bumpers_stand_for_the_pages_and_y_for_enter(self):
        self.assertEqual(self.key("LeftShoulder"), KEYS["PageUp"])
        self.assertEqual(self.key("RightShoulder"), KEYS["PageDown"])
        self.assertEqual(self.key("Y"), KEYS["Enter"])

    def test_the_buttons_the_engine_uses_have_no_key(self):
        # B leaves the screen, and the sticks and the menu buttons are the engine's: a key here would act twice. A is the
        # key of the pick, which only the handler gives it (it clicks where the pointer is otherwise)
        for button in ("A", "B", "Start", "Back", "Guide", "LeftStick", "RightStick", "LeftTrigger", "RightTrigger"):
            self.assertIsNone(self.key(button), button)

    def test_a_screen_can_give_a_button_a_key(self):
        self.assertIsNone(self.key("X"))
        self.assertEqual(self.key("X", {"X": KEYS["D"]}), KEYS["D"])
        # the others stay
        self.assertEqual(self.key("Y", {"X": KEYS["D"]}), KEYS["Enter"])

    def test_a_screen_can_change_the_key_of_a_button(self):
        self.assertEqual(self.key("Y", {"Y": KEYS["Space"]}), KEYS["Space"])

    def test_a_screen_can_take_a_default_key_away_with_false(self):
        self.assertIsNone(self.key("Y", {"Y": False}))
        self.assertEqual(self.key("DPadUp", {"Y": False}), KEYS["UpArrow"])

    def test_a_screen_can_read_the_buttons_with_a_function(self):
        own = self.lua.eval("function(button) if button == %d then return false end end" % BUTTONS["DPadUp"])
        self.assertIsNone(self.pad.key(BUTTONS["DPadUp"], own))
        self.assertEqual(self.pad.key(BUTTONS["DPadDown"], own), KEYS["DownArrow"])

    def handler(self, is_open, own=None):
        pressed = []
        if own is None:
            own = self.lua.table_from({BUTTONS["X"]: KEYS["R"]})
        handler = self.pad.handler("loot", lambda: is_open[0], lambda code: pressed.append(code), own)
        return handler, pressed

    def test_a_closed_screen_hears_nothing(self):
        is_open = [False]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["DPadDown"])
        self.assertEqual(pressed, [])
        self.assertEqual(self.printed, [])
        self.assertFalse(self.pick[0])

    def test_an_open_screen_gets_the_key_of_the_button_and_logs_it(self):
        is_open = [True]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["DPadDown"])
        handler(BUTTONS["X"])
        handler(BUTTONS["Start"])
        self.assertEqual(pressed, [KEYS["DownArrow"], KEYS["R"]])
        self.assertEqual(self.printed, [
            "Pad: screen=loot button=%d key=%d" % (BUTTONS["DPadDown"], KEYS["DownArrow"]),
            "Pad: screen=loot button=%d key=%d" % (BUTTONS["X"], KEYS["R"]),
        ])

    def test_a_clicks_at_the_pointer_until_the_dpad_picks(self):
        # The engine clicks with A where the pointer is; the screen then hears nothing of A
        is_open = [True]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [])
        self.assertEqual(self.printed, [])

    def test_a_is_enter_on_the_row_the_dpad_picked(self):
        is_open = [True]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["DPadDown"])
        self.assertTrue(self.pick[0])
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [KEYS["DownArrow"], KEYS["Enter"]])
        self.assertIn("Pad: screen=loot button=%d key=%d" % (BUTTONS["A"], KEYS["Enter"]), self.printed)

    def test_the_stick_gives_the_pointer_back_to_a(self):
        is_open = [True]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["DPadDown"])
        self.pick[0] = False  # the engine lets the pick go when the stick moves the pointer
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [KEYS["DownArrow"]])

    def test_every_button_that_moves_the_pick_holds_it(self):
        for button in ("DPadUp", "DPadDown", "DPadLeft", "DPadRight", "LeftShoulder", "RightShoulder"):
            self.pick[0] = False
            handler, pressed = self.handler([True])
            handler(BUTTONS[button])
            self.assertTrue(self.pick[0], button)

    def test_a_button_that_is_no_mover_does_not_hold_the_pick(self):
        # Y is Enter: it acts on the pick, it does not move it
        handler, pressed = self.handler([True])
        handler(BUTTONS["Y"])
        self.assertEqual(pressed, [KEYS["Enter"]])
        self.assertFalse(self.pick[0])

    def test_a_screen_can_give_a_its_own_key_or_none(self):
        is_open = [True]
        handler, pressed = self.handler(is_open, self.lua.table_from({BUTTONS["A"]: KEYS["Space"]}))
        self.pick[0] = True
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [KEYS["Space"]])
        handler, pressed = self.handler(is_open, self.lua.table_from({BUTTONS["A"]: False}))
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [])

    def test_translate_is_what_a_screen_that_reads_the_buttons_itself_asks(self):
        self.assertIsNone(self.pad.translate(BUTTONS["A"]))
        self.assertEqual(self.pad.translate(BUTTONS["DPadLeft"]), KEYS["LeftArrow"])
        self.assertTrue(self.pick[0])
        self.assertEqual(self.pad.translate(BUTTONS["A"]), KEYS["Enter"])
        self.assertIsNone(self.pad.translate(BUTTONS["B"]))

    def test_release_pick_gives_a_back_to_the_pointer(self):
        self.pad.translate(BUTTONS["DPadDown"])
        self.assertTrue(self.pad.holdsPick())
        self.pad.releasePick()
        self.assertFalse(self.pad.holdsPick())
        self.assertIsNone(self.pad.translate(BUTTONS["A"]))

    def test_an_engine_without_the_pick_leaves_a_to_the_pointer(self):
        # A build whose engine does not tell the pick: the module must not fail, and A stays a click
        _, module, _, _ = pad_module(with_pick_functions=False)
        self.assertFalse(module.holdsPick())
        module.holdPick()
        module.releasePick()
        self.assertIsNone(module.translate(BUTTONS["A"]))
        self.assertEqual(module.translate(BUTTONS["DPadDown"]), KEYS["DownArrow"])

    def test_the_log_stops_after_a_limit_but_the_presses_do_not(self):
        is_open = [True]
        handler, pressed = self.handler(is_open)
        for _ in range(250):
            handler(BUTTONS["DPadDown"])
        self.assertEqual(len(pressed), 250)
        self.assertEqual(len(self.printed), 200)
        self.assertIn("further presses are not logged", self.printed[-1])
        self.assertNotIn("further presses are not logged", self.printed[-2])


@unittest.skipUnless(LuaRuntime, "needs lupa")
class PipboyPadTest(unittest.TestCase):
    """The pad on the Pip-Boy: the bumpers turn the pages, the sides of the D-pad the tabs, the triggers the list."""

    def setUp(self):
        self.lua, self.pad, self.printed, self.pick = pad_module()

    def own(self, plain):
        return self.pad.pipboyOwn(lambda: plain[0])

    def key(self, button, plain=False):
        return self.pad.key(BUTTONS[button], self.own([plain]))

    def test_the_bumpers_turn_the_pages(self):
        self.assertEqual(self.key("LeftShoulder"), KEYS["LeftArrow"])
        self.assertEqual(self.key("RightShoulder"), KEYS["RightArrow"])

    def test_the_sides_of_the_dpad_go_to_the_tab_before_and_after(self):
        self.assertEqual(self.key("DPadLeft"), KEYS["LeftBracket"])
        self.assertEqual(self.key("DPadRight"), KEYS["RightBracket"])

    def test_up_and_down_pick_a_row_and_the_triggers_turn_a_page_of_the_list(self):
        self.assertEqual(self.key("DPadUp"), KEYS["UpArrow"])
        self.assertEqual(self.key("DPadDown"), KEYS["DownArrow"])
        self.assertEqual(self.key("LeftTrigger"), KEYS["PageUp"])
        self.assertEqual(self.key("RightTrigger"), KEYS["PageDown"])

    def test_x_drops_and_y_repairs_as_the_games_name_them(self):
        self.assertEqual(self.key("X"), KEYS["D"])
        self.assertEqual(self.key("Y"), KEYS["R"])

    def test_over_a_question_the_buttons_are_the_plain_arrows_and_pages(self):
        # A question or a screen of its own is up (how many, repair, mods): X and Y mean nothing there
        self.assertIsNone(self.key("X", plain=True))
        self.assertIsNone(self.key("Y", plain=True))
        self.assertEqual(self.key("DPadLeft", plain=True), KEYS["LeftArrow"])
        self.assertEqual(self.key("LeftShoulder", plain=True), KEYS["PageUp"])

    def test_a_is_enter_on_the_pick_of_the_pipboy_too(self):
        pressed = []
        handler = self.pad.handler("pipboy", lambda: True, lambda code: pressed.append(code), self.own([False]))
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [])
        handler(BUTTONS["DPadDown"])
        handler(BUTTONS["A"])
        self.assertEqual(pressed, [KEYS["DownArrow"], KEYS["Enter"]])

    def test_a_press_of_a_bumper_or_a_trigger_holds_the_pick_as_well(self):
        for button in ("LeftShoulder", "RightShoulder", "LeftTrigger", "RightTrigger"):
            self.pick[0] = False
            self.pad.translate(BUTTONS[button], self.own([False]))
            self.assertTrue(self.pick[0], button)


@unittest.skipUnless(LuaRuntime, "needs lupa")
class ScriptsTest(unittest.TestCase):
    def test_the_scripts_parse(self):
        lua = LuaRuntime()
        check = lua.eval("function(source) local _, message = load(source) return message end")
        for path in sorted(SCRIPTS.glob("fallout*.lua")) + [SCRIPTS / "input" / "playercontrols.lua"]:
            self.assertIsNone(check(path.read_text(encoding="utf-8")), path.name)

    def test_every_screen_hears_the_pad_and_a_script_can_press_for_it(self):
        for screen in SCREENS:
            text = (SCRIPTS / ("fallout%s.lua" % screen)).read_text(encoding="utf-8")
            self.assertIn("onControllerButtonPress = onPad", text, screen)
            self.assertIn("PadButton = function(data)", text, screen)
            self.assertIn("pad.handler('%s'" % screen, text, screen)

    def test_every_module_the_scripts_require_is_copied_with_the_data(self):
        # A module missing from the list of files/data/CMakeLists.txt is not found by the game that is built
        listed = (SCRIPTS.parents[1] / "CMakeLists.txt").read_text(encoding="utf-8")
        for path in sorted(SCRIPTS.glob("fallout*.lua")):
            self.assertIn("scripts/omw/" + path.name, listed, path.name)
            for module in re.findall(r"require\('scripts\.omw\.(\w+)'\)", path.read_text(encoding="utf-8")):
                self.assertIn("scripts/omw/%s.lua" % module, listed, module)

    def test_a_menu_over_a_screen_takes_the_keys_and_the_buttons(self):
        # A screen keeps its element when the main menu opens over it, so its keys and buttons act only while its mode
        # is the top one
        for screen, mode in (("pipboy", "Interface"), ("dialogue", "Dialogue"), ("vats", "Interface"),
                             ("loot", "Interface"), ("craft", "Interface"), ("interaction", "Interface")):
            text = (SCRIPTS / ("fallout%s.lua" % screen)).read_text(encoding="utf-8")
            self.assertGreaterEqual(text.count("I.UI.getMode() ~= '%s'" % mode), 1, screen)
            self.assertIn("I.UI.getMode() == '%s'" % mode, text, screen)

    def test_a_screen_that_reads_the_buttons_itself_asks_translate_not_key(self):
        # `key` knows nothing of the pick: a screen that took its key from it would leave A without a meaning after the D-pad
        for name in ("companion", "barter", "facescreen"):
            text = (SCRIPTS / ("fallout%s.lua" % name)).read_text(encoding="utf-8")
            self.assertIn("pad.translate(button)", text, name)
            self.assertNotIn("pad.key(button)", text, name)

    def test_a_screen_with_no_pick_of_its_own_gives_a_back_to_the_pointer(self):
        for name in ("blackjack", "roulette", "slots", "caravan", "specialscreen"):
            text = (SCRIPTS / ("fallout%s.lua" % name)).read_text(encoding="utf-8")
            self.assertIn("padModule.releasePick()", text, name)

    def test_the_keys_of_a_screen_and_the_buttons_go_through_one_function(self):
        for screen in SCREENS:
            text = (SCRIPTS / ("fallout%s.lua" % screen)).read_text(encoding="utf-8")
            # Some screens also hand the symbol of the key on: press(key.code, key.symbol)
            self.assertRegex(text, r"press\(key\.code[,)]", screen)


if __name__ == "__main__":
    unittest.main()
