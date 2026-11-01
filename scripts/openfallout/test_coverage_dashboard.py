#!/usr/bin/env python3
"""Checks coverage_dashboard.py on small sources, and that its numbers add up on this tree."""
import os
import re
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coverage_dashboard as coverage  # noqa: E402


class SourceReadingTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def write(self, name, text):
        with open(os.path.join(self.directory.name, name), "w") as stream:
            stream.write(text)

    def test_handlers_by_name_and_code_over_lines(self):
        self.write(
            "a.cpp",
            'interpreter.setHandler("GetStage", f);\n'
            'interpreter.setHandler(\n    "SetStage", g);\n'
            "interpreter.setHandler(0x278A, h);\n"
            '// interpreter.setHandler("Commented", f);\n',
        )
        names, codes = coverage.read_handlers([self.directory.name])
        self.assertEqual(sorted(names), ["getstage", "setstage"])
        self.assertEqual(sorted(codes), [0x278A])

    def test_listed_commands_and_their_reasons(self):
        text = (
            'constexpr IgnoredCommand ignored[] = {\n'
            '    { "PlayBink", "videos are not played" },\n'
            '    // { "Old", "not any more" },\n'
            '    { "SetActorAlpha", "the transparency is not drawn" },\n'
            '};\n'
        )
        self.assertEqual(
            coverage.read_listed_commands(text, "ignored"),
            {"playbink": "videos are not played", "setactoralpha": "the transparency is not drawn"},
        )

    def test_fixed_answers_with_and_without_a_namespace(self):
        self.write(
            "a.cpp",
            'x.setHandler("IsXBox", [](CallContext&) { return 0.0; });\n'
            'x.setHandler("GetCasino", [](FalloutScript::CallContext&) { return 1.0; });\n'
            'x.setHandler("Real", [&world](CallContext& call) { return world.value(call); });\n',
        )
        self.assertEqual(sorted(coverage.read_fixed_answers([self.directory.name])), ["getcasino", "isxbox"])

    def test_census_rows(self):
        text = (
            "## Every record type in the two base plugins\n\n| Record | Loader | `Fallout3.esm` | `FalloutNV.esm` |\n"
            "|---|:-:|---:|---:|\n| `ACHR` | yes | 2,154 | 3,386 |\n| `ALOC` | yes | - | 89 |\n| **All** | | **1** | **2** |\n\n"
            "## Scripts held by records\n| `XXXX` | yes | 1 | 1 |\n"
        )
        self.assertEqual(coverage.read_census_types(text), [("ACHR", 2154, 3386), ("ALOC", 0, 89)])

    def test_tracker_csv_skips_note_lines(self):
        self.write("t.csv", "# a note\nfamily,name,evidence\ncommand,Foo,somewhere 0x401000\n")
        rows = coverage.read_tracker(os.path.join(self.directory.name, "t.csv"))
        self.assertEqual(coverage.read_reads(rows), {("command", "foo"): "somewhere 0x401000"})

    def test_a_read_without_evidence_is_refused(self):
        with self.assertRaises(SystemExit):
            coverage.read_reads([{"family": "command", "name": "Foo", "evidence": ""}])

    def test_animation_groups_of_both_tables(self):
        groups = {name: (new_vegas, fallout3) for name, new_vegas, fallout3 in coverage.read_animation_groups()}
        self.assertEqual(len(groups), 245)
        self.assertEqual(groups["Idle"], (0, 0))
        self.assertEqual(sum(1 for _, fallout3 in groups.values() if fallout3 is not None), 183)

    def test_entry_point_constants(self):
        constants = coverage.read_entry_point_constants()
        self.assertEqual(constants["weaponDamage"], 0)
        self.assertEqual(constants["improvedDetection"], 69)


class TreeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = coverage.build()

    def test_every_unit_is_in_one_state(self):
        for library in self.data["libraries"]:
            self.assertEqual(sum(library["count"].values()), len(library["units"]), library["name"])
            for unit in library["units"]:
                self.assertIn(unit["s"], (coverage.DONE, coverage.STAND_IN, coverage.TODO))

    def test_every_command_of_the_table_has_a_place(self):
        listed = coverage.read_vanilla_commands(coverage.read_text(coverage.VANILLA_TABLE))
        placed = sum(len(lib["units"]) for lib in self.data["libraries"] if lib["family"] == "command")
        self.assertEqual(placed, len(listed))

    def test_every_handler_names_a_command(self):
        # The only handlers the table of the games lacks are the extender ones the engine adds itself
        entries = {name.lower() for name in coverage.read_extender_entries(coverage.read_text(coverage.EXTENDER_ENTRIES)).values()}
        self.assertEqual(set(self.data["unknownHandlers"]) - entries, set())

    def test_tale_of_two_wastelands_codes_are_all_in_the_tables(self):
        reached = sum(1 for lib in self.data["libraries"] for unit in lib["units"] if "ttw" in unit)
        self.assertEqual(reached, self.data["ttwTotal"])

    def test_every_unit_says_whether_it_is_read(self):
        for library in self.data["libraries"]:
            self.assertEqual(library["read"], sum(unit["r"] for unit in library["units"]), library["name"])
            for unit in library["units"]:
                self.assertIn(unit["r"], (0, 1))
                if unit["r"]:
                    self.assertTrue(unit.get("e"), "%s is read with no evidence" % unit["n"])

    def test_every_read_on_record_matches_a_unit(self):
        self.assertEqual(self.data["unknownReads"], [])

    def test_the_menu_table_is_the_121_files_and_names_real_screens(self):
        rows = coverage.read_tracker(coverage.TRACKER_MENUS)
        names = [row["name"] for row in rows]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(len(names), 121)
        screens = set(os.listdir(coverage.path("files/data/scripts/omw")))
        for row in rows:
            if row["implemented"] == coverage.STAND_IN:
                # A stand-in says which of the engine's screens does the job
                named = re.findall(r"\b(\w+\.lua)\b", row["note"])
                self.assertTrue(named and named[0] in screens, row["name"])
            if row["read"] == "read":
                self.assertTrue(row["evidence"].strip(), row["name"])
        menu_units = sum(len(lib["units"]) for lib in self.data["libraries"] if lib["family"] == "menu")
        self.assertEqual(menu_units, 121)

    def test_the_perk_table_agrees_with_perks_hpp(self):
        rows = coverage.read_tracker(coverage.TRACKER_PERKS)
        self.assertEqual([int(row["number"]) for row in rows], list(range(74)))
        names = {int(row["number"]): row["name"] for row in rows}
        # The constants of perks.hpp are the numbers of the executables; the editor's names say the same thing
        expected = {
            "weaponDamage": "Weapon Damage", "criticalChance": "Critical Hit Chance", "hitChance": "To Hit",
            "experience": "Experience", "buyPrice": "Buy Price", "actionPointRegen": "Action Point Regen",
            "runSpeed": "Run Speed", "attackSpeed": "Attack Speed", "packRat": "Light Items",
            "thresholdDefender": "defender", "thresholdAttacker": "attacker", "improvedDetection": "Improved Detection",
        }
        constants = coverage.read_entry_point_constants()
        for constant, word in expected.items():
            self.assertIn(word.lower(), names[constants[constant]].lower(), constant)
        dead = [int(row["number"]) for row in rows if row["dead"].strip()]
        library = next(lib for lib in self.data["libraries"] if lib["family"] == "perk")
        self.assertEqual(len(library["units"]), 74 - len(dead))

    def test_animation_groups_are_all_placed(self):
        placed = sum(len(lib["units"]) for lib in self.data["libraries"] if lib["family"] == "animation")
        self.assertEqual(placed, 245)

    def test_the_page_takes_the_data(self):
        page = coverage.render_html(
            self.data, os.path.join(os.path.dirname(coverage.__file__), "coverage_dashboard.html"), fragment=True
        )
        self.assertNotIn("/*COVERAGE_DATA*/", page)
        self.assertIn(self.data["libraries"][0]["name"], page)


if __name__ == "__main__":
    unittest.main()
