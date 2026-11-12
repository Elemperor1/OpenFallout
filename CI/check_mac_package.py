#!/usr/bin/env python3
"""Check a packaged OpenFallout for macOS the way a player would get it: a disk image or an app bundle.

    CI/check_mac_package.py PATH [--mode local|release] [--team-id ID] [--smoke run|auto|skip]
                            [--expect-jit on|off|any] [--work DIR] [--notes FILE]

PATH is OpenFallout-*.dmg or OpenFallout.app. The steps run in this order and each prints PASS, FAIL or SKIP:

  dmg          the disk image holds an Applications link and OpenFallout.app, and besides them only OpenFallout-CS.app and
               the README, CHANGELOG, LICENSE and AUTHORS files
  layout       the app holds its launcher, the engine and the nested Contents/Helpers/OpenFallout Engine.app
  signatures   every Mach-O file and every nested bundle has a valid signature, and so has the app as a whole; in
               release mode each of them (and the app) is also signed by a Developer ID Application identity of one
               team (--team-id, else the app's), with the hardened runtime and a secure timestamp, and none carries
               get-task-allow
  gatekeeper   release mode only: `spctl --assess --type open --context context:primary-signature` accepts the disk
               image, `spctl --assess --type execute` accepts the app, and `stapler validate` finds a ticket on both
  info.plist   the app and its engine app declare Game Mode, the role-playing-games category and high resolution, and
               neither carries the Carbon key
  openal       the engine links the OpenAL Soft that is in the bundle, not the system's OpenAL framework
  library-validation  when the engine has the hardened runtime, every Mach-O file in the app has its Team ID (or
               the engine's entitlements turn library validation off); otherwise the loader would refuse the bundled
               libraries and the game would crash at start, so the smoke step is not started
  luajit       from the bundled LuaJIT build (hardened-runtime build with MAP_JIT, or the default mprotect one) and the
               engine's signature and entitlements, the compiler state the signature allows: on, off (the hardened
               runtime refuses MAP_JIT memory without allow-jit, LuaJIT then turns its compiler off), or a crash (an
               mprotect build under the hardened runtime without allow-unsigned-executable-memory is killed at its
               first compiled trace: "Code Signature Invalid"). It fails on a crash, and on "off" when --expect-jit is on;
               the smoke step is then not started. See docs/openfallout/mac-luajit.md.
  smoke        the engine app is started through scripts/openfallout/mac_launch.py on the synthetic Fallout plugin, with
               a hidden window (--headless, SDL_MAC_BACKGROUND_APP=1) and its own --config/--user-data folders; it passes
               when the log shows the cell loaded, a Lua script ran, the LuaJIT compiler state that the engine logs is
               the expected one and the engine quit peacefully. The compiler state seen is always printed.

Local mode accepts an ad hoc signature and skips the gatekeeper step; release mode requires the Developer ID signature,
Gatekeeper's acceptance and the stapled tickets (CI runs it on the notarized build, CI/macos/release_signing.py).
`--notes FILE` appends one release-notes line: the package, the result, the mode, the smoke result and the macOS version
and chip of the machine that ran the check. `--smoke auto` runs the smoke step but reports it as
skipped when the machine gives the engine no OpenGL context (a hosted CI runner may have none); run it with
`--smoke run` on a Mac with a display before a release is tagged.

The exit status is 0 when no step failed.
"""
import argparse
import contextlib
import os
from pathlib import Path
import platform
import plistlib
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
ENGINE_APP = Path("Contents/Helpers/OpenFallout Engine.app")
ENGINE = Path("Contents/MacOS/openfallout")
# what the packaging block puts in the disk image: the apps (the editor only when it is built), a link to
# /Applications and the documents it installs at the top
DMG_REQUIRED = {"Applications", "OpenFallout.app"}
DMG_ALLOWED = DMG_REQUIRED | {"OpenFallout-CS.app", "AUTHORS.md", "CHANGELOG.md", "LICENSE", "README.md"}
CATEGORY = "public.app-category.role-playing-games"
MACHO_MAGICS = {bytes.fromhex(magic) for magic in ("cffaedfe", "cefaedfe", "feedfacf", "feedface", "cafebabe", "bebafeca")}
SYSTEM_OPENAL = "/System/Library/Frameworks/OpenAL.framework"


class StepFailed(Exception):
    pass


def run(command, **kwargs):
    return subprocess.run([str(part) for part in command], capture_output=True, text=True, **kwargs)


