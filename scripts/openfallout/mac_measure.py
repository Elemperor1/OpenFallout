#!/usr/bin/env python3
"""Measure the engine on the fixed scenes of docs/openfallout/measuring.md, the same way every time.

    scripts/openfallout/mac_measure.py scenes
    scripts/openfallout/mac_measure.py run SCENE... --engine <build>/openfallout [--out DIR] [--repeat N]
                                       [--capture time|alloc|game|metal] [--hud] [--label TEXT]
    scripts/openfallout/mac_measure.py compare BEFORE AFTER

`run` starts the engine once per scene and repeat, windowed and in the background (SDL_MAC_BACKGROUND_APP=1: never
frontmost, no keyboard focus, the pointer is not grabbed), each run with its own --config/--user-data folder and settings
of its own (engine defaults, a 1280x720 window, no vsync, no frame-rate limit), so the player's settings and saves are
never read or written. The frame-time probe (scripts/openfallout/perfprobe) lets the player stand and then turn once,
and quits the game. Each run writes `result.json`: frame times of the stand and turn phases, the time from "Starting a
new game" to the first frame of the scene (cell load), the start-up time, the memory footprint (now and peak, from
`footprint`), the machine, the macOS version, the engine version line and the load of the computer before and after.
`--capture` also records an Instruments trace of the stand and turn phases with `xctrace record --attach`.
`compare` prints the medians of two result folders (or files) side by side.

The game data folders come from the round profile (~/.config/openfallout-rounds.json, `games.nv.data`/`games.fo3.data`,
see scripts/openfallout/round/README.md) or from --nv-data / --fo3-data.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
PROBE_DIR = HERE / "perfprobe"
PROFILE_DEFAULT = Path(os.environ.get("OPENFALLOUT_ROUND_PROFILE", Path.home() / ".config" / "openfallout-rounds.json"))

# The fixed scenes: one interior, one exterior and one busy exterior of each game, by their start arguments.
SCENES = {
    "nv-interior": {"game": "nv", "master": "FalloutNV.esm", "start": "GSDocMitchellHouse"},
    "nv-exterior": {"game": "nv", "master": "FalloutNV.esm", "start": "WastelandNV:6,15"},
    "nv-busy": {"game": "nv", "master": "FalloutNV.esm", "start": "WastelandNV:-18,0"},
    "fo3-interior": {"game": "fo3", "master": "Fallout3.esm", "start": "Vault101d"},
    "fo3-exterior": {"game": "fo3", "master": "Fallout3.esm", "start": "Wasteland:18,24"},
    "fo3-busy": {"game": "fo3", "master": "Fallout3.esm", "start": "MegatonWorld:-1,-6"},
}


def engine_command(engine, resources, scene, game_data, run_dir, probe_dir=PROBE_DIR):
    """The engine's command line for one run of a scene."""
    run_dir = Path(run_dir)
    command = [str(engine), "--resources", str(resources),
               "--config", str(run_dir / "config"), "--replace", "config", "--user-data", str(run_dir / "userdata"),
               "--data-local", str(run_dir / "userdata" / "data")]
    for folder in game_data:
        command += ["--data", str(folder)]
    command += ["--data", str(probe_dir), "--data", str(run_dir / "data")]
    command += ["--content", scene["master"], "--content", "perfprobe.omwscripts"]
    return command + ["--skip-menu", "--no-grab", "--start", scene["start"]]

# The Instruments templates of `--capture`: CPU time, allocations, frame time (the game template's frame and display
# intervals; the Metal trace shows the GPU side of the OpenGL-on-Metal layer).
TEMPLATES = {"time": "Time Profiler", "alloc": "Allocations", "game": "Game Performance", "metal": "Metal System Trace"}


def xctrace_command(kind, pid, trace, seconds):
    """`xctrace record` of the running engine (by pid) for `seconds`."""
    return ["xcrun", "xctrace", "record", "--template", TEMPLATES[kind], "--attach", str(pid),
            "--time-limit", f"{int(seconds)}s", "--output", str(trace), "--no-prompt"]

LOG_LINE = re.compile(r"^\[(\d+):(\d+):(\d+(?:\.\d+)?) [A-Z*]\] ?(.*)$")
PERF_FIELD = re.compile(r"(\w+)=(-?\d+(?:\.\d+)?)")


