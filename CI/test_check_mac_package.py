"""The packaged-app check against small stand-in bundles: real Mach-O files, real codesign, no game and no engine."""

import contextlib
import io
import json
import os
import plistlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_mac_package  # noqa: E402

ON_MAC = sys.platform == "darwin" and shutil.which("clang") and shutil.which("codesign")

GAME_KEYS = {
    "LSApplicationCategoryType": "public.app-category.role-playing-games",
    "LSSupportsGameMode": True,
    "NSHighResolutionCapable": True,
}


def compile_c(output, source, *flags):
    output.parent.mkdir(parents=True, exist_ok=True)
    c_file = output.parent / (output.name.replace(" ", "_") + ".c")
    c_file.write_text(source)
    subprocess.run(["clang", "-o", str(output), str(c_file), *flags], check=True)
    c_file.unlink()


def write_plist(app, **keys):
    (app / "Contents").mkdir(parents=True, exist_ok=True)
    with (app / "Contents/Info.plist").open("wb") as file:
        plistlib.dump(keys, file)


def sign(path, *options):
    subprocess.run(["codesign", "--force", "--sign", "-", *options, str(path)], check=True, capture_output=True)


class StandIn:
    """OpenFallout.app as the packaging block lays it out, with tiny programs in place of the real ones."""

    def __init__(self, root, system_openal=False, sign_engine=True, game_keys=None, engine_keys=None, hardened=None):
        """hardened: None signs plain ad hoc; a dict of entitlements signs the engine with the hardened runtime."""
        self.app = root / "OpenFallout.app"
        contents = self.app / "Contents"
        frameworks = contents / "Frameworks"
        openal = frameworks / "libopenal.1.25.0.dylib"
        compile_c(openal, "int alGetError(void) { return 0; }\n", "-dynamiclib",
                  "-install_name", "@executable_path/../Frameworks/libopenal.1.25.0.dylib")
        engine = contents / "MacOS/openfallout"
        if system_openal:
            compile_c(engine, "int main(void) { return 0; }\n", "-framework", "OpenAL")
        else:
            compile_c(engine, "int alGetError(void);\nint main(void) { return alGetError(); }\n", str(openal))
        compile_c(contents / "MacOS/openfallout-launcher", "int main(void) { return 0; }\n")
        nested = contents / "Helpers/OpenFallout Engine.app"
        compile_c(nested / "Contents/MacOS/OpenFallout Engine", "int main(void) { return 0; }\n")
        write_plist(self.app, CFBundleExecutable="openfallout-launcher", CFBundleIdentifier="org.openfallout.openfallout",
                    CFBundlePackageType="APPL", **(GAME_KEYS if game_keys is None else game_keys))
        write_plist(nested, CFBundleExecutable="OpenFallout Engine",
                    CFBundleIdentifier="org.openfallout.openfallout.engine", CFBundlePackageType="APPL",
                    **(GAME_KEYS if engine_keys is None else engine_keys))
        # inside out, as SignMacApplications does
        for item in (openal, contents / "MacOS/openfallout-launcher"):
            sign(item)
        if hardened is None:
            sign(engine)
        else:
            entitlements = root / "engine.entitlements"
            entitlements.write_bytes(plistlib.dumps(hardened))
            sign(engine, "--options", "runtime", "--entitlements", str(entitlements))
        sign(nested / "Contents/MacOS/OpenFallout Engine")
        sign(nested)
        sign(self.app)
        if not sign_engine:
            # the linker signs arm64 programs by itself, so take the signature off as a packaging step that missed it would
            subprocess.run(["codesign", "--remove-signature", str(nested / "Contents/MacOS/OpenFallout Engine")],
                           check=True)


def run_check(*args):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        status = check_mac_package.main([str(arg) for arg in args])
    return status, out.getvalue() + err.getvalue()