# ------------------------------------------------------------------------------------------------------------- layout

@contextlib.contextmanager
def mounted(dmg):
    """Attaches the disk image read-only and out of Finder's sight; yields the mount point."""
    mountpoint = Path(tempfile.mkdtemp(prefix="openfallout-package-"))
    result = run(["hdiutil", "attach", dmg, "-readonly", "-nobrowse", "-noautoopen", "-mountpoint", mountpoint])
    if result.returncode != 0:
        mountpoint.rmdir()
        raise StepFailed(f"hdiutil could not attach {dmg}: {result.stderr.strip()}")
    try:
        yield mountpoint
    finally:
        run(["hdiutil", "detach", mountpoint, "-force"])
        with contextlib.suppress(OSError):
            mountpoint.rmdir()


def check_dmg_layout(volume):
    entries = {entry.name for entry in volume.iterdir() if not entry.name.startswith(".")}
    problems = []
    if DMG_REQUIRED - entries:
        problems.append(f"the disk image lacks {', '.join(sorted(DMG_REQUIRED - entries))}")
    if entries - DMG_ALLOWED:
        problems.append(f"the disk image holds what it should not: {', '.join(sorted(entries - DMG_ALLOWED))} "
                        f"(allowed: {', '.join(sorted(DMG_ALLOWED))})")
    if problems:
        raise StepFailed("; ".join(problems))
    if not (volume / "Applications").is_symlink():
        raise StepFailed("Applications in the disk image is not a link to /Applications")


def read_plist(bundle):
    try:
        with (bundle / "Contents/Info.plist").open("rb") as file:
            return plistlib.load(file)
    except (OSError, plistlib.InvalidFileException) as error:
        raise StepFailed(f"{bundle.name} has no readable Contents/Info.plist ({error})")


def check_app_layout(app):
    problems = []
    for bundle in (app, app / ENGINE_APP):
        if not bundle.is_dir():
            problems.append(f"{relative(bundle, app)} is missing")
            continue
        executable = read_plist(bundle).get("CFBundleExecutable")
        if not executable or not (bundle / "Contents/MacOS" / executable).is_file():
            problems.append(f"{relative(bundle, app)}: CFBundleExecutable {executable!r} is not in Contents/MacOS")
    if not (app / ENGINE).is_file():
        problems.append(f"{ENGINE} is missing")
    if problems:
        raise StepFailed("; ".join(problems))


# --------------------------------------------------------------------------------------------------------- signatures

def relative(path, app):
    return app.name if path == app else str(path.relative_to(app))


def is_macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as file:
        return file.read(4) in MACHO_MAGICS


def nested_bundles(app):
    """Bundles inside the app, innermost first."""
    found = [path for path in app.rglob("*") if path.is_dir() and not path.is_symlink()
             and path.suffix in (".app", ".framework", ".bundle", ".plugin")]
    return sorted(found, key=lambda path: len(path.parts), reverse=True)


def signature_kind(path):
    details = run(["codesign", "--display", "--verbose=2", path]).stderr
    if "Signature=adhoc" in details:
        return "ad hoc"
    authority = re.search(r"^Authority=(.*)$", details, re.MULTILINE)
    return authority.group(1) if authority else "unknown"


def check_signatures(app, mode, team_id=None):
    problems = []
    machos = [path for path in app.rglob("*") if is_macho(path)]
    for item in machos + nested_bundles(app):
        result = run(["codesign", "--verify", "--strict", item])
        if result.returncode != 0:
            lines = result.stderr.strip().replace(f"{item}: ", "").splitlines()
            reason = lines[0] if lines else "codesign failed"
            problems.append(f"{relative(item, app)}: {reason}")
    if not problems:
        result = run(["codesign", "--verify", "--deep", "--strict", app])
        if result.returncode != 0:
            problems.append(f"{app.name}: {result.stderr.strip()}")
    if problems:
        raise StepFailed("not validly signed: " + "; ".join(problems))
    kind = signature_kind(app)
    counted = f"{len(machos)} Mach-O files and {len(nested_bundles(app))} nested bundles verified"
    if mode == "local":
        return f"{counted}, app signature: {kind}"
    team = check_release_signatures(app, machos, team_id)
    return f"{counted}, Developer ID of team {team}, hardened runtime, secure timestamp, no get-task-allow"


