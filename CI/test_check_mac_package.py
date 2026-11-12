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


# stand-ins for the two LuaJIT builds: the hardened-runtime one (LUAJIT_ENABLE_OSX_HRT) switches write protection of
# MAP_JIT memory per thread, the default one flips page protection with mprotect
LUAJIT_HRT = ("#include <pthread.h>\nint luaL_newstate(void) { pthread_jit_write_protect_np(1); return 0; }\n")
LUAJIT_MPROTECT = ("#include <sys/mman.h>\nint luaL_newstate(void) { return mprotect(0, 0, PROT_READ); }\n")


def sign(path, *options):
    subprocess.run(["codesign", "--force", "--sign", "-", *options, str(path)], check=True, capture_output=True)


class StandIn:
    """OpenFallout.app as the packaging block lays it out, with tiny programs in place of the real ones."""

    def __init__(self, root, system_openal=False, sign_engine=True, game_keys=None, engine_keys=None, hardened=None,
                 luajit=LUAJIT_HRT):
        """hardened: None signs plain ad hoc; a dict of entitlements signs the engine with the hardened runtime.
        luajit: the source of the bundled LuaJIT stand-in (LUAJIT_HRT or LUAJIT_MPROTECT)."""
        self.app = root / "OpenFallout.app"
        contents = self.app / "Contents"
        frameworks = contents / "Frameworks"
        openal = frameworks / "libopenal.1.25.0.dylib"
        compile_c(openal, "int alGetError(void) { return 0; }\n", "-dynamiclib",
                  "-install_name", "@executable_path/../Frameworks/libopenal.1.25.0.dylib")
        lua = frameworks / "libluajit-5.1.2.1.1767980792.dylib"
        compile_c(lua, luajit, "-dynamiclib", "-mmacosx-version-min=14.0",
                  "-install_name", "@rpath/libluajit-5.1.2.1.1767980792.dylib")
        engine = contents / "MacOS/openfallout"
        uses_lua = ["-Wl,-rpath,@executable_path/../Frameworks", str(lua)]
        if system_openal:
            compile_c(engine, "int luaL_newstate(void);\nint main(void) { return luaL_newstate(); }\n",
                      "-framework", "OpenAL", *uses_lua)
        else:
            compile_c(engine, "int alGetError(void);\nint luaL_newstate(void);\n"
                      "int main(void) { return alGetError() + luaL_newstate(); }\n", str(openal), *uses_lua)
        compile_c(contents / "MacOS/openfallout-launcher", "int main(void) { return 0; }\n")
        nested = contents / "Helpers/OpenFallout Engine.app"
        compile_c(nested / "Contents/MacOS/OpenFallout Engine", "int main(void) { return 0; }\n")
        write_plist(self.app, CFBundleExecutable="openfallout-launcher", CFBundleIdentifier="org.openfallout.openfallout",
                    CFBundlePackageType="APPL", **(GAME_KEYS if game_keys is None else game_keys))
        write_plist(nested, CFBundleExecutable="OpenFallout Engine",
                    CFBundleIdentifier="org.openfallout.openfallout.engine", CFBundlePackageType="APPL",
                    **(GAME_KEYS if engine_keys is None else engine_keys))
        # inside out, as SignMacApplications does
        for item in (openal, lua, contents / "MacOS/openfallout-launcher"):
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
        app = StandIn(self.root, hardened={"com.apple.security.cs.disable-library-validation": True,
                                           "com.apple.security.cs.allow-jit": True}).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 0, output)
        self.assertIn("PASS library-validation: hardened runtime, library validation disabled by entitlement", output)

    def test_plain_ad_hoc_engine_needs_no_library_validation(self):
        app = StandIn(self.root).app
        _, output = run_check(app, "--smoke", "skip")
        self.assertIn("PASS library-validation: no hardened runtime", output)

    def test_plain_ad_hoc_engine_expects_the_compiler_on(self):
        app = StandIn(self.root).app
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 0, output)
        self.assertIn("PASS luajit: Contents/Frameworks/libluajit-5.1.2.1.1767980792.dylib is a hardened-runtime build "
                      "(MAP_JIT); no hardened runtime, so the compiler is expected on", output)

    def hardened(self, luajit, *keys):
        granted = {"com.apple.security.cs.disable-library-validation": True}
        granted.update({f"com.apple.security.cs.{key}": True for key in keys})
        return StandIn(self.root, hardened=granted, luajit=luajit).app

    def test_default_luajit_under_allow_jit_would_crash_and_is_not_started(self):
        app = self.hardened(LUAJIT_MPROTECT, "allow-jit")
        # no real `open` may run if the step lets the smoke through: a stub that fails stands in for it
        stub = self.root / "bin/open"
        stub.parent.mkdir()
        stub.write_text("#!/bin/sh\nexit 1\n")
        stub.chmod(0o755)
        with unittest.mock.patch.dict(os.environ, {"PATH": f"{stub.parent}{os.pathsep}{os.environ['PATH']}"}):
            status, output = run_check(app, "--smoke", "run", "--work", self.root / "work")
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL luajit: Contents/Frameworks/libluajit-5.1.2.1.1767980792.dylib is a default build (mprotect, "
                      "no LUAJIT_ENABLE_OSX_HRT); under the hardened runtime without allow-unsigned-executable-memory "
                      "the engine is killed at its first compiled trace (Code Signature Invalid)", output)
        self.assertIn("FAIL smoke: not started, see the luajit step", output)
        self.assertFalse((self.root / "work").exists())

    def test_hrt_luajit_with_allow_jit_expects_the_compiler_on(self):
        status, output = run_check(self.hardened(LUAJIT_HRT, "allow-jit"), "--smoke", "skip")
        self.assertEqual(status, 0, output)
        self.assertIn("is a hardened-runtime build (MAP_JIT); hardened runtime with allow-jit, so the compiler is "
                      "expected on", output)

    def test_hrt_luajit_without_allow_jit_means_compiler_off(self):
        app = self.hardened(LUAJIT_HRT)
        status, output = run_check(app, "--smoke", "skip")
        self.assertEqual(status, 1, output)
        self.assertIn("hardened runtime without allow-jit (MAP_JIT memory is refused), so the compiler will be off, "
                      "not on as expected", output)
        status, output = run_check(app, "--smoke", "skip", "--expect-jit", "off")
        self.assertEqual(status, 0, output)

    def test_default_luajit_with_unsigned_executable_memory_expects_on(self):
        status, output = run_check(self.hardened(LUAJIT_MPROTECT, "allow-unsigned-executable-memory"), "--smoke", "skip")
        self.assertEqual(status, 0, output)
        self.assertIn("is a default build (mprotect, no LUAJIT_ENABLE_OSX_HRT); hardened runtime with "
                      "allow-unsigned-executable-memory, so the compiler is expected on", output)

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
SOFTWARE_GL = "[12:00:00.300 I] OpenGL Renderer: Apple Software Renderer\n"


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

    def test_engine_crash_signal_is_quoted(self):
        crash = ("\n\n*** Fatal Error ***\nInvalid address alignment (signal 10)\nAddress: 0x900000a00\n\n"
                 "Generating /tmp/openfallout-crash.log and killing process 1, please wait...\n")
        status, output, _ = self.smoke(JIT_ON + STARTED, stderr=crash)
        self.assertEqual(status, 1, output)
        self.assertIn("the engine's standard error says: *** Fatal Error *** Invalid address alignment (signal 10)",
                      output)

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

    def test_software_renderer_run_that_does_not_finish_is_skipped_in_auto_mode_only(self):
        crash = "\n\n*** Fatal Error ***\nInvalid address alignment (signal 10)\nAddress: 0x900000a00\n"
        log = SOFTWARE_GL + JIT_ON + STARTED
        status, output, _ = self.smoke(log, "--smoke", "auto", stderr=crash)
        self.assertEqual(status, 0, output)
        self.assertIn("SKIP smoke: this machine has only Apple's software renderer", output)
        self.assertIn("Invalid address alignment (signal 10)", output)
        status, output, _ = self.smoke(log, "--smoke", "run", stderr=crash)
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL smoke", output)

    def test_wrong_compiler_state_fails_in_auto_mode_even_on_the_software_renderer(self):
        status, output, _ = self.smoke(SOFTWARE_GL + JIT_NOTHING_COMPILED + STARTED, "--smoke", "auto")
        self.assertEqual(status, 1, output)
        self.assertIn("the LuaJIT compiler should be on", output)

    def test_software_renderer_run_that_finishes_passes(self):
        status, output, _ = self.smoke(SOFTWARE_GL + JIT_ON + STARTED + QUIT, "--smoke", "auto")
        self.assertEqual(status, 0, output)
        self.assertIn("PASS smoke", output)


