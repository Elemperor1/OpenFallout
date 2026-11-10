#!/usr/bin/env python3
"""Checks command_census.py on small census files, and that it reads the states of this tree."""
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import command_census as census  # noqa: E402
import coverage_dashboard as dashboard  # noqa: E402

SCRIPTS = """kind,code,name,measure,count
call,0x1002,AddItem,statements,10
call,0x1002,AddItem,expressions,1
call,0x1002,AddItem,on_reference,9
call,0x1002,AddItem,scripts,4
call,0x1002,AddItem,scripts_info,3
call,0x1ABC,,statements,2
call,0x1ABC,,scripts,1
condition,0x1048,GetIsID,uses,7
condition,0x1048,GetIsID,first_parameter,7
"""

DIALOGUE = """kind,code,name,measure,count
dialogue_condition,0x1048,GetIsID,uses,100
dialogue_condition,0x1048,GetIsID,on_player,30
dialogue_condition,0x1049,Other,uses,5
"""


class Files:
    def __init__(self, test):
        self.directory = tempfile.TemporaryDirectory()
        test.addCleanup(self.directory.cleanup)

    def write(self, name, text):
        path = os.path.join(self.directory.name, name)
        with open(path, "w", encoding="utf-8") as stream:
            stream.write(text)
        return path


class ReadingTest(unittest.TestCase):
    def setUp(self):
        self.files = Files(self)

    def test_counts_by_command_kind_and_measure(self):
        counts = census.read_census_csv(self.files.write("s.csv", SCRIPTS))
        self.assertEqual(counts[0x1002][("call", "statements")], 10)
        self.assertEqual(counts[0x1002][("call", "scripts_info")], 3)
        self.assertEqual(counts[0x1048][("condition", "uses")], 7)

    def test_a_file_that_is_not_a_census_is_refused(self):
        with self.assertRaises(SystemExit):
            census.read_census_csv(self.files.write("bad.csv", "a,b,c\n1,2,3\n"))

    def test_a_line_with_a_bad_count_is_refused(self):
        with self.assertRaises(SystemExit):
            census.read_census_csv(
                self.files.write("bad.csv", "kind,code,name,measure,count\ncall,0x1002,AddItem,statements,many\n")
            )

    def test_a_line_with_the_wrong_number_of_fields_is_refused(self):
        with self.assertRaises(SystemExit):
            census.read_census_csv(self.files.write("bad.csv", "kind,code,name,measure,count\ncall,0x1002,x,1\n"))

    def test_files_of_one_game_add_up(self):
        scripts = census.read_census_csv(self.files.write("s.csv", SCRIPTS))
        dialogue = census.read_census_csv(self.files.write("d.csv", DIALOGUE))
        merged = census.merge_counts([scripts, dialogue])
        self.assertEqual(census.uses_of(merged[0x1048]), {"calls": 0, "scripts": 0, "conditions": 107, "uses": 107})
        self.assertEqual(
            census.uses_of(merged[0x1002]), {"calls": 11, "scripts": 4, "conditions": 0, "uses": 11}
        )