def check_release_signatures(app, machos, team_id):
    """Every Mach-O file, nested bundle and the app: a Developer ID Application signature of one team (team_id when
    given, else the app's), the hardened runtime, a secure timestamp and no get-task-allow. Returns the team."""
    expected = team_id or signing_details(app)[1]
    problems = []
    for item in machos + nested_bundles(app) + [app]:
        name = relative(item, app)
        details = run(["codesign", "--display", "--verbose=2", item]).stderr
        if "Signature=adhoc" in details:
            problems.append(f"{name}: ad hoc signature, not Developer ID")
            continue
        authority = re.search(r"^Authority=(.*)$", details, re.MULTILINE)
        if not authority or not authority.group(1).startswith("Developer ID Application:"):
            problems.append(f"{name}: signed by {authority.group(1) if authority else 'no authority'}, not Developer ID")
            continue
        hardened, team = signing_details(item)
        if team != expected:
            problems.append(f"{name}: Team ID {team}, not {expected}")
        if not hardened:
            problems.append(f"{name}: no hardened runtime")
        if not re.search(r"^Timestamp=", details, re.MULTILINE):
            problems.append(f"{name}: no secure timestamp")
        if entitlements(item).get("com.apple.security.get-task-allow") is True:
            problems.append(f"{name}: carries get-task-allow (the debugging entitlement)")
    if problems:
        raise StepFailed("not a release signature: " + "; ".join(problems))
    return expected


# ---------------------------------------------------------------------------------------------------------- gatekeeper

def check_gatekeeper(app, dmg):
    """What Gatekeeper decides for a downloaded copy: the disk image is assessed as a download (its own signature), the
    app as a program to run; both must carry a stapled notarization ticket, so that they open offline."""
    problems, accepted = [], []
    assessments = [(app, ["--type", "execute"])]
    if dmg:
        assessments.insert(0, (dmg, ["--type", "open", "--context", "context:primary-signature"]))
    for item, kind in assessments:
        result = run(["spctl", "--assess", *kind, "-vv", item])
        text = (result.stderr + result.stdout).strip()
        source = re.search(r"^source=(.*)$", text, re.MULTILINE)
        if result.returncode != 0:
            problems.append(f"Gatekeeper rejects {item.name}: {' '.join(text.splitlines()[:2])}")
        else:
            accepted.append(f"{item.name} accepted ({source.group(1) if source else 'no source given'})")
    for item in ([dmg] if dmg else []) + [app]:
        result = run(["xcrun", "stapler", "validate", item])
        if result.returncode != 0:
            text = (result.stderr + result.stdout).strip().splitlines()
            problems.append(f"no valid stapled ticket on {item.name}: {text[-1] if text else 'stapler failed'}")
    if problems:
        raise StepFailed("; ".join(problems))
    return ", ".join(accepted) + "; tickets stapled"


# ------------------------------------------------------------------------------------------------- library-validation

def signing_details(path):
    """(hardened runtime, Team ID or None) of a signed file."""
    details = run(["codesign", "--display", "--verbose=2", path]).stderr
    flags = re.search(r"flags=0x[0-9a-f]+\(([^)]*)\)", details)
    team = re.search(r"^TeamIdentifier=(.*)$", details, re.MULTILINE)
    team = team.group(1).strip() if team else None
    return bool(flags and "runtime" in flags.group(1).split(",")), (None if team in (None, "not set") else team)


def entitlements(path):
    xml = run(["codesign", "--display", "--entitlements", "-", "--xml", path]).stdout
    try:
        return plistlib.loads(xml.encode()) if xml.strip() else {}
    except plistlib.InvalidFileException:
        return {}


def check_library_validation(app):
    """A hardened engine only loads libraries signed by its own team, unless an entitlement turns that off. An ad hoc
    signature has no team, so a hardened ad hoc package dies in the loader at start: say so instead of starting it."""
    engine = app / ENGINE
    if not engine.is_file():
        raise StepFailed(f"{ENGINE} is missing")
    hardened, team = signing_details(engine)
    if not hardened:
        return "no hardened runtime"
    if entitlements(engine).get("com.apple.security.cs.disable-library-validation") is True:
        return "hardened runtime, library validation disabled by entitlement"
    refused = [relative(path, app) for path in app.rglob("*")
               if path != engine and is_macho(path) and (team is None or signing_details(path)[1] != team)]
    if team is None:
        raise StepFailed(f"{ENGINE} runs with the hardened runtime and library validation, but has no Team ID (ad hoc "
                         f"signature), so macOS refuses to load its libraries: {', '.join(refused[:5])}"
                         f"{' and more' if len(refused) > 5 else ''}. Sign with an identity that has a Team ID "
                         f"(`-` only gives an ad hoc signature) or leave the hardened runtime off for local builds")
    if refused:
        raise StepFailed(f"{ENGINE} has Team ID {team} and library validation, but these are signed by another team "
                         f"or none: {', '.join(refused)}")
    return f"hardened runtime, every Mach-O file signed by team {team}"


