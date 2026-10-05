"""Verify restored builds reuse objects and rebuild changed source/header inputs."""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


spec = importlib.util.spec_from_file_location("build_cache", Path(__file__).with_name("build_cache.py"))
cache = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache)


@unittest.skipUnless(all(shutil.which(tool) for tool in ("cmake", "ninja", "cc", "git")),
                     "requires CMake, Ninja, a C compiler and Git")
class BuildCacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.build = self.root / "build"
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.16)\nproject(cache_probe C)\n'
            'add_executable(probe main.c value.c)\n'
        )
        (self.root / "value.h").write_text('#define VALUE 1\n')
        (self.root / "value.c").write_text('#include "value.h"\nint value(void) { return VALUE; }\n')
        (self.root / "main.c").write_text('int value(void);\nint main(void) { return value() != 1; }\n')
        self.run_command("git", "init", "-q")
        self.run_command("git", "add", ".")
        self.configure_and_build()
        cache.snapshot(self.root, self.build)
        self.objects = sorted(self.build.rglob("*.o"))
        self.assertEqual(len(self.objects), 2)
        self.original_times = [path.stat().st_mtime_ns for path in self.objects]

    def run_command(self, *command):
        result = subprocess.run(command, cwd=self.root, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            self.fail(f"{' '.join(command)} failed:\n{result.stdout}")
        return result.stdout

    def configure_and_build(self):
        self.run_command("cmake", "-S", ".", "-B", "build", "-G", "Ninja")
        return self.run_command("cmake", "--build", "build")

    def emulate_checkout(self):
        for name in ("CMakeLists.txt", "main.c", "value.c", "value.h"):
            os.utime(self.root / name, None)

    def test_unchanged_checkout_reuses_all_objects(self):
        self.emulate_checkout()
        cache.restore(self.root, self.build)
        self.configure_and_build()
        self.assertEqual(self.original_times,
                         [path.stat().st_mtime_ns for path in self.objects])
        self.run_command(str(self.build / "probe"))

    def test_header_content_change_rebuilds_only_affected_object(self):
        self.emulate_checkout()
        header = self.root / "value.h"
        header.write_text('#define VALUE 2\n')
        # Deliberately give changed content an old timestamp; content decides.
        os.utime(header, ns=(1_000_000_000, 1_000_000_000))
        cache.restore(self.root, self.build)
        self.configure_and_build()
        times = [path.stat().st_mtime_ns for path in self.objects]
        self.assertEqual(sum(a != b for a, b in zip(self.original_times, times)), 1)
        result = subprocess.run([str(self.build / "probe")])
        self.assertEqual(result.returncode, 1, "executable must use the changed header")

    def test_changed_source_rebuilds_even_with_same_size_and_old_timestamp(self):
        self.emulate_checkout()
        source = self.root / "main.c"
        source.write_text('int value(void);\nint main(void) { return value() != 2; }\n')
        os.utime(source, ns=(1_000_000_000, 1_000_000_000))
        cache.restore(self.root, self.build)
        self.configure_and_build()
        self.assertEqual(sum(a != path.stat().st_mtime_ns
                             for a, path in zip(self.original_times, self.objects)), 1)
        self.assertEqual(subprocess.run([str(self.build / "probe")]).returncode, 1)

    def test_dependency_and_configuration_changes_invalidate_build_key(self):
        dependency = self.root / "manifest.txt"
        dependency.write_text("dependencies-v1")
        first = cache.build_key(self.root, "cc", [dependency], "Debug")
        self.assertEqual(first, cache.build_key(self.root, "cc", [dependency], "Debug"))
        self.assertNotEqual(first, cache.build_key(self.root, "cc", [dependency], "Release"))
        dependency.write_text("dependencies-v2")
        self.assertNotEqual(first, cache.build_key(self.root, "cc", [dependency], "Debug"))
        dependency.write_text("dependencies-v1")
        with (self.root / "CMakeLists.txt").open("a") as stream:
            stream.write("\nadd_compile_definitions(CHANGED_RULES)\n")
        self.assertNotEqual(first, cache.build_key(self.root, "cc", [dependency], "Debug"))

    def test_incompatible_snapshot_is_discarded(self):
        (self.build / cache.STATE_FILE).write_text('{"version": 0}')
        cache.restore(self.root, self.build)
        self.assertFalse(self.build.exists())


if __name__ == "__main__":
    unittest.main()