# ------------------------------------------------------------------------------------------------------- release mode

TEAM = "EXAMPLE123"
DEVELOPER_ID = f"Developer ID Application: Example Studio ({TEAM})"
FAKE_MACHO = bytes.fromhex("cffaedfe") + bytes(28)


class FakeTools:
    """Answers the codesign, spctl and stapler calls of the check as a notarized, stapled Developer ID package would;
    a test changes `signers` (path suffix -> signature details) or `fails` to make one item or tool misbehave."""

    def __init__(self):
        self.calls = []
        self.overrides = {}     # path suffix -> dict of details to change for that item
        self.fails = {}         # (tool, verb) -> (exit status, stderr)
        self.entitlements = {}  # path suffix -> entitlements dict

    def details(self, path):
        info = {"authority": DEVELOPER_ID, "team": TEAM, "flags": "0x10000(runtime)", "timestamp": True}
        for suffix, change in self.overrides.items():
            if path.endswith(suffix):
                info.update(change)
        lines = [f"Executable={path}", "Identifier=org.openfallout.item", f"CodeDirectory v=20500 size=1 flags={info['flags']} hashes=1"]
        if info["authority"] == "adhoc":
            lines += ["Signature=adhoc", "TeamIdentifier=not set"]
        else:
            lines += [f"Authority={info['authority']}", "Authority=Developer ID Certification Authority",
                      "Authority=Apple Root CA"]
            lines += ["Timestamp=12 Nov 2026 at 10:00:00"] if info["timestamp"] else ["Signed Time=12 Nov 2026 at 10:00:00"]
            lines += [f"TeamIdentifier={info['team']}"]
        return "\n".join(lines) + "\n"

    def __call__(self, command, **kwargs):
        command = [str(part) for part in command]
        self.calls.append(command)
        tool = command[0] if command[0] != "xcrun" else command[1]
        verb = command[2] if command[0] == "xcrun" else (command[1] if len(command) > 1 else "")
        out, err, status = "", "", 0
        if (tool, verb) in self.fails:
            status, err = self.fails[(tool, verb)]
        elif tool == "codesign" and "--entitlements" in command:
            granted = next((value for suffix, value in self.entitlements.items() if command[-1].endswith(suffix)), {})
            out = plistlib.dumps(granted).decode() if granted else ""
        elif tool == "codesign" and "--display" in command:
            err = self.details(command[-1])
        elif tool == "spctl":
            err = f"{command[-1]}: accepted\nsource=Notarized Developer ID\norigin={DEVELOPER_ID}\n"
        elif tool == "stapler":
            out = f"Processing: {command[-1]}\nThe validate action worked!\n"
        elif tool == "sw_vers":
            out = {"-productVersion": "26.1\n", "-buildVersion": "25B78\n"}[command[1]]
        elif tool == "sysctl":
            out = "Apple M2 (Virtual)\n"
        return subprocess.CompletedProcess(command, status, out, err)


