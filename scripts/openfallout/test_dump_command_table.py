#!/usr/bin/env python3
"""Checks dump_command_table.py on a small executable that this builds: one section with a table of three commands."""
import contextlib
import io
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dump_command_table as dump  # noqa: E402

IMAGE_BASE = 0x400000
SECTION_VA = 0x1000
SECTION_RAW = 0x400


def build_executable(stride=0x28, entries=3, broken=()):
    """A PE32 file whose one section holds the strings, parameter tables and a table of commands."""
    section = bytearray()

    def add(blob):
        offset = len(section)
        section.extend(blob)
        while len(section) % 4:
            section.append(0)
        return IMAGE_BASE + SECTION_VA + offset

    names = [("Activate", "act"), ("GetStage", ""), ("SetStage", "")]
    pointers = [(add(n.encode() + b"\0"), add(s.encode() + b"\0") if s else 0) for n, s in names]
    label = add(b"Quest\0")
    one_param = add(struct.pack("<III", label, 0x0E, 0))
    two_params = add(struct.pack("<III", label, 0x0E, 0) + struct.pack("<III", label, 0x17, 1))
    help_text = add(b"help\0")
    params = [0, one_param, two_params]
    counts = [0, 1, 2]
    parsers = [IMAGE_BASE + SECTION_VA, IMAGE_BASE + SECTION_VA, IMAGE_BASE + SECTION_VA + 4]
    table = bytearray()
    for i in range(entries):
        kind = i % 3  # the names, parameters and parse functions are used over again for a longer table
        # a command whose name pointer leads nowhere is not taken for a command
        name = 0 if i in broken else pointers[kind][0]
        entry = struct.pack("<IIIIHHIIIII", name, pointers[kind][1], 0x1000 + i, help_text, 0, counts[kind],
                            params[kind], IMAGE_BASE + SECTION_VA, parsers[kind], 0, 0)
        table.extend(entry[:stride])
    add(table)
    # Something that is not a table, to be skipped
    add(struct.pack("<IIII", 1, 2, 3, 4) * 8)

    header = bytearray(SECTION_RAW)
    header[0:2] = b"MZ"
    struct.pack_into("<I", header, 0x3C, 0x80)
    header[0x80:0x84] = b"PE\0\0"
    optional_size = 224
    struct.pack_into("<HHIIIHH", header, 0x84, 0x14C, 1, 0, 0, 0, optional_size, 0)
    optional = 0x84 + 20
    struct.pack_into("<H", header, optional, 0x10B)
    struct.pack_into("<I", header, optional + 28, IMAGE_BASE)
    struct.pack_into("<I", header, optional + 56, 0x10000)
    struct.pack_into("<8sIIII", header, optional + optional_size, b".data", len(section), SECTION_VA, len(section),
                     SECTION_RAW)
    return bytes(header) + bytes(section)