# ------------------------------------------------------------------------------------------------------------- luajit

ALLOW_JIT = "com.apple.security.cs.allow-jit"
ALLOW_UNSIGNED = "com.apple.security.cs.allow-unsigned-executable-memory"
HRT_SYMBOL = "_pthread_jit_write_protect_np"


def bundled_library(app, binary, pattern, what):
    """The file inside the app that the binary loads for the first linked library matching pattern."""
    libraries = linked_libraries(binary)
    names = [name for name in libraries if re.search(pattern, name)]
    if not names:
        raise StepFailed(f"{relative(binary, app)} links no {what} (otool -L lists {', '.join(libraries)})")
    contents = (app / "Contents").resolve()
    for candidate in resolve(names[0], binary):
        candidate = candidate.resolve()
        if candidate.is_file() and contents in candidate.parents:
            return candidate
    raise StepFailed(f"{relative(binary, app)} links {names[0]}, which is not a file inside the bundle")


def check_luajit(app, expect_jit):
    engine = app / ENGINE
    if not engine.is_file():
        raise StepFailed(f"{ENGINE} is missing")
    library = bundled_library(app, engine, r"libluajit[^/]*\.dylib$", "LuaJIT")
    name = relative(library, app.resolve())
    hrt = HRT_SYMBOL in run(["nm", "-u", library]).stdout.split()
    build = "a hardened-runtime build (MAP_JIT)" if hrt else "a default build (mprotect, no LUAJIT_ENABLE_OSX_HRT)"
    hardened, _ = signing_details(engine)
    granted = entitlements(engine) if hardened else {}
    if not hardened:
        expected, why = "on", "no hardened runtime"
    elif granted.get(ALLOW_UNSIGNED) is True:
        expected, why = "on", "hardened runtime with allow-unsigned-executable-memory"
    elif hrt and granted.get(ALLOW_JIT) is True:
        expected, why = "on", "hardened runtime with allow-jit"
    elif hrt:
        expected, why = "off", "hardened runtime without allow-jit (MAP_JIT memory is refused)"
    else:
        raise StepFailed(f"{name} is {build}; under the hardened runtime without allow-unsigned-executable-memory the "
                         f"engine is killed at its first compiled trace (Code Signature Invalid). Ship a "
                         f"LUAJIT_ENABLE_OSX_HRT build with allow-jit (docs/openfallout/mac-luajit.md)")
    # "on" only says that the signature allows the compiler; the engine may still run without it, which the smoke
    # step sees. "off" is certain.
    if expect_jit == "on" and expected == "off":
        raise StepFailed(f"{name} is {build}; {why}, so the compiler will be {expected}, not {expect_jit} as expected")
    return f"{name} is {build}; {why}, so the compiler is expected {expected}"


# --------------------------------------------------------------------------------------------------------- info.plist

def check_info_plists(app):
    problems = []
    for bundle in (app, app / ENGINE_APP):
        name = relative(bundle, app)
        if not bundle.is_dir():
            problems.append(f"{name} is missing")
            continue
        keys = read_plist(bundle)
        if keys.get("LSSupportsGameMode") is not True:
            problems.append(f"{name}: LSSupportsGameMode is {keys.get('LSSupportsGameMode')!r}, expected true")
        if "LSRequiresCarbon" in keys:
            problems.append(f"{name}: carries LSRequiresCarbon")
        if keys.get("LSApplicationCategoryType") != CATEGORY:
            problems.append(f"{name}: LSApplicationCategoryType is {keys.get('LSApplicationCategoryType')!r}, "
                            f"expected {CATEGORY!r}")
        if keys.get("NSHighResolutionCapable") is not True:
            problems.append(f"{name}: NSHighResolutionCapable is {keys.get('NSHighResolutionCapable')!r}, expected true")
    if problems:
        raise StepFailed("; ".join(problems))