def read_log(text):
    """What a run's openfallout.log says: the version line, the per-phase frame times of the probe (ms), the seconds from
    the first log line and from "Starting a new game" to the probe's first frame, and whether the probe finished."""
    result = {"version": None, "startup_s": None, "cell_load_s": None, "phases": {}, "done": False}
    first = new_game = None
    previous, day = None, 0.0
    for line in text.splitlines():
        match = LOG_LINE.match(line)
        if not match:
            continue
        seconds = int(match.group(1)) * 3600 + int(match.group(2)) * 60 + float(match.group(3))
        if previous is not None and seconds + day < previous - 3600:
            day += 86400  # the clock passed midnight
        seconds += day
        previous = seconds
        message = match.group(4)
        if first is None:
            first = seconds
        if result["version"] is None and message.startswith("OpenFallout version"):
            result["version"] = message.strip()
        elif message.startswith("Starting a new game"):
            new_game = seconds
        elif "\tPERF\t" in message:
            fields = message.split("\tPERF\t", 1)[1].split("\t")
            if fields[0] == "begin" and result["startup_s"] is None:
                result["startup_s"] = round(seconds - first, 3)
                if new_game is not None:
                    result["cell_load_s"] = round(seconds - new_game, 3)
            elif fields[0] == "phase" and len(fields) > 2:
                numbers = {}
                for field in fields[2:]:
                    for key, value in PERF_FIELD.findall(field):
                        if key not in ("cell", "world", "grid", "pos"):
                            numbers[key] = int(value) if key in ("frames", "over33", "over100") else float(value)
                result["phases"][fields[1]] = numbers
            elif fields[0] == "done":
                result["done"] = True
    return result

UNITS = {"B": 1 / 1024 ** 2, "KB": 1 / 1024, "MB": 1.0, "GB": 1024.0}
FOOTPRINT_FIELD = re.compile(r"^\s*(phys_footprint|phys_footprint_peak):\s*([\d.]+)\s*(B|KB|MB|GB)\s*$", re.M)


def read_footprint(text):
    """{footprint_mib, peak_mib} from `footprint --noCategories -p PID`, or None when it names no process."""
    values = {name: float(number) * UNITS[unit] for name, number, unit in FOOTPRINT_FIELD.findall(text)}
    if "phys_footprint" not in values:
        return None
    return {"footprint_mib": round(values["phys_footprint"], 1),
            "peak_mib": round(values.get("phys_footprint_peak", values["phys_footprint"]), 1)}

# What `compare` puts side by side: (label, path in result.json)
METRICS = [
    ("stand p50 ms", ("phases", "stand", "p50")), ("stand p95 ms", ("phases", "stand", "p95")),
    ("turn p50 ms", ("phases", "turn", "p50")), ("turn p95 ms", ("phases", "turn", "p95")),
    ("turn p99 ms", ("phases", "turn", "p99")),
    ("cell load s", ("cell_load_s",)), ("start-up s", ("startup_s",)),
    ("footprint MiB", ("memory", "footprint_mib")), ("peak footprint MiB", ("memory", "peak_mib")),
]


def _value(result, path):
    for key in path:
        if not isinstance(result, dict) or result.get(key) is None:
            return None
        result = result[key]
    return result


def _median(results, scene, path):
    values = [_value(r, path) for r in results if r.get("scene") == scene]
    values = [v for v in values if v is not None]
    return round(statistics.median(values), 3) if values else None


def compare(before, after):
    """Rows (scene, metric, median before, median after, change in %) of two lists of results."""
    scenes = sorted({r["scene"] for r in before + after}, key=lambda s: (list(SCENES) + [s]).index(s))
    rows = []
    for scene in scenes:
        for label, path in METRICS:
            a, b = _median(before, scene, path), _median(after, scene, path)
            if a is None and b is None:
                continue
            change = round((b - a) / a * 100, 1) if a not in (None, 0) and b is not None else None
            rows.append((scene, label, a, b, change))
    return rows


# ------------------------------------------------------------------------------------------------------------------ runs

# Over the engine's defaults: a plain window of a fixed size, no vsync and no frame-rate limit, so that frame times are the
# engine's and not the display's. Nothing of the player's own settings.cfg is read.
SETTINGS = "[Video]\nwindow mode = 2\nresolution x = 1280\nresolution y = 720\nvsync mode = 0\nframerate limit = 0\n"
# The probe's phases for a fixed scene: settle after the load (a picture at the end to check the scene), stand still, then
# one full turn (four pictures). No running: the scene stays the same cells.
PROBE_CONFIG = """return {
    phases = {
        { name = 'settle', secs = 20, shot = true },
        { name = 'stand', secs = 30 },
        { name = 'turn', secs = 40, turn = true },
    },
}
"""
MEASURED_SECONDS = 70  # stand + turn: what a capture records
RUN_LIMIT = 300


