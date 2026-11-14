#!/usr/bin/env python3
"""The wheel of files/data/scripts/omw/falloutmenu.lua: how many notches the menu file is told for a wheel event of
the engine. Run with a `luajit` (or `lua`) on the PATH, skipped where there is none; the engine modules the script
requires are given as empty stand-ins."""

import shutil
import subprocess
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "files" / "data" / "scripts" / "omw" / "falloutmenu.lua"
LUA = shutil.which("luajit") or shutil.which("lua")

PRELUDE = """
for _, name in ipairs({ 'openfallout.async', 'openfallout.core', 'openfallout.ui' }) do
    package.preload[name] = function() return {} end
end
package.preload['openfallout.util'] = function() return { vector2 = function(x, y) return { x = x, y = y } end } end
local menu = dofile(SCRIPT_PATH)
local function event(notches, delta)
    return { notches = notches and { y = notches } or nil, delta = { y = delta or 0 } }
end
"""


def run(lua_body):
    program = f"local SCRIPT_PATH = {str(SCRIPT)!r}\n" + PRELUDE + lua_body
    result = subprocess.run([LUA, "-"], input=program, text=True, capture_output=True, timeout=30)
    if result.returncode != 0:
        raise AssertionError(result.stderr)
    return result.stdout.split()


@unittest.skipUnless(LUA, "needs luajit or lua")
class WheelNotchesTest(unittest.TestCase):
    def notches(self, notches=None, delta=0.0):
        args = "nil" if notches is None else repr(notches)
        return int(run(f"print(menu.wheelNotches(event({args}, {delta!r})))")[0])

    def test_the_file_counts_up_as_minus_one(self):
        self.assertEqual(self.notches(1), -1)
        self.assertEqual(self.notches(-1), 1)
        self.assertEqual(self.notches(0, 0.4), 0)

    def test_a_fast_wheel_keeps_every_notch(self):
        self.assertEqual(self.notches(3), -3)
        self.assertEqual(self.notches(-2), 2)

    def test_without_whole_notches_the_delta_gives_one_step_its_way(self):
        self.assertEqual(self.notches(None, 2.5), -1)
        self.assertEqual(self.notches(None, -0.2), 1)
        self.assertEqual(self.notches(None, 0.0), 0)


if __name__ == "__main__":
    unittest.main()
