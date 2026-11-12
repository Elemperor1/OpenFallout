#!/usr/bin/env python3
"""Check a packaged OpenFallout for macOS the way a player would get it: a disk image or an app bundle.

    CI/check_mac_package.py PATH [--mode local] [--smoke run|auto|skip] [--expect-jit on|off|any] [--work DIR]

PATH is OpenFallout-*.dmg or OpenFallout.app. The steps run in this order and each prints PASS, FAIL or SKIP:

  dmg          the disk image holds an Applications link and OpenFallout.app, and besides them only OpenFallout-CS.app and
               the README, CHANGELOG, LICENSE and AUTHORS files
  layout       the app holds its launcher, the engine and the nested Contents/Helpers/OpenFallout Engine.app
  signatures   every Mach-O file and every nested bundle has a valid signature, and so has the app as a whole
  info.plist   the app and its engine app declare Game Mode, the role-playing-games category and high resolution, and
               neither carries the Carbon key
  openal       the engine links the OpenAL Soft that is in the bundle, not the system's OpenAL framework
  library-validation  when the engine has the hardened runtime, every Mach-O file in the app has its Team ID (or
               the engine's entitlements turn library validation off); otherwise the loader would refuse the bundled
               libraries and the game would crash at start, so the smoke step is not started
  smoke        the engine app is started through scripts/openfallout/mac_launch.py on the synthetic Fallout plugin, with
               a hidden window (--headless, SDL_MAC_BACKGROUND_APP=1) and its own --config/--user-data folders; it passes
               when the log shows the cell loaded, a Lua script ran, the LuaJIT compiler state that the engine logs is
               the expected one and the engine quit peacefully. The compiler state seen is always printed.

Local mode accepts an ad hoc signature and has no Gatekeeper steps; release mode (a Developer ID signature, Gatekeeper
assessment and a stapled ticket) arrives with notarization. `--smoke auto` runs the smoke step but reports it as
skipped when the machine gives the engine no OpenGL context (a hosted CI runner may have none); run it with
`--smoke run` on a Mac with a display before a release is tagged.

The exit status is 0 when no step failed.
"""
import argparse
import contextlib
from pathlib import Path
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


def check_signatures(app, mode):
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
    if mode == "local":
        return f"{len(machos)} Mach-O files and {len(nested_bundles(app))} nested bundles verified, app signature: {kind}"
    raise StepFailed(f"mode {mode} is not supported")


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
    loader = [line.strip() for line in stderr.splitlines() if line.startswith("dyld") or "Reason:" in line]
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

def report(name, step, *args):
    try:
        detail = step(*args)
    except StepFailed as error:
        print(f"FAIL {name}: {error}")
        return False
    print(f"PASS {name}" + (f": {detail}" if detail else ""))
    return True


def check_app(app, options, dmg_ok=True):
    results = [dmg_ok]
    layout_ok = report("layout", check_app_layout, app)
    results.append(layout_ok)
    results.append(report("signatures", check_signatures, app, options.mode))
    results.append(report("info.plist", check_info_plists, app))
    results.append(report("openal", check_openal, app))
    loadable = report("library-validation", check_library_validation, app)
    results.append(loadable)
    if options.smoke == "skip":
        results.append(skip("smoke", "--smoke skip"))
    elif not layout_ok:
        results.append(False)
        print("FAIL smoke: not started, the layout step failed")
    elif not loadable:
        results.append(False)
        print("FAIL smoke: not started, the engine could not load its libraries (see library-validation)")
    else:
        results.append(report_smoke(app, options))
    return all(results)


def skip(name, why):
    print(f"SKIP {name}: {why}")
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1].strip(),
                                     epilog="See the top of this file for the steps.")
    parser.add_argument("package", type=Path, help="OpenFallout-*.dmg or OpenFallout.app")
    parser.add_argument("--mode", choices=["local"], default="local",
                        help="local: ad hoc signatures are accepted and Gatekeeper is not asked (release mode comes "
                             "with notarization)")
    parser.add_argument("--smoke", choices=["run", "auto", "skip"], default="run",
                        help="run: start the game and fail if it does not start; auto: as run, but skipped when "
                             "there is no OpenGL context; skip: do not start it")
    parser.add_argument("--expect-jit", choices=["on", "off", "any"], default="on",
                        help="the LuaJIT compiler state the smoke run must see (default: on, hot code compiled)")
    parser.add_argument("--seconds", type=float, default=120, help="how long the smoke run may take")
    parser.add_argument("--work", type=Path, help="folder for the smoke run's files (kept); default: a temporary one")
    options = parser.parse_args(argv)

    package = options.package.absolute()
    if not package.exists():
        print(f"FAIL layout: {package} does not exist")
        return 1
    if package.suffix == ".dmg":
        try:
            with mounted(package) as volume:
                dmg_ok = report("dmg", check_dmg_layout, volume)
                ok = check_app(volume / "OpenFallout.app", options, dmg_ok)
        except StepFailed as error:
            print(f"FAIL dmg: {error}")
            return 1
    else:
        ok = check_app(package, options)
    print("package check: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
