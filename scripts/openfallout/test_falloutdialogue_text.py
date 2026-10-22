#!/usr/bin/env python3
"""Tests of the text cleaning of files/data/scripts/omw/falloutdialogue.lua, run with LuaJIT (skipped where there is
none). The function is cut out of the script, which needs the modules of the engine to load as a whole."""

import re
import shutil
import subprocess
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "files" / "data" / "scripts" / "omw" / "falloutdialogue.lua"


def clean_function():
    text = SCRIPT.read_text(encoding="utf-8")
    start = text.index("local reportedMarkup = 0")
    end = text.index("-- The words of a text in lines that fit the width.")
    return text[start:end]


def clean(text):
    """The text as the window shows it, and what the script printed about it."""
    luajit = shutil.which("luajit")
    # The script logs what it took out with print: that goes to the error output here
    program = ("print = function(...) io.stderr:write(table.concat({...}, ' '), '\\n') end\n" + clean_function()
               + "\nlocal result = clean(io.read('*a'))\nio.write(result)\n")
    run = subprocess.run([luajit, "-e", program], input=text.encode("utf-8"), capture_output=True, check=True)
    return run.stdout.decode("utf-8")


@unittest.skipUnless(shutil.which("luajit"), "needs LuaJIT")
class CleanTest(unittest.TestCase):
    def words(self, text):
        return clean(text).split()

    def test_plain_text_is_unchanged(self):
        self.assertEqual(clean("Hello there, stranger."), "Hello there, stranger.")

    def test_a_note_in_braces_glued_to_the_first_word_is_taken_out(self):
        self.assertEqual(self.words("{Happy}Hello there."), ["Hello", "there."])
        self.assertEqual(self.words("{Eyes narrowed}I know you."), ["I", "know", "you."])

    def test_notes_inside_the_text_and_after_each_other_are_taken_out(self):
        self.assertEqual(self.words("{One}{Two}Well {Sigh} then."), ["Well", "then."])

    def test_a_brace_that_is_not_closed_stays(self):
        self.assertEqual(self.words("Open {brace"), ["Open", "{brace"])

    def test_markup_tags_with_an_equals_sign_are_taken_out(self):
        self.assertEqual(self.words("You have <Global=Caps> caps."), ["You", "have", "caps."])
        self.assertEqual(self.words("<Alias=Player>, hello."), [",", "hello."])

    def test_square_and_other_angle_marks_stay(self):
        self.assertEqual(self.words("[Speech 50] <sigh> Fine."), ["[Speech", "50]", "<sigh>", "Fine."])

    def test_control_characters_become_spaces(self):
        self.assertEqual(self.words("One\x01two\r\nthree"), ["One", "two", "three"])

    def test_nothing_is_not_an_error(self):
        self.assertEqual(clean("").strip(), "")


if __name__ == "__main__":
    unittest.main()
