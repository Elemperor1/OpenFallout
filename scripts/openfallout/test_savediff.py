#!/usr/bin/env python3
"""Checks savediff.py on saved games that this builds: a header record and the records of two sections."""
import contextlib
import io
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import savediff  # noqa: E402


def sub(name, payload):
    return name.encode() + struct.pack("<I", len(payload)) + payload


def record(name, *subs):
    body = b"".join(subs)
    return name.encode() + struct.pack("<I", len(body)) + b"\0" * 8 + body


def section(name, version, payload):
    return record("FOSV", sub("NAME", name.encode() + b"\0"), sub("VERS", struct.pack("<I", version)), sub("DATA", payload))


def save(*sections):
    return record("TES3", sub("HEDR", b"\0" * 4)) + b"".join(sections)


class SaveDiffTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def write(self, name, data):
        path = os.path.join(self.directory.name, name)
        with open(path, "wb") as stream:
            stream.write(data)
        return path

    def compare(self, first, second, ignore=()):
        out = io.StringIO()
        problems = savediff.compare(self.write("a", first), self.write("b", second), ignore, out)
        return problems, out.getvalue()

    def test_the_sections_are_read_in_the_order_they_were_written(self):
        path = self.write("a", save(section("globals", 1, b"abc"), section("quests", 2, b"")))
        found, counts = savediff.sections(path)
        self.assertEqual([("globals", 1, b"abc"), ("quests", 2, b"")], found)
        self.assertEqual({"TES3": 1, "FOSV": 2}, counts)

    def test_equal_saves_have_no_problems(self):
        data = save(section("globals", 1, b"abc"), section("quests", 1, b"xyz"))
        problems, text = self.compare(data, data)
        self.assertEqual(0, problems)
        self.assertIn("globals", text)
        self.assertNotIn("DIFFERS", text)

    def test_a_section_that_differs_is_told_with_the_place(self):
        problems, text = self.compare(save(section("globals", 1, b"abcdef")), save(section("globals", 1, b"abcxef")))
        self.assertEqual(1, problems)
        self.assertIn("DIFFERS", text)
        self.assertIn("first at 3", text)

    def test_a_section_of_another_version_or_size_differs(self):
        problems, _ = self.compare(save(section("a", 1, b"ab")), save(section("a", 2, b"ab")))
        self.assertEqual(1, problems)
        problems, text = self.compare(save(section("a", 1, b"ab")), save(section("a", 1, b"abc")))
        self.assertEqual(1, problems)
        self.assertIn("first at 2", text)

    def test_a_missing_section_is_a_problem(self):
        problems, text = self.compare(save(section("a", 1, b"1"), section("b", 1, b"2")), save(section("a", 1, b"1")))
        self.assertEqual(1, problems)
        self.assertIn("missing from the second file", text)

    def test_an_ignored_section_may_differ(self):
        problems, text = self.compare(save(section("a", 1, b"1")), save(section("a", 1, b"2")), {"a"})
        self.assertEqual(0, problems)
        self.assertIn("(ignored)", text)

    def test_a_file_that_is_cut_short_cannot_be_read(self):
        path = self.write("cut", save(section("a", 1, b"1"))[:-3])
        with self.assertRaises(savediff.SaveFileError):
            savediff.sections(path)

    def test_the_exit_status_follows_the_problems(self):
        first = self.write("a", save(section("a", 1, b"1")))
        second = self.write("b", save(section("a", 1, b"2")))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(0, savediff.main([first, first]))
            self.assertEqual(1, savediff.main([first, second]))
            self.assertEqual(0, savediff.main([first, second, "--ignore", "a"]))
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(2, savediff.main([first, os.path.join(self.directory.name, "missing")]))


if __name__ == "__main__":
    unittest.main()