class DumpCommandTableTest(unittest.TestCase):
    def dump(self, data, *extra):
        with tempfile.TemporaryDirectory() as folder:
            exe = os.path.join(folder, "game.exe")
            csv_path = os.path.join(folder, "table.csv")
            with open(exe, "wb") as stream:
                stream.write(data)
            result = dump.main([exe, "--csv", csv_path, "--minimum", "3", *extra])
            rows = []
            if os.path.exists(csv_path):
                with open(csv_path) as stream:
                    rows = [line.rstrip("\n").split(",") for line in stream]
            return result, rows

    def test_finds_the_table_and_its_parameters(self):
        result, rows = self.dump(build_executable())
        self.assertEqual(result, 0)
        self.assertEqual(rows, [
            ["0", "0x1000", "Activate", "act", "0", "", "0x0", "0x401000"],
            ["0", "0x1001", "GetStage", "", "0", "14", "0x0", "0x401000"],
            ["0", "0x1002", "SetStage", "", "0", "14 23?", "0x0", "0x401004"],
        ])

    def test_finds_a_table_with_shorter_entries(self):
        result, rows = self.dump(build_executable(stride=0x24), "--stride", "0x24")
        self.assertEqual(result, 0)
        self.assertEqual([r[2] for r in rows], ["Activate", "GetStage", "SetStage"])

    def test_keeps_the_commands_between_entries_that_are_not_commands(self):
        # a table of twelve with dummies at 6, 8 and 10: the run of 0 to 5 is a table, 7 and 9 are runs of one each,
        # and 11 stands after the last dummy. They belong to the table all the same.
        result, rows = self.dump(build_executable(entries=12, broken=(6, 8, 10)), "--minimum", "5")
        self.assertEqual(result, 0)
        self.assertEqual([r[1] for r in rows], ["0x%X" % (0x1000 + i) for i in (0, 1, 2, 3, 4, 5, 7, 9, 11)])

    def test_keeps_the_commands_on_both_sides_of_a_long_run(self):
        # a stray before a run of 69 and one after it: the one after is close to the run, and the one before must not
        # make it look far
        result, rows = self.dump(build_executable(entries=73, broken=(1, 71)), "--minimum", "5")
        self.assertEqual(result, 0)
        self.assertEqual([r[1] for r in rows], ["0x%X" % (0x1000 + i) for i in [0, *range(2, 71), 72]])

    def test_leaves_out_a_command_that_is_far_from_every_table(self):
        # entry 5 stands far from the run of 0 to 2 and has a code that the table does not come near
        data = bytearray(build_executable(entries=8, broken=(3, 4, 6, 7)))
        far = data.find(struct.pack("<I", 0x1005), SECTION_RAW)
        struct.pack_into("<I", data, far, 0x3777)
        result, rows = self.dump(bytes(data), "--minimum", "3")
        self.assertEqual(result, 0)
        self.assertEqual([r[1] for r in rows], ["0x1000", "0x1001", "0x1002"])

    def test_reports_a_file_with_no_table(self):
        data = bytearray(build_executable())
        data[SECTION_RAW:] = bytes(len(data) - SECTION_RAW)
        result, rows = self.dump(bytes(data))
        self.assertNotEqual(result, 0)
        self.assertEqual(rows, [])

    def test_a_section_that_is_longer_than_the_file_does_not_break_it(self):
        data = bytearray(build_executable())
        # the section header says the section holds a thousand bytes more than the file has
        section_header = 0x84 + 20 + 224
        size = struct.unpack_from("<I", data, section_header + 16)[0]
        struct.pack_into("<I", data, section_header + 16, size + 1000)
        result, rows = self.dump(bytes(data))
        self.assertEqual(result, 0)
        self.assertEqual([r[2] for r in rows], ["Activate", "GetStage", "SetStage"])

    def test_probe_says_why_a_structure_is_not_a_command(self):
        data = bytearray(build_executable())
        # the second command loses its name: the table breaks there, but the probe still finds its code
        entry = data.find(struct.pack("<I", 0x1001), SECTION_RAW) - 8
        struct.pack_into("<I", data, entry, 0)
        with tempfile.TemporaryDirectory() as folder:
            exe = os.path.join(folder, "game.exe")
            with open(exe, "wb") as stream:
                stream.write(bytes(data))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                result = dump.main([exe, "--probe", "0x1001,0x1002,0x1777"])
        text = out.getvalue()
        self.assertEqual(result, 0)
        self.assertIn("code 0x1001 at file offset", text)
        self.assertIn("not a command", text)
        self.assertIn("rejected: the name pointer 0x0 does not lead to a short printable string", text)
        self.assertIn("code 0x1002 at file offset", text)
        self.assertIn("SetStage ()", text)
        self.assertIn("code 0x1777: no structure with this code", text)

    def test_refuses_what_is_not_an_executable(self):
        with self.assertRaises(SystemExit):
            self.dump(b"not an executable at all")


if __name__ == "__main__":
    unittest.main()