@unittest.skipUnless(ON_MAC, "needs macOS, clang and codesign")
class PackageCheckTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="package check ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def test_good_ad_hoc_bundle_passes_in_local_mode(self):
        app = StandIn(self.root).app
        status, output = run_check(app, "--mode", "local", "--smoke", "skip")
        self.assertEqual(status, 0, output)
        for step in ("layout", "signatures", "info.plist", "openal"):
            self.assertIn(f"PASS {step}", output)
        self.assertIn("SKIP smoke", output)

    def test_unsigned_nested_binary_fails_and_names_it(self):
        app = StandIn(self.root, sign_engine=False).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL signatures", output)
        self.assertIn("Contents/Helpers/OpenFallout Engine.app/Contents/MacOS/OpenFallout Engine", output)
        self.assertIn("not signed at all", output)

    def test_missing_game_mode_key_fails(self):
        keys = dict(GAME_KEYS)
        del keys["LSSupportsGameMode"]
        app = StandIn(self.root, engine_keys=keys).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL info.plist: Contents/Helpers/OpenFallout Engine.app: LSSupportsGameMode is None, "
                      "expected true", output)

    def test_carbon_key_and_wrong_category_fail(self):
        app = StandIn(self.root, game_keys=dict(GAME_KEYS, LSRequiresCarbon=True,
                                                LSApplicationCategoryType="public.app-category.games")).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("OpenFallout.app: carries LSRequiresCarbon", output)
        self.assertIn("LSApplicationCategoryType is 'public.app-category.games'", output)

    def test_system_openal_link_fails(self):
        app = StandIn(self.root, system_openal=True).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL openal: Contents/MacOS/openfallout links the system OpenAL framework", output)

    def test_bundled_openal_is_named(self):
        app = StandIn(self.root).app
        _, output = run_check(app, "--smoke", "skip")
        self.assertIn("PASS openal: links @executable_path/../Frameworks/libopenal.1.25.0.dylib -> "
                      "Contents/Frameworks/libopenal.1.25.0.dylib", output)

    def test_hardened_ad_hoc_engine_that_cannot_load_its_libraries_fails_before_launch(self):
        app = StandIn(self.root, hardened={}).app
        status, output = run_check(app, "--smoke", "run")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL library-validation: Contents/MacOS/openfallout runs with the hardened runtime and library "
                      "validation, but has no Team ID", output)
        self.assertIn("Contents/Frameworks/libopenal.1.25.0.dylib", output)
        self.assertIn("FAIL smoke: not started", output)

    def test_hardened_engine_with_library_validation_disabled_passes_the_static_step(self):
        app = StandIn(self.root, hardened={"com.apple.security.cs.disable-library-validation": True}).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 0, output)
        self.assertIn("PASS library-validation: hardened runtime, library validation disabled by entitlement", output)

    def test_plain_ad_hoc_engine_needs_no_library_validation(self):
        app = StandIn(self.root).app
        _, output = run_check(app, "--smoke", "skip")
        self.assertIn("PASS library-validation: no hardened runtime", output)

    def test_missing_engine_app_fails_layout(self):
        app = StandIn(self.root).app
        shutil.rmtree(app / "Contents/Helpers")
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL layout: Contents/Helpers/OpenFallout Engine.app is missing", output)

    def test_disk_image_with_extra_entry_fails(self):
        stage = self.root / "stage"
        stage.mkdir()
        StandIn(stage)
        (stage / "OpenFallout-CS.app").mkdir()
        os.symlink("/Applications", stage / "Applications")
        (stage / "README.txt").write_text("extra")
        dmg = self.root / "OpenFallout-test.dmg"
        subprocess.run(["hdiutil", "create", "-quiet", "-fs", "HFS+", "-srcfolder", str(stage), "-volname", "OFTest",
                        str(dmg)], check=True)
        status, output = run_check(dmg, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL dmg: the disk image holds what it should not: README.txt", output)
        self.assertIn("PASS signatures", output)

        (stage / "README.txt").unlink()
        (stage / "README.md").write_text("read me")
        dmg.unlink()
        subprocess.run(["hdiutil", "create", "-quiet", "-fs", "HFS+", "-srcfolder", str(stage), "-volname", "OFTest",
                        str(dmg)], check=True)
        status, output = run_check(dmg, "--smoke", "skip")
        self.assertEqual(status, 0, output)
        self.assertIn("PASS dmg", output)


# Stands in for macOS's `open`: records its arguments and writes the engine log that $SMOKE_LOG holds into the
# --config folder of the engine arguments, as the engine would; exits with $OPEN_STATUS.
OPEN_STUB = """#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
argv = sys.argv[1:]
Path(os.environ["OPEN_RECORD"]).write_text(json.dumps(argv))
engine = argv[argv.index("--args") + 1:]
config = Path(engine[engine.index("--config") + 1])
(config / "openfallout.log").write_text(os.environ["SMOKE_LOG"])
Path(argv[argv.index("--stderr") + 1]).write_text(os.environ.get("SMOKE_STDERR", ""))
sys.exit(int(os.environ.get("OPEN_STATUS", "0")))
"""

STARTED = "[12:00:01.000 I] OFSMOKE\tstarted in\tOFTestCell\n"
JIT_ON = "[12:00:00.500 I] LuaJIT compiler: on (fold cse dce fwd dse narrow loop abc sink fuse); hot loop: 2 compiled trace(s), 0 aborted\n"
JIT_NOTHING_COMPILED = "[12:00:00.500 I] LuaJIT compiler: on (fold cse); hot loop: 0 compiled trace(s), 3 aborted, first abort error 27\n"
QUIT = "[12:00:05.000 I] Quitting peacefully.\n"
NO_GL = "[12:00:00.400 E] Error in framelistener: Failed to create SDL window: Could not create GL context\n"


@unittest.skipUnless(ON_MAC, "needs macOS, clang and codesign")
class SmokeRunTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="package smoke ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        bin_dir = self.root / "bin"
        bin_dir.mkdir()
        (bin_dir / "open").write_text(OPEN_STUB)
        (bin_dir / "open").chmod(0o755)
        self.record = self.root / "open.json"
        self.app = StandIn(self.root).app
        self.environ = {"PATH": f"{bin_dir}{os.pathsep}{os.environ['PATH']}", "OPEN_RECORD": str(self.record)}

    def smoke(self, log, *args, stderr=""):
        with unittest.mock.patch.dict(os.environ, dict(self.environ, SMOKE_LOG=log, SMOKE_STDERR=stderr)):
            status, output = run_check(self.app, "--work", self.root / "work", *args)
        return status, output, json.loads(self.record.read_text())

    def test_started_compiled_and_quit_passes_and_reports_the_compiler(self):
        status, output, _ = self.smoke(STARTED + JIT_ON + QUIT)
        self.assertEqual(status, 0, output)
        self.assertIn("PASS smoke", output)
        self.assertIn("LuaJIT compiler: on (fold cse dce fwd dse narrow loop abc sink fuse); hot loop: 2 compiled "
                      "trace(s), 0 aborted", output)

    def test_starts_the_engine_app_hidden_with_its_own_folders(self):
        _, _, argv = self.smoke(STARTED + JIT_ON + QUIT)
        self.assertIn("-g", argv[:argv.index("--args")])
        self.assertEqual(argv[argv.index("-a") + 1], str(self.app / "Contents/Helpers/OpenFallout Engine.app"))
        self.assertIn("SDL_MAC_BACKGROUND_APP=1", argv)
        engine = argv[argv.index("--args") + 1:]
        for flag in ("--headless", "--skip-menu", "--no-grab"):
            self.assertIn(flag, engine)
        self.assertEqual(engine[engine.index("--start") + 1], "OFTestCell")
        self.assertEqual(engine[engine.index("--config") + 1], str(self.root / "work/config"))
        self.assertEqual(engine[engine.index("--user-data") + 1], str(self.root / "work/userdata"))
        self.assertTrue((self.root / "work/data/OFTest.esm").is_file())
        settings = (self.root / "work/config/settings.cfg").read_text()
        self.assertIn("[Lua]\nlua profiler = false", settings)

    def test_compiler_that_compiled_nothing_fails_when_on_is_expected(self):
        status, output, _ = self.smoke(STARTED + JIT_NOTHING_COMPILED + QUIT)
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL smoke: the LuaJIT compiler should be on and compile hot code; the engine says: on (fold cse); "
                      "hot loop: 0 compiled trace(s), 3 aborted, first abort error 27", output)
        status, output, _ = self.smoke(STARTED + JIT_NOTHING_COMPILED + QUIT, "--expect-jit", "off")
        self.assertEqual(status, 0, output)

    def test_engine_that_did_not_quit_peacefully_fails(self):
        status, output, _ = self.smoke(STARTED + JIT_ON)
        self.assertEqual(status, 1, output)
        self.assertIn("the engine did not quit peacefully", output)

    def test_game_that_never_started_fails(self):
        status, output, _ = self.smoke(JIT_ON + QUIT)
        self.assertEqual(status, 1, output)
        self.assertIn("the game did not start", output)

    def test_loader_refusal_is_quoted(self):
        refusal = ("dyld[1]: Library not loaded: @executable_path/../Frameworks/libopenal.1.25.0.dylib\n"
                   "  Reason: tried: '...' (code signature not valid for use in process: mapping process and mapped "
                   "file (non-platform) have different Team IDs)\n")
        status, output, _ = self.smoke("", stderr=refusal)
        self.assertEqual(status, 1, output)
        self.assertIn("the engine's standard error says: dyld[1]: Library not loaded: "
                      "@executable_path/../Frameworks/libopenal.1.25.0.dylib", output)
        self.assertIn("have different Team IDs", output)

    def test_missing_jit_line_fails(self):
        status, output, _ = self.smoke(STARTED + QUIT)
        self.assertEqual(status, 1, output)
        self.assertIn("the log has no LuaJIT compiler line", output)

    def test_no_opengl_context_is_skipped_in_auto_mode_and_fails_in_run_mode(self):
        status, output, _ = self.smoke(NO_GL, "--smoke", "auto")
        self.assertEqual(status, 0, output)
        self.assertIn("SKIP smoke: this machine gave the engine no OpenGL context", output)
        status, output, _ = self.smoke(NO_GL, "--smoke", "run")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL smoke", output)


if __name__ == "__main__":
    unittest.main()
