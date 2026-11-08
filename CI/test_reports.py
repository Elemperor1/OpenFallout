#!/usr/bin/env python3
"""Collect isolated coverage and JUnit results for each Ubuntu test suite."""

import argparse
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


SUITES = {
    "components": "components-tests",
    "openfallout": "openfallout-tests",
    "editor": "openfallout-cs-tests",
}


def clear_counters(build):
    # Preserve compiled objects and .gcno metadata for incremental builds.
    for path in build.rglob("*.gcda"):
        path.unlink()


def write_junit(source, destination, root):
    """Normalize GoogleTest's XML, including disabled tests, to JUnit XML."""
    original = ET.parse(source).getroot()
    report = ET.Element("testsuites")
    totals = {key: 0 for key in ("tests", "failures", "errors", "skipped")}
    for original_suite in original.findall("testsuite"):
        suite = ET.SubElement(report, "testsuite", {
            key: original_suite.get(key, default) for key, default in (
                ("name", ""), ("time", "0"), ("timestamp", ""))
        })
        counts = dict.fromkeys(totals, 0)
        for original_case in original_suite.findall("testcase"):
            attributes = {key: original_case.get(key, default) for key, default in (
                ("name", ""), ("classname", original_suite.get("name", "")), ("time", "0"))}
            if original_case.get("file"):
                path = Path(original_case.get("file"))
                if path.is_absolute() and path.is_relative_to(root):
                    path = path.relative_to(root)
                attributes["file"] = path.as_posix()
            case = ET.SubElement(suite, "testcase", attributes)
            for child in original_case:
                if child.tag in ("failure", "error", "skipped", "properties", "system-out", "system-err"):
                    case.append(copy.deepcopy(child))
            if (original_case.get("status") == "notrun"
                    or original_case.get("result") == "suppressed") and case.find("skipped") is None:
                ET.SubElement(case, "skipped", {"message": "Disabled in GoogleTest"})
            counts["tests"] += 1
            for tag, key in (("failure", "failures"), ("error", "errors"), ("skipped", "skipped")):
                counts[key] += int(case.find(tag) is not None)
        suite.attrib.update({key: str(value) for key, value in counts.items()})
        for key, value in counts.items():
            totals[key] += value
    if not totals["tests"]:
        raise ValueError("GoogleTest did not report any test cases")
    report.attrib.update({key: str(value) for key, value in totals.items()})
    report.set("time", original.get("time", "0"))
    ET.indent(report)
    ET.ElementTree(report).write(destination, encoding="utf-8", xml_declaration=True)


def collect_suite(root, suite, gcovr="gcovr"):
    build = root / "build"
    binary = build / SUITES[suite]
    if not binary.is_file():
        raise FileNotFoundError(f"Test binary does not exist: {binary}")
    results = root / "test-results"
    coverage = root / "coverage"
    results.mkdir(exist_ok=True)
    coverage.mkdir(exist_ok=True)
    raw = results / f"{suite}.gtest.xml"
    junit = results / f"{suite}.junit.xml"
    xml = coverage / f"{suite}.xml"
    trace = coverage / f"{suite}.json"
    for path in (raw, junit, xml, trace):
        path.unlink(missing_ok=True)
    clear_counters(build)
    try:
        tests = subprocess.run([str(binary), f"--gtest_output=xml:{raw}"], cwd=root)
        junit_status = 0
        try:
            write_junit(raw, junit, root)
        except (OSError, ET.ParseError, ValueError) as error:
            print(f"Could not generate {suite} JUnit results: {error}", file=sys.stderr)
            junit_status = 1
        # Collect coverage even on test failure, so failure reports remain useful.
        collected = subprocess.run([
            gcovr, "--root", ".", "build",
            "--exclude", "extern/", "--exclude", "build/",
            "--exclude", "apps/[^/]*_tests/",
            "--merge-mode-functions", "merge-use-line-max",
            "--exclude-unreachable-branches",
            # The compiler adds a hidden exception exit to every call that may throw. No test takes it, and
            # Codecov counts a line with an untaken branch as partial, not covered. gcovr drops these branches
            # only while it reads the gcov data, so the traces that the merge reads must be made with this flag.
            "--exclude-throw-branches",
            "--gcov-ignore-parse-errors=negative_hits.warn_once_per_file",
            "-j", str(os.cpu_count() or 1),
            "--xml-pretty", "--xml", str(xml), "--json", str(trace), "--print-summary",
        ], cwd=root)
        if collected.returncode:
            # Never upload a partially written coverage report.
            xml.unlink(missing_ok=True)
            trace.unlink(missing_ok=True)
        return tests.returncode or junit_status or collected.returncode
    finally:
        clear_counters(build)


def merge_coverage(root, gcovr="gcovr"):
    command = [gcovr, "--root", ".", "--merge-mode-functions", "merge-use-line-max"]
    for suite in SUITES:
        trace = root / "coverage" / f"{suite}.json"
        if not trace.is_file():
            raise FileNotFoundError(f"Missing coverage trace: {trace}")
        command.extend(("--add-tracefile", str(trace)))
    command.extend(("--xml-pretty", "--output", "coverage.xml", "--print-summary"))
    return subprocess.run(command, cwd=root).returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("run").add_argument("suite", choices=SUITES)
    commands.add_parser("merge")
    args = parser.parse_args()
    root = Path.cwd().resolve()
    try:
        if args.command == "run":
            return collect_suite(root, args.suite)
        return merge_coverage(root)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
