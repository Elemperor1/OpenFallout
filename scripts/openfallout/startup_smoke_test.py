#!/usr/bin/env python3
"""Start the engine in the main menu on a tiny synthetic game file and fail if a built-in Lua script or l10n file breaks.

The unit tests do not load the scripts under files/data, so a renamed module id or l10n context that is wrong only
shows up when the engine runs. This runs it for a few seconds under a virtual display (Xvfb, Mesa software rendering
are enough), then reads the log. It needs no game data: the game file it writes holds only the records the world
refuses to start without (skills, globals, game settings, one race, one class and the player).

    scripts/openfallout/startup_smoke_test.py --build build

The engine's configuration and logs go to a temporary home directory. The exit status is 0 when the log has no
"Can't start", "Lua error" or "Fatal error" line and the Lua scripts and l10n files were loaded.
"""
import argparse
import os
import pty
import re
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Strings that look like game setting names (fFoo, iFoo, sFoo) anywhere in the sources. The engine reads many of
# them when it starts, and a missing one is a fatal error, so the synthetic file defines every candidate.
GMST_NAME = re.compile(r'"([fis][A-Z][A-Za-z0-9]+)"')


def sub(name, data):
    return name + struct.pack("<I", len(data)) + data


def rec(name, subs):
    body = b"".join(subs)
    return name + struct.pack("<III", len(body), 0, 0) + body


def gmst_names():
    names = set()
    for top in ("apps", "components"):
        for path in (ROOT / top).rglob("*"):
            if path.suffix in (".cpp", ".hpp"):
                names.update(GMST_NAME.findall(path.read_text(encoding="utf-8", errors="replace")))
    return sorted(names)


def game_file():
    records = []
    for index in range(27):  # one SKIL record per skill, the engine reads all of them
        records.append(rec(b"SKIL", [sub(b"INDX", struct.pack("<i", index)),
                                     sub(b"SKDT", struct.pack("<2i4f", 0, 0, 1.0, 1.0, 1.0, 1.0)),
                                     sub(b"DESC", b"Skill %d\0" % index)]))
    for name, kind, value in (("gamehour", b"f", 9.0), ("dayspassed", b"s", 1), ("day", b"s", 1),
                              ("month", b"s", 0), ("year", b"s", 427), ("timescale", b"f", 30.0)):
        records.append(rec(b"GLOB", [sub(b"NAME", name.encode() + b"\0"), sub(b"FNAM", kind),
                                     sub(b"FLTV", struct.pack("<f", value))]))
    records.append(rec(b"RACE", [sub(b"NAME", b"smoke_race\0"), sub(b"FNAM", b"Smoke\0"),
                                 sub(b"RADT", struct.pack("<14i16i4f1i", *([0] * 14), *([50] * 16),
                                                          1.0, 1.0, 1.0, 1.0, 1))]))
    records.append(rec(b"CLAS", [sub(b"NAME", b"smoke_class\0"), sub(b"FNAM", b"Smoke\0"),
                                 sub(b"CLDT", struct.pack("<2i1i10i2i", 0, 1, 0, *([0] * 10), 1, 0))]))
    records.append(rec(b"NPC_", [sub(b"NAME", b"player\0"), sub(b"FNAM", b"Player\0"),
                                 sub(b"RNAM", b"smoke_race\0"), sub(b"CNAM", b"smoke_class\0"),
                                 sub(b"ANAM", b"\0"), sub(b"BNAM", b"\0"), sub(b"KNAM", b"\0"),
                                 sub(b"NPDT", struct.pack("<h8B27BBhhhBBBBi", 1, *([50] * 8), *([5] * 27),
                                                          0, 50, 50, 50, 50, 0, 0, 0, 0)),
                                 sub(b"FLAG", struct.pack("<i", 0))]))
    for name in gmst_names():
        if name[0] == "f":
            value = sub(b"FLTV", struct.pack("<f", 1.0))
        elif name[0] == "i":
            value = sub(b"INTV", struct.pack("<i", 1))
        else:
            value = sub(b"STRV", b"x\0")
        records.append(rec(b"GMST", [sub(b"NAME", name.encode() + b"\0"), value]))
    # TES3 header: version 1.3, type 0, 32 byte company, 256 byte description, record count
    hedr = struct.pack("<fI32s256sI", 1.3, 0, b"OpenFallout", b"startup smoke test", len(records))
    return rec(b"TES3", [sub(b"HEDR", hedr)]) + b"".join(records)


def start_xvfb():
    if os.environ.get("DISPLAY"):
        return None
    xvfb = shutil.which("Xvfb")
    if xvfb is None:
        sys.exit("DISPLAY is not set and Xvfb is not installed")
    display = ":%d" % (90 + os.getpid() % 100)
    process = subprocess.Popen([xvfb, display, "-screen", "0", "1280x720x24", "+extension", "GLX"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.environ["DISPLAY"] = display
    time.sleep(2)
    return process


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, type=Path, help="build directory with the openfallout program")
    parser.add_argument("--seconds", type=float, default=15, help="how long to let the main menu run")
    parser.add_argument("--keep", action="store_true", help="keep the temporary directory and print its path")
    args = parser.parse_args()

    build = args.build.resolve()
    program = build / "openfallout"
    if not program.exists():
        sys.exit(f"{program} does not exist")

    work = Path(tempfile.mkdtemp(prefix="openfallout-smoke-"))
    data = work / "data"
    data.mkdir()
    (data / "smoke.omwgame").write_bytes(game_file())
    home = work / "home"
    home.mkdir()
    runtime = work / "runtime"
    runtime.mkdir(mode=0o700)

    xvfb = start_xvfb()
    env = dict(os.environ, HOME=str(home), XDG_RUNTIME_DIR=str(runtime), LIBGL_ALWAYS_SOFTWARE="1")
    for name in ("XDG_CONFIG_HOME", "XDG_DATA_HOME"):
        env.pop(name, None)
    command = [str(program), "--resources", str(build / "resources"), "--data", str(data),
               "--content", "smoke.omwgame", "--no-grab"]
    # The engine shows a dialog instead of logging a fatal error when stdin is not a terminal, so give it one.
    master, slave = pty.openpty()
    process = subprocess.Popen(command, stdin=slave, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    os.close(slave)
    try:
        process.wait(timeout=args.seconds)
    except subprocess.TimeoutExpired:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            print("the engine did not stop within 30 seconds of SIGTERM")
    os.close(master)
    if xvfb is not None:
        xvfb.terminate()

    log = (home / ".config" / "openfallout" / "openfallout.log")
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    problems = [line for line in text.splitlines()
                if re.search(r"Can't start|Lua error|Fatal error|Failed to load Lua|l10n.*(error|fail)", line, re.I)]
    scripts = re.search(r"Lua scripts configuration \((\d+) scripts\)", text)
    languages = len(re.findall(r'Language file ".*" is enabled', text))
    print(f"log: {log}")
    print(f"Lua scripts configured: {scripts.group(1) if scripts else 'none'}; l10n files enabled: {languages}")
    for line in problems:
        print("PROBLEM:", line)
    ok = not problems and scripts is not None and languages > 0 and "Quitting peacefully" in text
    if not scripts or not languages:
        print("PROBLEM: the engine did not get as far as loading the scripts and l10n files")
    elif "Quitting peacefully" not in text:
        print("PROBLEM: the log does not end with 'Quitting peacefully'")
    if args.keep:
        print(f"kept {work}")
    else:
        shutil.rmtree(work, ignore_errors=True)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