def text_of(command):
    try:
        return subprocess.run(command, capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return None


def machine():
    """The computer and its state: chip, memory, macOS, load, and how many compilers are running (a run next to a build is
    noise)."""
    builds = text_of(["pgrep", "-x", "-l", "ninja|clang|clang\\+\\+|ld"]) or ""
    return {
        "chip": text_of(["sysctl", "-n", "machdep.cpu.brand_string"]),
        "memory_gib": round(int(text_of(["sysctl", "-n", "hw.memsize"]) or 0) / 1024 ** 3, 1),
        "macos": " ".join(filter(None, [text_of(["sw_vers", "-productVersion"]), text_of(["sw_vers", "-buildVersion"])])),
        "load": [round(x, 2) for x in os.getloadavg()],
        "build_processes": len([line for line in builds.splitlines() if line.strip()]),
    }


def game_folders(profile_path, args):
    profile = {}
    if Path(profile_path).exists():
        profile = json.loads(Path(profile_path).read_text(encoding="utf-8"))
    games = profile.get("games") or {}
    folders = {"nv": [args.nv_data] if args.nv_data else (games.get("nv") or {}).get("data", []),
               "fo3": [args.fo3_data] if args.fo3_data else (games.get("fo3") or {}).get("data", [])}
    return folders, profile


def prepare(run_dir):
    for sub in ("config", "userdata/data", "data/scripts"):
        (run_dir / sub).mkdir(parents=True, exist_ok=True)
    (run_dir / "config" / "settings.cfg").write_text(SETTINGS, encoding="utf-8")
    (run_dir / "config" / "openfallout.cfg").write_text("replace=config\n", encoding="utf-8")
    (run_dir / "data" / "scripts" / "perfprobeconfig.lua").write_text(PROBE_CONFIG, encoding="utf-8")


def sample_footprint(pid):
    return read_footprint(text_of(["footprint", "--noCategories", "-p", str(pid)]) or "")


def run_once(args, scene_name, run_dir, data):
    """One engine start on one scene; writes and gives result.json."""
    import pty
    import signal
    sys.path.insert(0, str(HERE / "round"))
    import roundguard

    scene = SCENES[scene_name]
    prepare(run_dir)
    engine = Path(args.engine).resolve()
    resources = Path(args.resources).resolve() if args.resources else engine.parent.parent / "Resources" / "resources"
    command = engine_command(engine, resources, scene, data, run_dir)
    if args.nice:
        command = ["nice", "-n", str(args.nice)] + command
    env = dict(os.environ, SDL_MAC_BACKGROUND_APP="1")
    if args.hud:
        env["MTL_HUD_ENABLED"] = "1"
    before = machine()
    system = roundguard.default_system(platform.system() == "Darwin")
    heartbeat = roundguard.Heartbeat(system.ping, system.window_server_pid) if system.heartbeat else None
    if heartbeat is not None:
        heartbeat.start()
    # the engine shows a dialog instead of logging a fatal error when stdin is not a terminal
    master, slave = pty.openpty()
    started = time.monotonic()
    process = subprocess.Popen(command, stdin=slave, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env,
                               cwd=run_dir, start_new_session=True)
    os.close(slave)
    log_path = run_dir / "config" / "openfallout.log"
    capture, samples, stopped = None, {}, None
    seen = ""
    while process.poll() is None:
        time.sleep(0.5)
        try:
            seen = log_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            seen = ""
        if "PERF\tstart\tstand" in seen and "stand_start" not in samples:
            samples["stand_start"] = sample_footprint(process.pid)
            if args.capture:
                capture = subprocess.Popen(xctrace_command(args.capture, process.pid, run_dir / f"{args.capture}.trace",
                                                           MEASURED_SECONDS),
                                           stdout=open(run_dir / "xctrace.log", "w"), stderr=subprocess.STDOUT)
        if "PERF\tphase\tstand" in seen and "stand_end" not in samples:
            samples["stand_end"] = sample_footprint(process.pid)
        if "PERF\tphase\tturn" in seen and "turn_end" not in samples:
            samples["turn_end"] = sample_footprint(process.pid)
        problem = heartbeat.problem() if heartbeat is not None else None
        if problem or time.monotonic() - started > args.limit:
            stopped = problem or f"time limit {args.limit} s"
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            break
    os.close(master)
    if heartbeat is not None:
        heartbeat.stop()
    if capture is not None:
        try:
            capture.wait(timeout=120)
        except subprocess.TimeoutExpired:
            capture.terminate()
    try:
        seen = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        seen = ""
    result = read_log(seen)
    valid = [s for s in samples.values() if s]
    result.update({
        "scene": scene_name, "start": scene["start"], "label": args.label, "commit": args.commit,
        "exit_status": process.returncode, "stopped": stopped, "wall_s": round(time.monotonic() - started, 1),
        "memory": {"footprint_mib": (samples.get("stand_end") or {}).get("footprint_mib"),
                   "peak_mib": max((s["peak_mib"] for s in valid), default=None), "samples": samples},
        "capture": f"{args.capture}.trace" if capture is not None else None,
        "machine_before": before, "machine_after": machine(),
        "when": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
    })
    (run_dir / "result.json").write_text(json.dumps(result, indent=1) + "\n", encoding="utf-8")
    return result


def git_commit():
    return text_of(["git", "-C", str(HERE), "rev-parse", "--short=10", "HEAD"])


def load_results(paths):
    results = []
    for path in map(Path, paths):
        files = [path] if path.is_file() else sorted(path.rglob("result.json"))
        results += [json.loads(f.read_text(encoding="utf-8")) for f in files]
    return results


def print_rows(rows, before="before", after="after"):
    def show(value):
        return "-" if value is None else f"{value:g}"
    print(f"| scene | metric | {before} | {after} | change % |")
    print("|---|---|---:|---:|---:|")
    for scene, label, a, b, change in rows:
        print(f"| {scene} | {label} | {show(a)} | {show(b)} | {show(change)} |")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("scenes", help="list the fixed scenes and their start arguments")
    run = sub.add_parser("run", help="measure scenes (windowed, in the background)")
    run.add_argument("scenes", nargs="+", help="scene names, or `all`")
    run.add_argument("--engine", required=True, help="the openfallout program of the build")
    run.add_argument("--resources", help="its resources folder (default: next to the program in the app bundle)")
    run.add_argument("--out", default="measure-runs", help="folder of the results (default: %(default)s)")
    run.add_argument("--repeat", type=int, default=3, help="starts per scene (default: %(default)s)")
    run.add_argument("--capture", choices=sorted(TEMPLATES), help="also record an Instruments trace of the measured phases")
    run.add_argument("--hud", action="store_true", help="show the Metal performance HUD (MTL_HUD_ENABLED=1)")
    run.add_argument("--label", default="", help="a note stored in every result (what was measured)")
    run.add_argument("--commit", default=None, help="the engine's commit (default: the commit of this checkout)")
    run.add_argument("--nice", type=int, default=0, help="run the engine at this nice value (default: %(default)s)")
    run.add_argument("--limit", type=int, default=RUN_LIMIT, help="seconds before a run is stopped (default: %(default)s)")
    run.add_argument("--gap", type=int, default=10, help="seconds of rest between two starts (default: %(default)s)")
    run.add_argument("--profile", default=str(PROFILE_DEFAULT), help="round profile with the game data folders")
    run.add_argument("--nv-data", help="the New Vegas Data folder (instead of the profile's)")
    run.add_argument("--fo3-data", help="the Fallout 3 Data folder (instead of the profile's)")
    comp = sub.add_parser("compare", help="medians of two result folders or files, side by side")
    comp.add_argument("before")
    comp.add_argument("after")
    args = parser.parse_args(argv)

    if args.command == "scenes":
        for name, scene in SCENES.items():
            print(f"{name:13} --content {scene['master']:14} --start {scene['start']}")
        return 0
    if args.command == "compare":
        print_rows(compare(load_results([args.before]), load_results([args.after])),
                   Path(args.before).name, Path(args.after).name)
        return 0

    names = list(SCENES) if args.scenes == ["all"] else args.scenes
    unknown = [n for n in names if n not in SCENES]
    if unknown:
        parser.error(f"unknown scene(s) {', '.join(unknown)}; `scenes` lists them")
    folders, _ = game_folders(args.profile, args)
    args.commit = args.commit or git_commit()
    out = Path(args.out).resolve() / time.strftime("%Y%m%d-%H%M%S")
    results, first = [], True
    for repeat in range(1, args.repeat + 1):
        for name in names:
            data = folders[SCENES[name]["game"]]
            if not data:
                sys.exit(f"no data folder for {SCENES[name]['game']}: give --{SCENES[name]['game']}-data or a round profile")
            if not first:
                time.sleep(args.gap)
            first = False
            result = run_once(args, name, out / f"{name}-{repeat}", data)
            stand = result["phases"].get("stand", {})
            print(f"{name} #{repeat}: stand p50 {stand.get('p50')} ms, cell load {result['cell_load_s']} s, "
                  f"peak {result['memory']['peak_mib']} MiB, exit {result['exit_status']}"
                  + (f", stopped: {result['stopped']}" if result["stopped"] else ""), flush=True)
            results.append(result)
    print(f"results in {out}")
    print_rows(compare(results, []), "median", "-")
    return 0


if __name__ == "__main__":
    sys.exit(main())
