"""Verify per-suite isolation and faithful JUnit reporting, including failures."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET


spec = importlib.util.spec_from_file_location("reports", Path(__file__).with_name("test_reports.py"))
reports = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reports)


class TestReportsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.build = self.root / "build"
        self.build.mkdir()
        for binary in reports.SUITES.values():
            (self.build / binary).touch()
        self.xml = '''<testsuites time="1.25"><testsuite name="Sample" time="1.25">
          <testcase name="passes" classname="Sample" time="0.25"/>
          <testcase name="fails" classname="Sample" time="1"><failure message="failed">details</failure></testcase>
          <testcase name="skips" classname="Sample" time="0"><skipped message="unavailable"/></testcase>
          <testcase name="DISABLED_case" classname="Sample" time="0" status="notrun" result="suppressed"/>
        </testsuite></testsuites>'''

    def test_junit_preserves_failures_skips_names_and_durations(self):
        source = self.root / "gtest.xml"
        source.write_text(self.xml)
        destination = self.root / "junit.xml"
        reports.write_junit(source, destination, self.root)
        root = ET.parse(destination).getroot()
        self.assertEqual({key: root.get(key) for key in ("tests", "failures", "errors", "skipped")},
                         {"tests": "4", "failures": "1", "errors": "0", "skipped": "2"})
        cases = root.findall("./testsuite/testcase")
        self.assertEqual(cases[0].get("time"), "0.25")
        self.assertEqual(cases[1].find("failure").text, "details")
        self.assertEqual(cases[2].find("skipped").get("message"), "unavailable")
        self.assertIsNotNone(cases[3].find("skipped"))
        self.assertIsNone(cases[3].get("status"))

    def test_junit_uses_repository_relative_source_paths(self):
        source = self.root / "gtest.xml"
        source.write_text(f'''<testsuites><testsuite name="Sample">
            <testcase name="test" file="{self.root}/apps/components_tests/sample.cpp"/>
            </testsuite></testsuites>''')
        destination = self.root / "junit.xml"
        reports.write_junit(source, destination, self.root)
        case = ET.parse(destination).find("./testsuite/testcase")
        self.assertEqual(case.get("file"), "apps/components_tests/sample.cpp")
        self.assertEqual(case.get("classname"), "Sample")

    def test_junit_rejects_empty_reports(self):
        source = self.root / "gtest.xml"
        source.write_text("<testsuites/>")
        with self.assertRaises(ValueError):
            reports.write_junit(source, self.root / "junit.xml", self.root)

    def fake_run(self, command, cwd, test_exit=0, coverage_exit=0, xml=None):
        if command[0] == "gcovr":
            self.assertEqual((self.build / "counter.gcda").read_text(), "current suite")
            Path(command[command.index("--xml") + 1]).write_text("coverage")
            Path(command[command.index("--json") + 1]).write_text("trace")
            return subprocess.CompletedProcess(command, coverage_exit)
        self.assertFalse((self.build / "counter.gcda").exists(), "stale counters were not cleared")
        (self.build / "counter.gcda").write_text("current suite")
        output = Path(command[1].split("xml:", 1)[1])
        output.write_text(self.xml if xml is None else xml)
        return subprocess.CompletedProcess(command, test_exit)

    def test_suites_have_isolated_counters_reports_and_reusable_objects(self):
        (self.build / "counter.gcda").write_text("old run")
        for extension in ("gcno", "o"):
            (self.build / f"keep.{extension}").write_text("keep")
        with patch.object(reports.subprocess, "run", side_effect=self.fake_run):
            for suite in reports.SUITES:
                self.assertEqual(reports.collect_suite(self.root, suite), 0)
                self.assertFalse(list(self.build.rglob("*.gcda")))
                self.assertTrue((self.root / "test-results" / f"{suite}.junit.xml").is_file())
                self.assertTrue((self.root / "coverage" / f"{suite}.json").is_file())
        for extension in ("gcno", "o"):
            self.assertEqual((self.build / f"keep.{extension}").read_text(), "keep")

    def test_failing_tests_still_produce_junit_and_coverage_and_fail_the_job(self):
        with patch.object(reports.subprocess, "run", side_effect=lambda command, cwd:
                          self.fake_run(command, cwd, test_exit=7)):
            self.assertEqual(reports.collect_suite(self.root, "components"), 7)
        self.assertTrue((self.root / "test-results/components.junit.xml").is_file())
        self.assertTrue((self.root / "coverage/components.xml").is_file())
        self.assertFalse(list(self.build.rglob("*.gcda")))

    def test_malformed_junit_keeps_raw_xml_and_still_collects_coverage(self):
        with patch.object(reports.subprocess, "run", side_effect=lambda command, cwd:
                          self.fake_run(command, cwd, xml="broken")):
            self.assertEqual(reports.collect_suite(self.root, "editor"), 1)
        self.assertTrue((self.root / "test-results/editor.gtest.xml").is_file())
        self.assertFalse((self.root / "test-results/editor.junit.xml").exists())
        self.assertTrue((self.root / "coverage/editor.xml").is_file())

    def test_coverage_failure_discards_partial_reports_but_keeps_junit(self):
        with patch.object(reports.subprocess, "run", side_effect=lambda command, cwd:
                          self.fake_run(command, cwd, coverage_exit=3)):
            self.assertEqual(reports.collect_suite(self.root, "components"), 3)
        self.assertFalse((self.root / "coverage/components.xml").exists())
        self.assertFalse((self.root / "coverage/components.json").exists())
        self.assertTrue((self.root / "test-results/components.junit.xml").is_file())
        self.assertFalse(list(self.build.rglob("*.gcda")))

    def test_collection_exception_still_clears_counters(self):
        def failed_collection(command, cwd):
            if command[0] == "gcovr":
                raise OSError("gcovr unavailable")
            return self.fake_run(command, cwd)
        with patch.object(reports.subprocess, "run", side_effect=failed_collection):
            with self.assertRaises(OSError):
                reports.collect_suite(self.root, "components")
        self.assertFalse(list(self.build.rglob("*.gcda")))

    def test_combined_report_requires_every_suite_trace(self):
        with self.assertRaises(FileNotFoundError):
            reports.merge_coverage(self.root)
        (self.root / "coverage").mkdir()
        for suite in reports.SUITES:
            (self.root / "coverage" / f"{suite}.json").write_text("trace")
        with patch.object(reports.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run:
            self.assertEqual(reports.merge_coverage(self.root), 0)
        command = run.call_args.args[0]
        self.assertEqual(command.count("--add-tracefile"), 3)
        self.assertNotIn("build", command, "merge must use saved traces, not cleared live counters")


if __name__ == "__main__":
    unittest.main()