def fake_package(root):
    app = root / "OpenFallout.app"
    for item in ("Contents/MacOS/openfallout", "Contents/MacOS/openfallout-launcher",
                 "Contents/Frameworks/libopenal.1.25.0.dylib",
                 "Contents/Helpers/OpenFallout Engine.app/Contents/MacOS/OpenFallout Engine"):
        (app / item).parent.mkdir(parents=True, exist_ok=True)
        (app / item).write_bytes(FAKE_MACHO)
    write_plist(app, CFBundleExecutable="openfallout-launcher", **GAME_KEYS)
    write_plist(app / "Contents/Helpers/OpenFallout Engine.app", CFBundleExecutable="OpenFallout Engine", **GAME_KEYS)
    return app


class ReleaseModeTests(unittest.TestCase):
    """Release mode against a stand-in tree and stand-in tools: runs on any machine."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="release check ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.tools = FakeTools()
        patcher = unittest.mock.patch.object(check_mac_package, "run", self.tools)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.app = fake_package(self.root)

    def check(self, *args, package=None):
        return run_check(package or self.app, "--mode", "release", "--smoke", "skip", *args)

    def test_notarized_developer_id_package_passes_signatures_and_gatekeeper(self):
        status, output = self.check("--team-id", TEAM)
        self.assertIn(f"PASS signatures: 4 Mach-O files and 1 nested bundles verified, Developer ID of team {TEAM}, "
                      "hardened runtime, secure timestamp, no get-task-allow", output)
        self.assertIn("PASS gatekeeper", output)
        assessed = [call for call in self.tools.calls if call[0] == "spctl"]
        self.assertEqual(assessed, [["spctl", "--assess", "--type", "execute", "-vv", str(self.app)]])
        stapled = [call for call in self.tools.calls if call[:3] == ["xcrun", "stapler", "validate"]]
        self.assertEqual(stapled, [["xcrun", "stapler", "validate", str(self.app)]])

    def test_ad_hoc_item_fails_release_mode_and_is_named(self):
        self.tools.overrides["libopenal.1.25.0.dylib"] = {"authority": "adhoc"}
        status, output = self.check()
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL signatures", output)
        self.assertIn("Contents/Frameworks/libopenal.1.25.0.dylib: ad hoc signature, not Developer ID", output)

    def test_item_of_another_team_fails(self):
        self.tools.overrides["openfallout-launcher"] = {"team": "OTHERTEAM1",
                                                        "authority": "Developer ID Application: Other (OTHERTEAM1)"}
        status, output = self.check()
        self.assertEqual(status, 1, output)
        self.assertIn(f"Contents/MacOS/openfallout-launcher: Team ID OTHERTEAM1, not {TEAM}", output)

    def test_team_id_option_must_match_the_app(self):
        status, output = self.check("--team-id", "WRONGTEAM1")
        self.assertEqual(status, 1, output)
        self.assertIn(f"OpenFallout.app: Team ID {TEAM}, not WRONGTEAM1", output)

    def test_missing_hardened_runtime_timestamp_or_debug_entitlement_fail(self):
        self.tools.overrides["MacOS/openfallout"] = {"flags": "0x0(none)"}
        self.tools.overrides["OpenFallout Engine.app"] = {"timestamp": False}
        self.tools.entitlements["MacOS/openfallout-launcher"] = {"com.apple.security.get-task-allow": True}
        status, output = self.check()
        self.assertEqual(status, 1, output)
        self.assertIn("Contents/MacOS/openfallout: no hardened runtime", output)
        self.assertIn("Contents/Helpers/OpenFallout Engine.app: no secure timestamp", output)
        self.assertIn("Contents/MacOS/openfallout-launcher: carries get-task-allow", output)

    def test_gatekeeper_rejection_or_missing_ticket_fails(self):
        self.tools.fails[("spctl", "--assess")] = (3, f"{self.app}: rejected\nsource=Unnotarized Developer ID\n")
        self.tools.fails[("stapler", "validate")] = (65, f"{self.app} does not have a ticket stapled to it.\n")
        status, output = self.check()
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL gatekeeper", output)
        self.assertIn("rejected", output)
        self.assertIn("does not have a ticket stapled to it", output)

    def test_disk_image_is_assessed_as_a_download_and_its_ticket_validated(self):
        volume = self.root / "volume"
        volume.mkdir()
        shutil.move(str(self.app), volume / "OpenFallout.app")
        os.symlink("/Applications", volume / "Applications")
        dmg = self.root / "OpenFallout-1.0-macOS-arm64.dmg"
        dmg.write_bytes(b"image")

        @contextlib.contextmanager
        def mounted(path):
            yield volume

        with unittest.mock.patch.object(check_mac_package, "mounted", mounted):
            status, output = self.check(package=dmg)
        self.assertIn("PASS gatekeeper", output)
        self.assertIn(["spctl", "--assess", "--type", "open", "--context", "context:primary-signature", "-vv", str(dmg)],
                      self.tools.calls)
        self.assertIn(["xcrun", "stapler", "validate", str(dmg)], self.tools.calls)
        self.assertIn(["xcrun", "stapler", "validate", str(volume / "OpenFallout.app")], self.tools.calls)

    def test_notes_record_the_macos_version_and_chip_the_package_was_checked_on(self):
        notes = self.root / "notes.md"
        notes.write_text("earlier line\n")
        status, output = self.check("--notes", notes)
        self.assertEqual(notes.read_text().splitlines()[0], "earlier line")
        line = notes.read_text().splitlines()[1]
        self.assertIn("OpenFallout.app", line)
        self.assertIn("macOS 26.1 (25B78)", line)
        self.assertIn("Apple M2 (Virtual)", line)
        self.assertIn("release mode", line)
        self.assertIn("FAIL", line)  # the stand-in has no real libraries, so later steps fail; the line says so
        self.assertIn("smoke SKIP", line)

    def test_local_mode_skips_gatekeeper(self):
        self.tools.overrides[""] = {"authority": "adhoc"}
        status, output = run_check(self.app, "--mode", "local", "--smoke", "skip")
        self.assertIn("SKIP gatekeeper: local mode", output)
        self.assertNotIn("spctl", [call[0] for call in self.tools.calls])


if __name__ == "__main__":
    unittest.main()