# ------------------------------------------------------------------------------------------------------------- openal

def linked_libraries(binary):
    result = run(["otool", "-L", binary])
    if result.returncode != 0:
        raise StepFailed(f"otool -L {binary.name} failed: {result.stderr.strip()}")
    return [line.strip().split(" (compatibility")[0] for line in result.stdout.splitlines()[1:] if line.strip()]


def rpaths(binary):
    lines = run(["otool", "-l", binary]).stdout.splitlines()
    return [lines[index + 2].split()[1] for index, line in enumerate(lines)
            if line.strip() == "cmd LC_RPATH" and index + 2 < len(lines)]


def resolve(name, binary):
    """The files a load command may stand for (rpaths expanded)."""
    folder = str(binary.parent)
    expand = lambda path: Path(path.replace("@executable_path", folder).replace("@loader_path", folder))
    if name.startswith("@rpath/"):
        return [expand(rpath) / name[len("@rpath/"):] for rpath in rpaths(binary)]
    return [expand(name)]


def check_openal(app):
    engine = app / ENGINE
    if not engine.is_file():
        raise StepFailed(f"{ENGINE} is missing")
    libraries = linked_libraries(engine)
    system = [name for name in libraries if name.startswith(SYSTEM_OPENAL)]
    if system:
        raise StepFailed(f"{ENGINE} links the system OpenAL framework ({system[0]}), not the bundled OpenAL Soft")
    openal = [name for name in libraries if re.search(r"libopenal[^/]*\.dylib$", name)]
    if not openal:
        raise StepFailed(f"{ENGINE} links no libopenal (otool -L lists {', '.join(libraries)})")
    contents = (app / "Contents").resolve()
    for candidate in resolve(openal[0], engine):
        candidate = candidate.resolve()
        if candidate.is_file() and contents in candidate.parents:
            return f"links {openal[0]} -> {relative(candidate, app.resolve())}"
    raise StepFailed(f"{ENGINE} links {openal[0]}, which is not a file inside the bundle")


# -------------------------------------------------------------------------------------------------------------- smoke

LAUNCHER = ROOT / "scripts/openfallout/mac_launch.py"
SMOKE_SCRIPT = """\
local core = require('openfallout.core')
local self = require('openfallout.self')
local elapsed, said = 0, false
return {
    engineHandlers = {
        onUpdate = function(dt)
            if not said then
                print('OFSMOKE', 'started in', self.cell and self.cell.name or '?')
                said = true
            end
            elapsed = elapsed + dt
            if elapsed >= 2 then core.quit() end
        end,
    },
}
"""
# a small hidden window, few frames, no profiler: the Lua profiler's instruction-count hook keeps LuaJIT from compiling
SMOKE_SETTINGS = """\
[Video]
window mode = 2
resolution x = 640
resolution y = 360
vsync mode = 0
framerate limit = 20

[Lua]
lua profiler = false
"""
STARTED = re.compile(r"OFSMOKE\tstarted in\t(.*)$", re.MULTILINE)
JIT_LINE = re.compile(r"LuaJIT compiler: (on|off) \(([^)]*)\); hot loop: (\w+) compiled trace\(s\), (\d+) aborted(.*)$",
                      re.MULTILINE)
QUIT = "Quitting peacefully."
NO_GL = re.compile(r"Failed to create SDL window|Failed to create GraphicsContext|GL context|OpenGL context", re.IGNORECASE)
CI_HINT = ("run `CI/check_mac_package.py <package> --smoke run` on a Mac with a display before the release is tagged")


def prepare_smoke(work):
    sys.path.insert(0, str(ROOT / "scripts/openfallout"))
    import synthetic_fallout_plugin as plugin

    for sub in ("config", "userdata/data", "data/scripts"):
        (work / sub).mkdir(parents=True, exist_ok=True)
    plugin.write(work / "data")
    (work / "data/scripts/packagesmoke.lua").write_text(SMOKE_SCRIPT, encoding="ascii")
    (work / "data/packagesmoke.omwscripts").write_text("PLAYER: scripts/packagesmoke.lua\n", encoding="ascii")
    # replace=config: the player's own configuration is not read, only the app's and this one
    (work / "config/openfallout.cfg").write_text("replace=config\n", encoding="ascii")
    (work / "config/settings.cfg").write_text(SMOKE_SETTINGS, encoding="ascii")
    return plugin.CELL_NAME


