#!/usr/bin/env python3
"""Tests of the rule that lets documentation-only changes skip the builds.

A wrong skip reports green without building, so the tests hide a code change among
documentation on purpose, as a real run would meet it.
"""

import os
import subprocess
import tempfile
import unittest

import changed_paths


def modified(*paths):
    return [("M", path) for path in paths]


class ClassificationTest(unittest.TestCase):
    def test_documentation_only_skips(self):
        self.assertFalse(changed_paths.needs_build(modified(
            "docs/openfallout/roadmap.md",
            "docs/openfallout/coverage/tracker/menus.csv",
            "scripts/openfallout/coverage_dashboard.py",
            "README.md",
        )))

    def test_added_and_deleted_notes_skip(self):
        self.assertFalse(changed_paths.needs_build([
            ("A", "docs/openfallout/new-note.md"),
            ("D", "scripts/openfallout/old_tool.py"),
        ]))

    def test_code_hidden_among_documentation_builds(self):
        self.assertTrue(changed_paths.needs_build(modified(
            "docs/openfallout/roadmap.md",
            "components/esm4/reader.cpp",
            "scripts/openfallout/coverage_dashboard.py",
        )))

    def test_empty_list_builds(self):
        self.assertTrue(changed_paths.needs_build([]))

    def test_ci_changes_build(self):
        for path in (".github/workflows/push.yml", "CI/build_cache.py", "CMakeLists.txt",
                     "files/data/CMakeLists.txt", "apps/openfallout/main.cpp",
                     "files/data/scripts/omw/ui.lua", "manual/index.rst"):
            with self.subTest(path=path):
                self.assertTrue(changed_paths.needs_build(modified(path)))

    def test_removing_an_installed_file_builds(self):
        self.assertTrue(changed_paths.needs_build([("D", "README.md")]))
        self.assertTrue(changed_paths.needs_build([("T", "CHANGELOG.md")]))

    def test_lookalike_folders_build(self):
        for path in ("docs/openfalloutx/a.md", "scripts/openfallout.py",
                     "docs/openfallout", "docs/README.md", "sub/README.md"):
            with self.subTest(path=path):
                self.assertTrue(changed_paths.needs_build(modified(path)))


class GitTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.git("init", "-q")
        self.write("docs/openfallout/a.md", "one")
        self.write("apps/main.cpp", "int main() {}")
        self.commit("first")
        self.base = self.git("rev-parse", "HEAD").strip()

    def git(self, *args):
        return subprocess.check_output(
            ["git", "-c", "user.name=t", "-c", "user.email=t@example.com", *args],
            cwd=self.directory.name, text=True)

    def write(self, path, text):
        full = os.path.join(self.directory.name, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as handle:
            handle.write(text)

    def commit(self, message):
        self.git("add", "-A")
        self.git("commit", "-q", "-m", message)

    def test_diff_lists_status_and_path(self):
        self.write("docs/openfallout/a.md", "two")
        self.write("docs/openfallout/b.md", "new")
        self.commit("second")
        self.assertEqual(
            sorted(changed_paths.diff_changes(self.base, "HEAD", self.directory.name)),
            [("A", "docs/openfallout/b.md"), ("M", "docs/openfallout/a.md")])

    def test_hidden_code_change_is_seen(self):
        self.write("docs/openfallout/a.md", "two")
        self.write("apps/main.cpp", "int main() { return 1; }")
        self.commit("second")
        changes = changed_paths.diff_changes(self.base, "HEAD", self.directory.name)
        self.assertTrue(changed_paths.needs_build(changes))

    def test_unfetchable_base_builds(self):
        # The repository has no origin, so fetching the base fails; that must build.
        build, reason = changed_paths.decide("pull_request", self.base, self.directory.name)
        self.assertTrue(build, reason)

    def test_missing_base_and_manual_runs_build(self):
        for event, base in (("pull_request", ""), ("push", "0" * 40),
                            ("workflow_dispatch", self.base), ("", self.base)):
            with self.subTest(event=event, base=base):
                self.assertTrue(changed_paths.decide(event, base, self.directory.name)[0])


if __name__ == "__main__":
    unittest.main()
