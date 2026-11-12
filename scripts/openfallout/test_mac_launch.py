"""Tests of mac_launch.py against a stand-in for macOS's open command."""
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest

HELPER = Path(__file__).resolve().parent / "mac_launch.py"

# Records its arguments as JSON and exits with $OPEN_STATUS (0 when unset).
STUB = """#!/usr/bin/env python3
import json, os, sys
with open(os.environ["OPEN_RECORD"], "w") as f:
    json.dump(sys.argv[1:], f)
sys.exit(int(os.environ.get("OPEN_STATUS", "0")))
"""


class MacLaunchTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="mac launch ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        bin_dir = self.root / "bin"
        bin_dir.mkdir()
        stub = bin_dir / "open"
        stub.write_text(STUB)
        stub.chmod(0o755)
        self.record = self.root / "open-args.json"
        self.env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"],
                        OPEN_RECORD=str(self.record))
        self.app = self.root / "OpenFallout.app"

    def launch(self, *args, status=0):
        env = dict(self.env, OPEN_STATUS=str(status))
        result = subprocess.run([sys.executable, str(HELPER), *args], cwd=self.root, env=env,
                                text=True, capture_output=True)
        argv = json.loads(self.record.read_text()) if self.record.exists() else None
        return result, argv

    def test_starts_the_bundle_through_open_and_waits(self):
        result, argv = self.launch("--app", str(self.app), "--stdout", "out.log", "--stderr", "err.log",
                                   "--", "--config", "/tmp/cfg dir", "--user-data", "/tmp/data", "--skip-menu")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(argv, [
            "-n", "-W",
            "--stdout", str(self.root / "out.log"),
            "--stderr", str(self.root / "err.log"),
            "-a", str(self.app),
            "--args", "--config", "/tmp/cfg dir", "--user-data", "/tmp/data", "--skip-menu",
        ])

    def test_opens_the_nested_engine_app_by_default(self):
        result, argv = self.launch()
        self.assertEqual(argv[argv.index("-a") + 1],
                         "/Applications/OpenFallout.app/Contents/Helpers/OpenFallout Engine.app")
        self.assertEqual(result.stderr, "")

    def test_passes_environment_variables_and_the_metal_hud(self):
        result, argv = self.launch("--app", str(self.app), "--env", "OSG_NOTIFY_LEVEL=DEBUG", "--metal-hud")
        self.assertEqual(result.returncode, 0, result.stderr)
        envs = [argv[i + 1] for i, word in enumerate(argv) if word == "--env"]
        self.assertEqual(envs, ["OSG_NOTIFY_LEVEL=DEBUG", "MTL_HUD_ENABLED=1"])
        self.assertEqual(argv[-1], "--args")

    def test_background_keeps_the_app_from_coming_to_the_front(self):
        result, argv = self.launch("--app", str(self.app), "--background", "--", "--skip-menu")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(argv[:3], ["-n", "-W", "-g"])
        self.assertEqual(argv[-2:], ["--args", "--skip-menu"])

    def test_logs_default_to_the_current_folder(self):
        _, argv = self.launch("--app", str(self.app))
        self.assertEqual(argv[argv.index("--stdout") + 1], str(self.root / "openfallout.stdout.log"))
        self.assertEqual(argv[argv.index("--stderr") + 1], str(self.root / "openfallout.stderr.log"))

    def test_returns_the_status_of_open(self):
        result, _ = self.launch("--app", str(self.app), status=1)
        self.assertEqual(result.returncode, 1)

    def test_warns_when_the_bundle_does_not_start_the_engine(self):
        contents = self.app / "Contents"
        contents.mkdir(parents=True)
        (contents / "Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable": "openfallout-launcher"}))
        result, _ = self.launch("--app", str(self.app))
        self.assertEqual(result.returncode, 0)
        self.assertIn("openfallout-launcher", result.stderr)

        for engine in ("openfallout", "OpenFallout Engine"):
            (contents / "Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable": engine}))
            result, _ = self.launch("--app", str(self.app))
            self.assertEqual(result.stderr, "", engine)


if __name__ == "__main__":
    unittest.main()