def smoke_verdict(text, expect_jit):
    """(problems, the compiler state the log shows or None, whether the engine had no OpenGL context)."""
    jit = JIT_LINE.search(text)
    state = None
    problems = []
    if jit:
        state = f"{jit.group(1)} ({jit.group(2)}); hot loop: {jit.group(3)} compiled trace(s), {jit.group(4)} aborted{jit.group(5)}"
        compiled = jit.group(1) == "on" and jit.group(3).isdigit() and int(jit.group(3)) > 0
        if expect_jit == "on" and not compiled:
            problems.append(f"the LuaJIT compiler should be on and compile hot code; the engine says: {state}")
        if expect_jit == "off" and compiled:
            problems.append(f"the LuaJIT compiler should be off; the engine says: {state}")
    else:
        problems.append("the log has no LuaJIT compiler line (is the engine older than this check?)")
    if not STARTED.search(text):
        problems.append("the game did not start (the Lua player script never ran in the cell)")
    if QUIT not in text:
        problems.append("the engine did not quit peacefully (no 'Quitting peacefully.' in the log)")
    no_gl = not STARTED.search(text) and NO_GL.search(text) is not None
    return problems, state, no_gl


def stop_engine(app):
    run(["pkill", "-f", str(app / ENGINE)])


def check_smoke(app, options):
    work = options.work.absolute() if options.work else Path(tempfile.mkdtemp(prefix="openfallout-package-smoke-"))
    cell = prepare_smoke(work)
    log = work / "config/openfallout.log"
    if log.exists():
        log.unlink()
    command = [sys.executable, LAUNCHER, "--app", app / ENGINE_APP, "--background",
               "--stdout", work / "stdout.log", "--stderr", work / "stderr.log", "--env", "SDL_MAC_BACKGROUND_APP=1",
               "--", "--config", work / "config", "--replace", "config", "--user-data", work / "userdata",
               "--data-local", work / "userdata/data", "--data", work / "data",
               "--content", "OFTest.esm", "--content", "packagesmoke.omwscripts",
               "--skip-menu", "--start", cell, "--headless", "--no-grab"]
    timed_out, helper_error = False, None
    try:
        launched = run(command, timeout=options.seconds)
    except subprocess.TimeoutExpired:
        timed_out = True
        stop_engine(app)
    else:
        # `open -W` also fails when the app died before it could wait on it, so the logs are read either way
        if launched.returncode != 0:
            helper_error = f"the launch helper (open) failed: {launched.stderr.strip()}"
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    problems, state, no_gl = smoke_verdict(text, options.expect_jit)
    print(f"smoke: LuaJIT compiler: {state or 'not reported'} (log: {log})")
    if no_gl and options.smoke == "auto":
        raise SmokeSkipped(f"this machine gave the engine no OpenGL context; {CI_HINT}")
    stderr = (work / "stderr.log").read_text(encoding="utf-8", errors="replace") if (work / "stderr.log").exists() else ""
    # the loader's refusal (dyld, Reason:) and the engine's crash handler (*** Fatal Error ***, <what> (signal N))
    loader = [line.strip() for line in stderr.splitlines()
              if line.startswith("dyld") or "Reason:" in line or "Fatal Error" in line or re.search(r"\(signal \d+\)", line)]
    if loader:
        problems.append("the engine's standard error says: " + " ".join(loader[:2]))
    if helper_error and problems:
        problems.insert(0, helper_error)
    if timed_out:
        problems.insert(0, f"the engine was still running after {options.seconds:g} s and was stopped")
    if problems:
        raise StepFailed("; ".join(problems))
    return f"started in {STARTED.search(text).group(1)}, LuaJIT compiler: {state}, quit peacefully"


class SmokeSkipped(Exception):
    pass


def report_smoke(app, options):
    try:
        return report("smoke", check_smoke, app, options)
    except SmokeSkipped as why:
        return skip("smoke", str(why))


# -------------------------------------------------------------------------------------------------------------- main

RESULTS = {}  # step name -> PASS, FAIL or SKIP, for the notes line


def report(name, step, *args):
    try:
        detail = step(*args)
    except StepFailed as error:
        fail(name, str(error))
        return False
    RESULTS[name] = "PASS"
    print(f"PASS {name}" + (f": {detail}" if detail else ""))
    return True


def fail(name, why):
    RESULTS[name] = "FAIL"
    print(f"FAIL {name}: {why}")


