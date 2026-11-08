#!/usr/bin/env python3
"""Decide whether a change needs the platform builds and the tests.

Only a change that touches nothing but the notes and trackers under docs/openfallout,
the helper scripts under scripts/openfallout (their Python tests run in the Changes job
of push.yml, whatever else changed) and the top-level README, CHANGELOG and AUTHORS may
skip them.  A skipped build reports green, so every
doubt runs everything: an empty or unreadable list of changed files, a base commit that
cannot be fetched, a manual run, a deleted file outside the two folders, and any change
to CI itself.
"""

import argparse
import os
import subprocess
import sys

# No build reads these folders, and the tests of the Python helpers run in the Changes job.
DOCUMENTATION_TREES = ("docs/openfallout/", "scripts/openfallout/")
# CMakeLists.txt installs these files, so changing one is harmless but removing one is not.
DOCUMENTATION_FILES = ("README.md", "CHANGELOG.md", "AUTHORS.md")


def skippable(status, path):
    """Whether a changed file can be left out of the build, from git's one-letter status."""
    if path.startswith(DOCUMENTATION_TREES):
        return True
    return status in ("A", "M") and path in DOCUMENTATION_FILES


def needs_build(changes):
    """changes is a list of (status, path) pairs, as git diff --name-status --no-renames gives."""
    if not changes:
        return True
    return not all(skippable(status, path) for status, path in changes)


def diff_changes(base, head="HEAD", cwd=None):
    out = subprocess.check_output(
        ["git", "diff", "--name-status", "--no-renames", "-z", base, head], cwd=cwd
    )
    parts = out.split(b"\0")
    if parts and parts[-1] == b"":
        parts.pop()
    if len(parts) % 2:
        raise ValueError("unexpected output of git diff")
    return [
        (parts[i].decode(), os.fsdecode(parts[i + 1])) for i in range(0, len(parts), 2)
    ]


def decide(event, base, cwd=None):
    """Return (needs_build, reason).  Fails closed: any problem means build."""
    try:
        if event not in ("push", "pull_request"):
            return True, "a %s run always builds" % (event or "manual")
        if not base or set(base) == {"0"}:
            return True, "there is no base commit to compare with"
        subprocess.run(
            ["git", "fetch", "--no-tags", "--depth=1", "origin", base], check=True, cwd=cwd
        )
        changes = diff_changes(base, "HEAD", cwd)
        if not changes:
            return True, "the list of changed files is empty"
        other = [path for status, path in changes if not skippable(status, path)]
        if other:
            return True, "%d of %d changed files are not documentation, first %s" % (
                len(other), len(changes), other[0])
        return False, "only documentation and notes changed (%d files)" % len(changes)
    except Exception as error:  # noqa: BLE001 - a wrong skip is worse than a slow run
        return True, "could not list the changed files (%s)" % error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", default="")
    parser.add_argument("--base", default="")
    parser.add_argument("--github-output", default="")
    args = parser.parse_args()
    build, reason = decide(args.event, args.base)
    print("builds needed: %s, because %s" % ("yes" if build else "no", reason))
    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as output:
            output.write("code=%s\n" % ("true" if build else "false"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
