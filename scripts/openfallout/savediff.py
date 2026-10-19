#!/usr/bin/env python3
"""Compares the state of Fallout in two saved games, section by section.

A saved game is a stream of records (a name, a size, eight bytes of flags, then subrecords); the state of Fallout is in the
records called FOSV, one for each section, with the name of the section (NAME), its version (VERS) and its bytes (DATA). The
sections are written in the order of their keys and without anything that depends on the run, so a game that is loaded and
saved again at once has the same bytes in every section, except for what really changed in between (the clock, a timer, a
script that ran). This prints, for each section, whether the two saves agree, and where they first differ.

    savediff.py first.omwsave second.omwsave [--ignore NAME ...]

The exit status is 0 when every section that is not ignored is the same in both, 1 when one differs or is missing, 2 when a file
cannot be read.
"""
import argparse
import struct
import sys


class SaveFileError(Exception):
    pass


def subrecords(body):
    offset = 0
    while offset < len(body):
        if offset + 8 > len(body):
            raise SaveFileError("a subrecord is cut short")
        name = body[offset:offset + 4].decode("latin-1")
        (size,) = struct.unpack_from("<I", body, offset + 4)
        if offset + 8 + size > len(body):
            raise SaveFileError(f"the subrecord {name} is cut short")
        yield name, body[offset + 8:offset + 8 + size]
        offset += 8 + size


def records(data):
    offset = 0
    while offset < len(data):
        if offset + 16 > len(data):
            raise SaveFileError("a record is cut short")
        name = data[offset:offset + 4].decode("latin-1")
        (size,) = struct.unpack_from("<I", data, offset + 4)
        start = offset + 16
        if start + size > len(data):
            raise SaveFileError(f"the record {name} is cut short")
        yield name, data[start:start + size]
        offset = start + size


def sections(path):
    """The sections of the state of Fallout in the file, in the order they were written: (name, version, bytes)."""
    try:
        with open(path, "rb") as stream:
            data = stream.read()
        found = []
        counts = {}
        for name, body in records(data):
            counts[name] = counts.get(name, 0) + 1
            if name != "FOSV":
                continue
            section, version, payload = None, None, None
            for sub, content in subrecords(body):
                if sub == "NAME":
                    section = content.rstrip(b"\0").decode("latin-1")
                elif sub == "VERS":
                    (version,) = struct.unpack("<I", content)
                elif sub == "DATA":
                    payload = content
            if section is None or payload is None:
                raise SaveFileError("a FOSV record has no name or no data")
            found.append((section, version, payload))
        return found, counts
    except (OSError, struct.error, SaveFileError) as error:
        raise SaveFileError(f"{path}: {error}")


def first_difference(first, second):
    for index in range(min(len(first), len(second))):
        if first[index] != second[index]:
            return index
    return min(len(first), len(second))


def compare(first_path, second_path, ignore=(), out=sys.stdout):
    """Prints the comparison and returns the number of sections that differ or are missing from one of the files."""
    first, first_counts = sections(first_path)
    second, second_counts = sections(second_path)
    if not first and not second:
        print("neither file has a section of the state of Fallout", file=out)
        return 1
    names = []
    for name, _, _ in first + second:
        if name not in names:
            names.append(name)
    left = {name: (version, payload) for name, version, payload in first}
    right = {name: (version, payload) for name, version, payload in second}
    problems = 0
    for name in names:
        ignored = name in ignore
        if name not in left or name not in right:
            where = "the first" if name not in left else "the second"
            print(f"{name:16} missing from {where} file" + ("  (ignored)" if ignored else ""), file=out)
            problems += 0 if ignored else 1
            continue
        (version_a, payload_a), (version_b, payload_b) = left[name], right[name]
        if version_a == version_b and payload_a == payload_b:
            print(f"{name:16} same       v{version_a} {len(payload_a)} bytes", file=out)
            continue
        where = first_difference(payload_a, payload_b)
        same = sum(1 for a, b in zip(payload_a, payload_b) if a == b)
        print(f"{name:16} DIFFERS    v{version_a}/v{version_b} {len(payload_a)}/{len(payload_b)} bytes, first at "
              f"{where}, {same} of {min(len(payload_a), len(payload_b))} bytes equal" + ("  (ignored)" if ignored else ""),
              file=out)
        problems += 0 if ignored else 1
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("first")
    parser.add_argument("second")
    parser.add_argument("--ignore", action="append", default=[], metavar="NAME", help="a section that may differ")
    args = parser.parse_args(argv)
    try:
        problems = compare(args.first, args.second, set(args.ignore))
    except SaveFileError as error:
        print(error, file=sys.stderr)
        return 2
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