def check_app(app, options, dmg_ok=True, dmg=None):
    results = [dmg_ok]
    layout_ok = report("layout", check_app_layout, app)
    results.append(layout_ok)
    results.append(report("signatures", check_signatures, app, options.mode, options.team_id))
    if options.mode == "release":
        results.append(report("gatekeeper", check_gatekeeper, app, dmg))
    else:
        results.append(skip("gatekeeper", "local mode"))
    results.append(report("info.plist", check_info_plists, app))
    results.append(report("openal", check_openal, app))
    loadable = report("library-validation", check_library_validation, app)
    results.append(loadable)
    jit_ok = report("luajit", check_luajit, app, options.expect_jit)
    results.append(jit_ok)
    if options.smoke == "skip":
        results.append(skip("smoke", "--smoke skip"))
    elif not layout_ok:
        results.append(False)
        fail("smoke", "not started, the layout step failed")
    elif not loadable:
        results.append(False)
        fail("smoke", "not started, the engine could not load its libraries (see library-validation)")
    elif not jit_ok:
        results.append(False)
        fail("smoke", "not started, see the luajit step")
    else:
        results.append(report_smoke(app, options))
    return all(results)


def skip(name, why):
    RESULTS[name] = "SKIP"
    print(f"SKIP {name}: {why}")
    return True


def tested_on():
    """'macOS <version> (<build>), <chip>, <architecture>' of this machine."""
    version = run(["sw_vers", "-productVersion"]).stdout.strip() or "?"
    build = run(["sw_vers", "-buildVersion"]).stdout.strip() or "?"
    chip = run(["sysctl", "-n", "machdep.cpu.brand_string"]).stdout.strip() or "unknown chip"
    return f"macOS {version} ({build}), {chip}, {platform.machine()}"


def write_notes(path, package, mode, ok):
    """Appends the release-notes line for this package: what was checked, with what result, on which Mac."""
    line = (f"- {package.name}: packaged-app check {'PASS' if ok else 'FAIL'} ({mode} mode; smoke "
            f"{RESULTS.get('smoke', 'not run')}) on {tested_on()}")
    with path.open("a", encoding="utf-8") as file:
        file.write(line + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                     epilog="See the top of this file for the steps.")
    parser.add_argument("package", type=Path, help="OpenFallout-*.dmg or OpenFallout.app")
    parser.add_argument("--mode", choices=["local", "release"], default="local",
                        help="local: ad hoc signatures are accepted and Gatekeeper is not asked; release: a Developer "
                             "ID signature (hardened runtime, secure timestamp, one team) on every binary, Gatekeeper "
                             "acceptance and a stapled ticket are required")
    parser.add_argument("--team-id", default=os.environ.get("OPENFALLOUT_CODESIGN_TEAM_ID") or None,
                        help="release mode: the Team ID every signature must carry (default: "
                             "$OPENFALLOUT_CODESIGN_TEAM_ID, else the app's own)")
    parser.add_argument("--smoke", choices=["run", "auto", "skip"], default="run",
                        help="run: start the game and fail if it does not start; auto: as run, but skipped when "
                             "there is no OpenGL context; skip: do not start it")
    parser.add_argument("--expect-jit", choices=["on", "off", "any"], default="on",
                        help="the LuaJIT compiler state the smoke run must see (default: on, hot code compiled)")
    parser.add_argument("--seconds", type=float, default=120, help="how long the smoke run may take")
    parser.add_argument("--notes", type=Path,
                        help="append a release-notes line to this file: the package, the result, and the macOS "
                             "version and chip it was checked on")
    parser.add_argument("--work", type=Path, help="folder for the smoke run's files (kept); default: a temporary one")
    options = parser.parse_args(argv)
    RESULTS.clear()

    package = options.package.absolute()
    if not package.exists():
        print(f"FAIL layout: {package} does not exist")
        return 1
    if package.suffix == ".dmg":
        try:
            with mounted(package) as volume:
                dmg_ok = report("dmg", check_dmg_layout, volume)
                ok = check_app(volume / "OpenFallout.app", options, dmg_ok, package)
        except StepFailed as error:
            fail("dmg", str(error))
            ok = False
    else:
        ok = check_app(package, options)
    print("package check: " + ("PASS" if ok else "FAIL"))
    if options.notes:
        write_notes(options.notes, package, options.mode, ok)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
