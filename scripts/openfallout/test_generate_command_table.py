#!/usr/bin/env python3
"""Checks generate_command_table.py on small tables."""
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_command_table as generate  # noqa: E402


def write_csv(directory, name, text):
    path = os.path.join(directory, name)
    with open(path, "w", newline="") as stream:
        stream.write(text)
    return path


class GenerateCommandTableTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def commands(self, name, text):
        return generate.read_commands(write_csv(self.directory.name, name, text))

    def test_keeps_only_the_commands_a_script_can_call(self):
        commands = self.commands(
            "a.csv",
            "0,0x0000,GameMode,,0,,0x0\n1,0x0124,SetGameSetting,SetGS,0,0 0,0x0\n2,0x1039,SetStage,,0,14 23,0x0,0x4F2A10\n",
        )
        self.assertEqual(list(commands), [0x1039])
        self.assertEqual(commands[0x1039], ("SetStage", "", False, "14 23", 0))

    def test_marks_the_commands_fallout_3_has_and_the_parameters_that_differ(self):
        fallout3 = self.commands("f.csv", "2,0x1039,SetStage,,0,14 23,0x0\n2,0x11C1,HasPerk,,1,39,0x0\n")
        new_vegas = self.commands(
            "n.csv", "2,0x1039,SetStage,,0,14 23,0x0\n2,0x11C1,HasPerk,,1,39 1?,0x0\n2,0x127C,PrintTrackedCells,,0,1?,0x0\n"
        )
        rows = generate.merge(fallout3, new_vegas)
        self.assertEqual([r[0] for r in rows], [0x1039, 0x11C1, 0x127C])
        self.assertEqual([r[6] for r in rows], [True, True, False])
        self.assertEqual([r[7] for r in rows], [None, "39", None])

    def test_stops_when_the_games_disagree_on_a_name(self):
        fallout3 = self.commands("f.csv", "2,0x1039,SetStage,,0,14 23,0x0\n")
        new_vegas = self.commands("n.csv", "2,0x1039,SetStageX,,0,14 23,0x0\n")
        with self.assertRaises(SystemExit):
            generate.merge(fallout3, new_vegas)

    def test_stops_when_fallout_3_has_a_command_new_vegas_lacks(self):
        fallout3 = self.commands("f.csv", "2,0x1039,SetStage,,0,14 23,0x0\n")
        with self.assertRaises(SystemExit):
            generate.merge(fallout3, {})

    def test_writes_one_line_per_command(self):
        fallout3 = self.commands("f.csv", "2,0x11C1,HasPerk,,1,39,0x0\n")
        new_vegas = self.commands("n.csv", "2,0x11C1,HasPerk,,1,39 1?,0x0\n")
        out = io.StringIO()
        generate.write(generate.merge(fallout3, new_vegas), out)
        lines = [line for line in out.getvalue().splitlines() if not line.startswith("//")]
        self.assertEqual(lines, ['{ 0x11C1, "HasPerk", "", true, 0x0, "39 1?", true, "39" },'])

    def test_writes_the_parameter_types_of_the_condition_functions(self):
        new_vegas = self.commands(
            "n.csv",
            "2,0x103A,GetStage,,0,14,0x0\n2,0x1039,SetStage,,0,14 23,0x0\n2,0x1005,GetLocked,,1,,0x0\n"
            "2,0x1002,AddItem,,1,50 1 1?,0x0\n",
        )
        rows = generate.merge({}, new_vegas)
        entries = generate.condition_parameters(rows)
        # a command with no parameters has no line; the third parameter is not kept
        # (the functions that only a condition can name are added to what the table has)
        self.assertEqual(entries, [(2, 50, 1), (53, 4, 22), (57, 14, 23), (58, 14, None), (79, 14, 22)])
        out = io.StringIO()
        generate.write_conditions(entries, out)
        lines = [line for line in out.getvalue().splitlines() if not line.startswith("//")]
        self.assertEqual(lines, ["{ 2, 50, 1 },", "{ 53, 4, 22 },", "{ 57, 14, 23 },", "{ 58, 14, 0xFFFF },",
                                 "{ 79, 14, 22 },"])


if __name__ == "__main__":
    unittest.main()
