#!/usr/bin/env python3
"""Tests of files/data/scripts/omw/falloutpad.lua, the buttons of a game controller on the screens of Fallout, run with
lupa (skipped where it is not installed), and a syntax check of the scripts that read it. The module needs only the key
and button tables of openfallout.input, which are given here with the values SDL gives the buttons."""

import re
import unittest
from pathlib import Path

try:
    from lupa import LuaRuntime
except ImportError:  # pragma: no cover
    LuaRuntime = None

SCRIPTS = Path(__file__).resolve().parents[2] / "files" / "data" / "scripts" / "omw"

BUTTONS = {
    "A": 0, "B": 1, "X": 2, "Y": 3, "Back": 4, "Guide": 5, "Start": 6, "LeftStick": 7, "RightStick": 8,
    "LeftShoulder": 9, "RightShoulder": 10, "DPadUp": 11, "DPadDown": 12, "DPadLeft": 13, "DPadRight": 14,
}
KEYS = {
    "UpArrow": 1, "DownArrow": 2, "LeftArrow": 3, "RightArrow": 4, "PageUp": 5, "PageDown": 6, "Enter": 7, "Space": 8,
    "D": 9, "R": 10, "V": 11,
}
SCREENS = ["pipboy", "dialogue", "vats", "loot", "craft", "interaction"]


def pad_module():
    """The module, and the lines it printed."""
    lua = LuaRuntime(unpack_returned_tuples=True)
    printed = []
    lua.globals().print = lambda *words: printed.append(" ".join(str(word) for word in words))
    keys = lua.table_from(KEYS)
    buttons = lua.table_from(BUTTONS)
    lua.globals().test_keys = keys
    lua.globals().test_buttons = buttons
    lua.execute("package.preload['openfallout.input'] = function() "
                "return {KEY = test_keys, CONTROLLER_BUTTON = test_buttons} end")
    module = lua.eval("function(source) return load(source, 'falloutpad')() end")(
        (SCRIPTS / "falloutpad.lua").read_text(encoding="utf-8"))
    return lua, module, printed


@unittest.skipUnless(LuaRuntime, "needs lupa")
class PadTest(unittest.TestCase):
    def setUp(self):
        self.lua, self.pad, self.printed = pad_module()

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
        # A clicks, B leaves the screen, and the sticks and the menu button are the engine's: a key here would act twice
        for button in ("A", "B", "Start", "Back", "Guide", "LeftStick", "RightStick"):
            self.assertIsNone(self.key(button), button)

    def test_a_screen_can_give_a_button_a_key(self):
        self.assertIsNone(self.key("X"))
        self.assertEqual(self.key("X", {"X": KEYS["D"]}), KEYS["D"])
        # the others stay
        self.assertEqual(self.key("Y", {"X": KEYS["D"]}), KEYS["Enter"])

    def test_a_screen_can_change_the_key_of_a_button(self):
        self.assertEqual(self.key("Y", {"Y": KEYS["Space"]}), KEYS["Space"])

    def handler(self, is_open):
        pressed = []
        own = self.lua.table_from({BUTTONS["X"]: KEYS["R"]})
        handler = self.pad.handler("loot", lambda: is_open[0], lambda code: pressed.append(code), own)
        return handler, pressed

    def test_a_closed_screen_hears_nothing(self):
        is_open = [False]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["DPadDown"])
        self.assertEqual(pressed, [])
        self.assertEqual(self.printed, [])

    def test_an_open_screen_gets_the_key_of_the_button_and_logs_it(self):
        is_open = [True]
        handler, pressed = self.handler(is_open)
        handler(BUTTONS["DPadDown"])
        handler(BUTTONS["X"])
        handler(BUTTONS["A"])
        handler(BUTTONS["Start"])
        self.assertEqual(pressed, [KEYS["DownArrow"], KEYS["R"]])
        self.assertEqual(self.printed, [
            "Pad: screen=loot button=%d key=%d" % (BUTTONS["DPadDown"], KEYS["DownArrow"]),
            "Pad: screen=loot button=%d key=%d" % (BUTTONS["X"], KEYS["R"]),
        ])

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

    def test_the_keys_of_a_screen_and_the_buttons_go_through_one_function(self):
        for screen in SCREENS:
            text = (SCRIPTS / ("fallout%s.lua" % screen)).read_text(encoding="utf-8")
            self.assertIn("press(key.code)", text, screen)


if __name__ == "__main__":
    unittest.main()