class RankingTest(unittest.TestCase):
    def setUp(self):
        files = Files(self)
        scripts = census.read_census_csv(files.write("s.csv", SCRIPTS))
        dialogue = census.read_census_csv(files.write("d.csv", DIALOGUE))
        self.games = {"fallout3": census.merge_counts([scripts, dialogue]), "newvegas": {0x1002: {("call", "statements"): 5}}}
        self.states = {
            0x1002: {"name": "AddItem", "state": dashboard.DONE, "note": "x.cpp", "fallout3": True},
            0x1048: {"name": "GetIsID", "state": dashboard.TODO, "note": "no handler", "fallout3": True},
            0x1049: {"name": "Other", "state": dashboard.STAND_IN, "note": "fixed", "fallout3": False},
            0x104A: {"name": "NeverCalled", "state": dashboard.TODO, "note": "no handler", "fallout3": True},
        }
        self.rows = census.build_rows(self.games, self.states)

    def row(self, code):
        return next(r for r in self.rows if r["code"] == code)

    def test_every_command_and_every_unknown_code_has_a_row(self):
        self.assertEqual([r["code"] for r in self.rows], [0x1002, 0x1048, 0x1049, 0x104A, 0x1ABC])
        self.assertEqual(self.row(0x1ABC)["state"], "unknown")
        self.assertEqual(self.row(0x1ABC)["note"], "not in the table of commands")

    def test_uses_are_added_over_games(self):
        self.assertEqual(self.row(0x1002)["uses"], 16)
        self.assertEqual(self.row(0x1002)["newvegas"]["calls"], 5)
        self.assertEqual(self.row(0x1048)["fallout3"]["conditions"], 107)

    def test_ranking_keeps_one_state_and_drops_unused_commands(self):
        missing = census.ranked(self.rows, dashboard.TODO)
        self.assertEqual([r["name"] for r in missing], ["GetIsID"])
        self.assertEqual([r["name"] for r in census.ranked(self.rows, dashboard.STAND_IN)], ["Other"])

    def test_the_summary_splits_the_uses(self):
        lines = {title: rest for title, *rest in census.summary_lines(self.rows)}
        # uses: AddItem 16 (done), GetIsID 107 and Other 5 (the unknown code is not in the table's total)
        self.assertEqual(lines["handler"][:3], [1, 1, 16])
        self.assertEqual(lines["no handler"][:3], [2, 1, 107])
        self.assertEqual(lines["stand-in"][:3], [1, 1, 5])
        self.assertEqual(lines["no handler"][3], "83.6%")

    def test_the_page_lists_the_missing_commands_and_the_unknown_codes(self):
        page = census.render_markdown(self.rows, 10, ["fallout3", "newvegas"])
        self.assertIn("| 1 | 0x1048 | GetIsID | 0 | 107 | 0 | 0 | 107 | 100.0% |", page)
        self.assertIn("0x1ABC (2)", page)
        self.assertIn("1 commands with no handler are called by no script", page)
        self.assertIn("Fallout 3, New Vegas", page)

    def test_the_page_says_when_a_list_is_longer_than_the_rows_it_shows(self):
        states = {code: {"name": "C%d" % code, "state": dashboard.TODO, "note": "", "fallout3": True} for code in range(0x1100, 0x1105)}
        games = {"fallout3": {code: {("call", "statements"): 1} for code in states}}
        page = census.render_markdown(census.build_rows(games, states), 2, ["fallout3"])
        self.assertIn("3 more commands with uses", page)

    def test_the_table_of_every_command_is_written_in_order_of_uses(self):
        files = Files(self)
        path = os.path.join(files.directory.name, "usage.csv")
        census.write_csv(self.rows, path)
        with open(path, encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertEqual(lines[0], "code,name,state,fo3_calls,fo3_scripts,fo3_conditions,nv_calls,nv_scripts,nv_conditions,uses")
        self.assertEqual(lines[1], "0x1048,GetIsID,todo,0,0,107,0,0,0,107")
        self.assertEqual(lines[2], "0x1002,AddItem,done,11,4,0,5,0,0,16")


class OutputTest(unittest.TestCase):
    def setUp(self):
        self.files = Files(self)
        self.census = self.files.write("scripts.csv", SCRIPTS)

    def run_main(self, *arguments):
        with mock.patch.object(sys, "argv", ["command_census.py", "--fallout3", self.census, *arguments]):
            with mock.patch("sys.stdout"), mock.patch("sys.stderr"):
                return census.main()

    def test_a_report_is_not_written_over_a_census_file(self):
        for option in ("--markdown", "--csv"):
            with self.assertRaises(SystemExit):
                self.run_main(option, self.census)
            with open(self.census, encoding="utf-8") as stream:
                self.assertEqual(stream.read(), SCRIPTS)

    def test_a_link_to_a_census_file_is_the_census_file(self):
        link = os.path.join(self.files.directory.name, "link.csv")
        try:
            os.symlink(self.census, link)
        except (OSError, NotImplementedError):
            self.skipTest("no links here")
        with self.assertRaises(SystemExit):
            self.run_main("--csv", link)
        with open(self.census, encoding="utf-8") as stream:
            self.assertEqual(stream.read(), SCRIPTS)

    def test_a_report_goes_to_a_path_that_is_not_there_yet(self):
        report = os.path.join(self.files.directory.name, "usage.csv")
        self.assertEqual(self.run_main("--csv", report), 0)
        self.assertTrue(os.path.exists(report))


class TreeTest(unittest.TestCase):
    def test_the_states_cover_the_table_of_commands(self):
        states = census.command_states()
        table = dashboard.read_vanilla_commands(dashboard.read_text(dashboard.VANILLA_TABLE))
        self.assertEqual(len(states), len(table))
        self.assertEqual({s["state"] for s in states.values()} - {dashboard.DONE, dashboard.STAND_IN, dashboard.TODO}, set())
        # a command that the engine has had a handler for since the first quests
        self.assertEqual(states[0x1002]["name"], "AddItem")
        self.assertEqual(states[0x1002]["state"], dashboard.DONE)


if __name__ == "__main__":
    unittest.main()
