#!/usr/bin/env python3
"""Run a scripted check ("round") of an OpenFallout build on the real game, without taking over anybody's computer.

A round is one command: it copies a save and the settings into a private folder (nothing of the player's own saves or
configuration is ever written), starts the engine with `--headless` (the window is never shown, the keyboard, the pointer
and the screen are left alone, no sound) at low priority, lets the scenario file drive the game from inside
(scripts/openfallout/round/README.md), and then writes a report: the `ROUND` lines of the log, the first warnings and
errors, whether the engine ever became the frontmost application (macOS), the screenshots as JPEG files and the frame times.

    round.py profile --engine build/openfallout            writes the profile of this computer (once)
    round.py build --repo . --ref origin/branch            incremental build in one persistent folder
    round.py run rounds/m13-mountains.json                 runs the round (the variants of the file, `--parallel` at a time)
    round.py run --scenario scenarios/smoke.lua --game nv --save latest
    round.py check                                         what a run would use, without starting the engine

The round file is JSON: `name`, `game` (nv, fo3 or ttw: a key of the profile's `games`), `scenario` (a Lua file, relative to
the round file), `save` (a file, "latest" for the newest save of the profile's `saves` folder, or none: then `start` is a cell
for `--start`), `settings` ({"Section": {"key": "value"}} merged over the profile's settings file), `resolution`,
`framerate`, `limit` (seconds), `env` (environment variables of the engine), `variants` (a list of {"name", "settings", "env"}: one
engine start each) and `extra_args`.
"""
import argparse
import concurrent.futures
import json
import os
import platform
import re
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SAVE_SUFFIXES = (".fos", ".ess", ".omwsave", ".fo3save")
ROOT = HERE.parents[2]
PROFILE_DEFAULT = Path(os.environ.get("OPENFALLOUT_ROUND_PROFILE", Path.home() / ".config" / "openfallout-rounds.json"))
IS_MAC = platform.system() == "Darwin"

# What every run changes in the settings, under what the round file asks for: a plain window of a fixed size (a screenshot is
# exactly that many pixels), no vsync and a low frame rate limit (it is a check, not a game, and it shares the computer)
DEFAULT_SETTINGS = {
    "Video": {"window mode": "2", "vsync mode": "0", "framerate limit": "30", "resolution x": "1280", "resolution y": "720"},
}

LOG_LINE = re.compile(r"^\[[\d:. ]+ ?([A-Z])\] ?(.*)$")
ROUND_LINE = re.compile(r"ROUND\t(.*)$")
SHOT_LINE = re.compile(r"^shot\t([^\t]*)\tn=(\d+)")
NUMBERS = re.compile(r"0x[0-9a-fA-F]+|-?\d+(?:\.\d+)?")


# ---------------------------------------------------------------------------------------------------------------- settings

def parse_settings(text):
    """{section: {key: value}} of a settings.cfg (ordered)."""
    result, section = {}, None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or stripped.startswith(";"):
            continue
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped[1:-1].strip()
            result.setdefault(section, {})
        elif "=" in stripped and section is not None:
            key, value = stripped.split("=", 1)
            result[section][key.strip()] = value.strip()
    return result


def merge_settings(base, *layers):
    """The settings of `base` (text or a dict) with each layer ({section: {key: value}}) put over it, as settings.cfg text."""
    merged = parse_settings(base) if isinstance(base, str) else {s: dict(v) for s, v in base.items()}
    for layer in layers:
        for section, values in (layer or {}).items():
            merged.setdefault(section, {}).update({key: str(value) for key, value in values.items()})
    lines = []
    for section, values in merged.items():
        lines.append(f"[{section}]")
        lines.extend(f"{key} = {value}" for key, value in values.items())
        lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------------------------------------------------- log

def round_lines(text):
    """The text after `ROUND<tab>` of each line of the log, in order."""
    found = []
    for line in text.splitlines():
        match = ROUND_LINE.search(line)
        if match:
            found.append(match.group(1))
    return found


def log_problems(text, limit=10):
    """The distinct warning and error lines of the log, numbers and ids folded to `#` so that the same message with other
    numbers counts once: [(level, message, count)], the first `limit` of each level in the order they were first seen."""
    seen, order = {}, []
    for line in text.splitlines():
        match = LOG_LINE.match(line)
        if not match or match.group(1) not in ("W", "E"):
            continue
        level, message = match.group(1), match.group(2)
        if "ROUND\t" in message:
            continue
        key = (level, NUMBERS.sub("#", message)[:200])
        if key not in seen:
            seen[key] = [message[:300], 0]
            order.append(key)
        seen[key][1] += 1
    result, kept = [], {"W": 0, "E": 0}
    for key in order:
        if kept[key[0]] < limit:
            kept[key[0]] += 1
            result.append((key[0], seen[key][0], seen[key][1]))
    return result


def count_levels(text):
    counts = {"W": 0, "E": 0}
    for line in text.splitlines():
        match = LOG_LINE.match(line)
        if match and match.group(1) in counts and "ROUND\t" not in line:
            counts[match.group(1)] += 1
    return counts


def shot_names(lines):
    """[(number, name)] of the `shot` lines of the scenario, in order."""
    result = []
    for line in lines:
        match = SHOT_LINE.match(line)
        if match:
            result.append((int(match.group(2)), match.group(1)))
    return result


# ------------------------------------------------------------------------------------------------------------------- focus

def parse_front_pid(front_output, info_output):
    """The process id of the frontmost application from the output of `lsappinfo front` (the ASN, "ASN:0x0-0x2b02b:") and of
    `lsappinfo info -only pid <asn>`. That prints `"pid"=1234` on some macOS versions and, on others, a block of the whole entry
    (`pid = 1234 type="Foreground" ...`, the filter ignored): the first line that starts with the key counts, so `parentPID`
    does not. None when either cannot be read."""
    if not re.search(r"ASN:", front_output or ""):
        return None
    match = re.search(r'(?im)^\s*"?pid"?\s*=\s*(\d+)', info_output or "")
    return int(match.group(1)) if match else None


def front_pid():
    """The pid of the frontmost application on macOS; None elsewhere or when it cannot be told."""
    if not IS_MAC or shutil.which("lsappinfo") is None:
        return None
    try:
        front = subprocess.run(["lsappinfo", "front"], capture_output=True, text=True, timeout=5).stdout.strip()
        info = subprocess.run(["lsappinfo", "info", "-only", "pid", front], capture_output=True, text=True, timeout=5).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    return parse_front_pid(front, info)


class FocusSampler(threading.Thread):
    """Once a second, while the engine runs, notes whether it is the frontmost application. A headless run must never be:
    that is what takes the keyboard of the person at the computer."""

    def __init__(self, pid, sample=front_pid):
        super().__init__(daemon=True)
        self.pid, self.sample, self.stop_event = pid, sample, threading.Event()
        self.samples = self.front = 0
        self.available = False

    def run(self):
        while not self.stop_event.is_set():
            found = self.sample()
            if found is not None:
                self.available = True
                self.samples += 1
                if found == self.pid:
                    self.front += 1
            self.stop_event.wait(1.0)

    def stop(self):
        self.stop_event.set()
        self.join(timeout=5)


# ---------------------------------------------------------------------------------------------------------------- screenshots

def convert_picture(source, target, width=1280):
    """Writes `source` as a JPEG of quality 85 at most `width` pixels wide (Pillow when it is there, `sips` on macOS, else the
    file is copied as it is). Gives the path written."""
    try:
        from PIL import Image
        with Image.open(source) as image:
            image = image.convert("RGB")
            if image.width > width:
                image = image.resize((width, round(image.height * width / image.width)))
            image.save(target, "JPEG", quality=85)
        return target
    except ImportError:
        pass
    if IS_MAC and shutil.which("sips"):
        result = subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "85", "--resampleWidth", str(width),
                                 str(source), "--out", str(target)], capture_output=True, check=False)
        if result.returncode == 0:
            return target
    fallback = target.with_suffix(source.suffix)
    shutil.copy(source, fallback)
    return fallback


def collect_shots(userdata, shots_dir, names, width=1280):
    """Copies the screenshots of the run to `shots_dir` as `<n>-<name>.jpg`: the engine numbers them `screenshotNNN` in the
    order they were taken, and the scenario logged `shot <name> n=<k>` for each, so the k-th file is the one of the k-th line."""
    files = sorted((userdata / "screenshots").glob("screenshot*.*")) if (userdata / "screenshots").is_dir() else []
    shots_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for index, (number, name) in enumerate(names):
        if index >= len(files):
            written.append((number, name, None))
            continue
        safe = re.sub(r"[^A-Za-z0-9_.-]+", "-", name) or "shot"
        written.append((number, name, convert_picture(files[index], shots_dir / f"{number:02d}-{safe}.jpg", width)))
    return written, max(0, len(files) - len(names))


# --------------------------------------------------------------------------------------------------------------------- runs

def load_profile(path):
    if not Path(path).exists():
        sys.exit(f"no profile at {path}: run `round.py profile --engine <the openfallout program>` first")
    return json.loads(Path(path).read_text(encoding="utf-8"))


def load_round(path, args):
    """The round of the file, with what the command line says over it."""
    config = json.loads(Path(path).read_text(encoding="utf-8")) if path else {}
    base = Path(path).resolve().parent if path else Path.cwd()
    for name in ("game", "save", "start", "limit", "name"):
        if getattr(args, name, None) not in (None, ""):
            config[name] = getattr(args, name)
    scenario = args.scenario or config.get("scenario")
    if scenario is None:
        sys.exit("no scenario: name one in the round file (\"scenario\") or with --scenario")
    scenario_path = Path(scenario)
    if not scenario_path.is_absolute():
        scenario_path = (base / scenario_path) if (base / scenario_path).exists() else (HERE / scenario_path)
    config["scenario"] = scenario_path.resolve()
    config.setdefault("name", scenario_path.stem)
    config.setdefault("game", "nv")
    config.setdefault("limit", 300)
    config.setdefault("variants", [{"name": "run"}])
    return config


def absolute(path):
    """An absolute path that keeps symbolic links: the engine runs in the folder of its run, so no path may stay relative."""
    return Path(os.path.abspath(Path(path).expanduser()))


GAME_MASTERS = {"nv": "falloutnv.esm", "fo3": "fallout3.esm"}
TTW_MASTER = "taleoftwowastelands.esm"


def save_content_files(path):
    """The content files a save lists (the `DEPE` sub-records of its header, in the first bytes of the file), or None when the
    header cannot be read."""
    try:
        with open(path, "rb") as stream:
            head = stream.read(65536)
    except OSError:
        return None
    files, at = [], head.find(b"DEPE")
    while at != -1 and head[at:at + 4] == b"DEPE" and at + 8 <= len(head):
        size = struct.unpack_from("<I", head, at + 4)[0]
        data = head[at + 8:at + 8 + size]
        if len(data) < size:
            break
        files.append(data.split(b"\0", 1)[0].decode("utf-8", "replace"))
        at += 8 + size
    return files or None


def save_is_for(path, game, content=()):
    """Whether a save was made with the content of `game`, as the engine tells it (`Character::addSlot`: the first game file of
    the save is the game's). A save whose header cannot be read is not turned away."""
    files = save_content_files(path)
    if files is None:
        return True
    lowered = [name.lower() for name in files]
    if game == "ttw":
        return TTW_MASTER in lowered
    master = next((c.lower() for c in content if c.lower().endswith((".esm", ".omwgame"))), GAME_MASTERS.get(game))
    if master is None:
        return True
    first = next((name for name in lowered if name.endswith((".esm", ".omwgame"))), None)
    return first == master and not (game == "nv" and TTW_MASTER in lowered)


def newest_save(folder, accept=None):
    """The newest save of a folder that `accept` takes (all, when it is None). The engine keeps its saves one level down, in a
    folder for each character (`saves/<Character>/<slot>.omwsave`); saves laid straight into the folder are found too."""
    folder = Path(folder)
    saves = [path for pattern in ("*", "*/*") for path in folder.glob(pattern)
             if path.is_file() and path.suffix.lower() in SAVE_SUFFIXES]
    if not saves:
        saves = [path for path in folder.glob("*") if path.is_file()]
    if not saves:
        sys.exit(f"no save in {folder}")
    found = [path for path in saves if accept is None or accept(path)]
    if not found:
        sys.exit(f"no save of this game in {folder} ({len(saves)} save(s) there are of other games)")
    return max(found, key=lambda path: path.stat().st_mtime)


def resolve_save(config, profile):
    save = config.get("save")
    if save in (None, ""):
        return None
    if save == "latest":
        folder = (profile.get("saves") or {}).get(config["game"])
        if not folder:
            sys.exit(f"the profile has no `saves` folder for {config['game']}")
        content = ((profile.get("games") or {}).get(config["game"]) or {}).get("content", [])
        return newest_save(folder, lambda path: save_is_for(path, config["game"], content))
    path = Path(save).expanduser()
    if not path.exists():
        sys.exit(f"no save {path}")
    return path


def engine_command(profile, config, run_dir, save, extra=()):
    """The command line of one engine start."""
    game = (profile.get("games") or {}).get(config["game"])
    if game is None:
        sys.exit(f"the profile has no game `{config['game']}`")
    engine = absolute(profile["engine"])
    command = [str(engine)]
    resources = profile.get("resources") or (engine.parent / "resources")
    command += ["--resources", str(absolute(resources))]
    command += ["--config", str(run_dir / "config"), "--user-data", str(run_dir / "userdata")]
    for data in game.get("data", []):
        command += ["--data", str(absolute(data))]
    command += ["--data", str(HERE), "--data", str(run_dir / "data")]
    # The plugins of the profile are the whole list (the player's own configuration is not read, see `prepare_run`); with no list
    # the engine takes the plugins the selected game makes active, as the game itself does.
    for content in game.get("content", []):
        command += ["--content", content]
    command += ["--content", "round.omwscripts"]
    command += ["--skip-menu", "--headless", "--no-grab"]
    if save is not None:
        command += ["--load-savegame", str(run_dir / "userdata" / "saves" / save.name)]
    elif config.get("start"):
        command += ["--start", config["start"]]
        if config.get("new_game"):
            command += ["--new-game"]
    command += list(config.get("extra_args", [])) + list(extra)
    return command


def prepare_run(profile, config, variant, run_dir, save):
    """Makes the private folders of one start: config (settings), userdata (a copy of the save) and data (the scenario)."""
    for sub in ("config", "userdata/saves", "data/scripts", "shots"):
        (run_dir / sub).mkdir(parents=True, exist_ok=True)
    base = ""
    if profile.get("settings") and Path(profile["settings"]).expanduser().exists():
        base = Path(profile["settings"]).expanduser().read_text(encoding="utf-8", errors="replace")
    video = {"resolution x": str(config["resolution"][0]), "resolution y": str(config["resolution"][1])} if config.get("resolution") else {}
    if config.get("framerate"):
        video["framerate limit"] = str(config["framerate"])
    layers = [DEFAULT_SETTINGS, {"Video": video}, config.get("settings"), variant.get("settings")]
    (run_dir / "config" / "settings.cfg").write_text(merge_settings(base, *layers), encoding="utf-8")
    # an empty configuration file, so that this folder is a configuration folder of its own (the log and the settings of the run
    # are written here, not into the player's own)
    # `replace=config` drops the configuration files of the player (their `data=`, `content=` and `fallback=` lines) and keeps the
    # first one, the engine's own: a round runs on the game and the plugins of its profile only. Read under the player's, the
    # same master would be named twice (the engine then refuses to start), a `content=` line for the other game would load that
    # game's plugins (any plugin that is named stops the engine from choosing the ones the selected game makes active) and a data
    # directory of the other game would be searched too
    (run_dir / "config" / "openfallout.cfg").write_text("replace=config\n", encoding="utf-8")
    if save is not None:
        shutil.copy(save, run_dir / "userdata" / "saves" / save.name)
    shutil.copy(config["scenario"], run_dir / "data" / "scripts" / "roundscenario.lua")


def priority_prefix(priority):
    """The command that lowers the priority of what follows it: `taskpolicy -b` on macOS (background: lower CPU and disk
    priority, so the person at the computer does not feel the run), `nice -n 10` elsewhere; nothing for `normal`."""
    if priority == "normal":
        return []
    if IS_MAC and shutil.which("taskpolicy"):
        return ["taskpolicy", "-b"]
    return ["nice", "-n", "10"] if shutil.which("nice") else []


class Pictures:
    """Reads the engine's log while it runs: has the scenario ended, and are the pictures it asked for written? The engine
    writes each picture in the background, in the same work queue as the loading of the land, and on a busy Mac that took
    seconds for the first and longer than the game stayed up for the second (round R5/R6), so the run ends when the pictures
    are there, not after a fixed time."""

    def __init__(self, path):
        self.path, self.offset, self.pending = Path(path), 0, ""
        self.asked = self.saved = 0
        self.done = False

    def poll(self):
        """Reads what the log has grown by; True when the scenario ended and every picture it asked for is written."""
        try:
            with open(self.path, "rb") as log:
                log.seek(self.offset)
                data = log.read()
        except OSError:
            return False
        if data:
            self.offset += len(data)
            # only whole lines are counted: a word cut by the end of what was read is read next time
            complete, _, self.pending = (self.pending + data.decode("utf-8", errors="replace")).rpartition("\n")
            self.asked += complete.count("ROUND\tshot\t")
            self.saved += complete.count(" has been saved")
            self.done = self.done or "ROUND\tdone\t" in complete
        return self.done and self.saved >= self.asked


def stop_engine(process):
    """Asks the engine's process group to quit (a polite SIGTERM, the engine quits on its own way), and ends it if it does not."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=30)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def run_engine(command, run_dir, limit, env_extra=None, priority="low"):
    """Starts the engine at low priority and waits for it; gives (exit status, seconds, focus sampler, timed out, ended early).
    The run ends when the engine does, when the scenario ended and its pictures are written (`ended early`: the runner stops
    the engine, which would otherwise wait out a delay made for slow pictures) or when `limit` seconds are over."""
    import pty
    env = dict(os.environ, OPENFALLOUT_HEADLESS="1", **(env_extra or {}))
    prefix = priority_prefix(priority)
    # The engine shows a dialog instead of logging a fatal error when stdin is not a terminal, and a dialog is a window
    master, slave = pty.openpty()
    started = time.monotonic()
    process = subprocess.Popen(prefix + command, stdin=slave, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env,
                               cwd=run_dir, start_new_session=True)
    os.close(slave)
    sampler = FocusSampler(process.pid)
    sampler.start()
    pictures = Pictures(run_dir / "config" / "openfallout.log")
    timed_out = ended_early = False
    while True:
        try:
            process.wait(timeout=0.5)
            break
        except subprocess.TimeoutExpired:
            pass
        if pictures.poll():
            ended_early = True
            stop_engine(process)
            break
        if time.monotonic() - started >= limit:
            timed_out = True
            stop_engine(process)
            break
    sampler.stop()
    os.close(master)
    return process.returncode, time.monotonic() - started, sampler, timed_out, ended_early


def missing_content(text):
    """The content files that the save wanted and the run did not load (the engine warns once for each)."""
    # a file name can hold spaces ("My Terrain Mod.esp"): take everything up to the " is missing" of the engine's message
    return re.findall(r"Saved game dependency (.+?) is missing\.?[ \t]*$", text, re.MULTILINE)


def log_tail(text, count=8):
    """The last lines of the engine's log, each without its time stamp and cut at 200 characters."""
    return [line.split("]", 1)[-1].strip()[:200] for line in text.splitlines() if line.strip()][-count:]


def write_report(run_dir, config, variant, status, seconds, sampler, timed_out, lines, problems, levels, shots, extra_shots,
                 command, tail=(), content_missing=(), ended_early=False):
    name = variant.get("name", "run")
    errors = [line for line in lines if line.startswith("error")]
    # The scenario ended (`done end`): what the round asked is answered, so a crash while the engine quits (a known fault of
    # the Linux builds: the Lua state is closed with references left in it) is a note, not a failed round
    finished = any(line.startswith("done\tend") for line in lines)
    # A screenshot that was asked for and never written is no evidence: the round fails (the engine writes the picture in the
    # background, so a crash while quitting can lose the last ones)
    missing = [shot for _, shot, path in shots if path is None]
    # A headless run loads a save that wants content files the run does not load (the save of a play session wants the plugin of the
    # play): a script file (.omwscripts) is harmless, but a game plugin that is missing can put the player in another place and
    # draw another world, so the round fails unless the round file says `allow_missing_content`
    not_loaded = list(content_missing)
    serious = [file for file in not_loaded if not file.lower().endswith(".omwscripts")]
    content_fails = bool(serious) and not config.get("allow_missing_content")
    verdict = "PASS" if finished and not timed_out and not errors and not missing and not content_fails else "FAIL"
    exit_note = ("" if status == 0 else
                 f"; the engine ended with status {status} after the scenario had finished (a crash while quitting)" if finished
                 else f"; the engine ended with status {status} before the scenario finished")
    if ended_early and finished:
        exit_note = f"; the runner stopped the engine once the scenario had ended and its pictures were written (status {status})"
    focus = ("not measured (macOS only)" if not sampler.available and not IS_MAC else
             "NOT MEASURED: `lsappinfo` gave no answer, so this round cannot show that the engine stayed in the background"
             if not sampler.available else
             f"engine was frontmost in {sampler.front} of {sampler.samples} samples")
    # On a Mac the focus is the point of a headless run: a run that could not measure it proves nothing, so it does not pass
    if (sampler.available and sampler.front) or (IS_MAC and not sampler.available):
        verdict = "FAIL"
    report = {"round": config["name"], "variant": name, "verdict": verdict, "exit_status": status, "exit_note": exit_note.lstrip("; "), "finished": finished,
              "seconds": round(seconds, 1),
              "timed_out": timed_out, "stopped_by_runner": ended_early, "focus_samples": sampler.samples, "focus_front": sampler.front,
              "warnings": levels["W"], "errors": levels["E"], "scenario_errors": errors, "missing_shots": missing,
              "content_not_loaded": not_loaded, "content_not_loaded_serious": serious,
              "shots": [{"n": n, "name": shot, "file": str(path) if path else None} for n, shot, path in shots],
              "round_lines": lines, "command": command}
    (run_dir / "report.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    out = [f"# Round {config['name']} / {name}: {verdict}", "",
           f"- exit status {status}{' (timed out and stopped)' if timed_out else ''}, {seconds:.0f} s, "
           f"scenario {'finished' if finished else 'NOT finished'}",
           f"- focus: {focus}",
           f"- log: {levels['W']} warnings, {levels['E']} errors; scenario errors: {len(errors)}" + exit_note,
           f"- screenshots: {sum(1 for s in shots if s[2])} of {len(shots)}" + (f" ({extra_shots} more than the scenario took)" if extra_shots else "") +
           (f"; MISSING, so the round fails: {', '.join(missing)}" if missing else ""),
           *([f"- content files of the save that were not loaded: {', '.join(not_loaded)}" +
              (" (script files only, harmless)" if not serious else
               " (a game plugin: the world may differ from the save's, so the round fails; `allow_missing_content` in the round "
               "file accepts it)" if content_fails else " (allowed by the round file)")] if not_loaded else []),
           "", "## Scenario lines", "", "```"] + lines[:400] + ["```", ""]
    if tail and not finished:
        # What the engine was doing when it stopped answering: a round that never began is told by its last lines
        out += ["## Last lines of the log (the scenario did not finish)", "", "```"] + list(tail) + ["```", ""]
    if problems:
        out += ["## First warnings and errors", ""] + [f"- {level} x{count}: {message}" for level, message, count in problems] + [""]
    out += ["## Screenshots", ""] + [f"- {n:02d} {shot}: {path.name if path else 'MISSING'}" for n, shot, path in shots] + [""]
    (run_dir / "report.md").write_text("\n".join(out), encoding="utf-8")
    return report


def run_variant(profile, config, variant, out_dir, save, priority="low"):
    name = re.sub(r"[^A-Za-z0-9_.-]+", "-", variant.get("name", "run"))
    run_dir = out_dir / name
    prepare_run(profile, config, variant, run_dir, save)
    command = engine_command(profile, config, run_dir, save)
    env = {**config.get("env", {}), **variant.get("env", {})}
    status, seconds, sampler, timed_out, ended_early = run_engine(command, run_dir, config["limit"] + 30, env, priority)
    log = run_dir / "config" / "openfallout.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    (run_dir / "openfallout.log").write_text(text, encoding="utf-8")
    lines = round_lines(text)
    shots, extra = collect_shots(run_dir / "userdata", run_dir / "shots", shot_names(lines), config.get("shot_width", 1280))
    return write_report(run_dir, config, variant, status, seconds, sampler, timed_out, lines, log_problems(text),
                        count_levels(text), shots, extra, command, tail=log_tail(text),
                        content_missing=missing_content(text), ended_early=ended_early)


def command_run(args):
    profile = load_profile(args.profile)
    config = load_round(args.round, args)
    save = resolve_save(config, profile)
    out_root = absolute(args.out or profile.get("out") or Path(tempfile.gettempdir()) / "openfallout-rounds")
    out_dir = out_root / (time.strftime("%Y%m%d-%H%M%S-") + config["name"])
    out_dir.mkdir(parents=True)
    variants = config["variants"]
    if args.variant:
        variants = [v for v in variants if v.get("name") in args.variant]
        if not variants:
            sys.exit(f"no variant named {', '.join(args.variant)} in round {config['name']}")
    print(f"round {config['name']}: {len(variants)} start(s), {args.parallel} at a time, results in {out_dir}", flush=True)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.parallel)) as pool:
        futures = {pool.submit(run_variant, profile, config, v, out_dir, save, args.priority): v for v in variants}
        for future in concurrent.futures.as_completed(futures):
            report = future.result()
            results.append(report)
            print(f"  {report['variant']}: {report['verdict']} in {report['seconds']} s, "
                  f"{len(report['shots'])} shot(s), {report['errors']} E / {report['warnings']} W", flush=True)
    rows = ["# Round " + config["name"], "", "| variant | verdict | seconds | shots | errors | warnings | focus |", "|---|---|---|---|---|---|---|"]
    for report in sorted(results, key=lambda r: r["variant"]):
        focus = f"{report['focus_front']}/{report['focus_samples']}" if report["focus_samples"] else "-"
        rows.append(f"| {report['variant']} | {report['verdict']} | {report['seconds']} | "
                    f"{sum(1 for s in report['shots'] if s['file'])}/{len(report['shots'])} | {report['errors']} | "
                    f"{report['warnings']} | {focus} |")
    (out_dir / "summary.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
    print("\n".join(rows))
    print(f"summary: {out_dir / 'summary.md'}")
    return 0 if all(r["verdict"] == "PASS" for r in results) else 1


# ------------------------------------------------------------------------------------------------------------ profile, build

def command_profile(args):
    engine = Path(args.engine).expanduser().resolve()
    if not engine.exists():
        sys.exit(f"{engine} does not exist")
    home = Path.home()
    candidates = {
        "nv": [home / "Games" / "Fallout New Vegas", home / "Games" / "Fallout New Vegas" / "Data"],
        "fo3": [home / "Games" / "Fallout 3", home / "Games" / "Fallout 3" / "Data"],
    }
    games = {}
    for key, master in (("nv", "FalloutNV.esm"), ("fo3", "Fallout3.esm")):
        for folder in candidates[key]:
            if (folder / master).exists():
                # `content` empty: the plugins the game makes active, as when the game is played (a save wants the same ones)
                games[key] = {"data": [str(folder)], "content": []}
                break
    if IS_MAC:
        userdata = home / "Library" / "Application Support" / "openfallout"
    else:
        userdata = home / ".local" / "share" / "openfallout"
    # The settings of the player's own configuration folder are read by the engine as the defaults under the run's own, so
    # the profile needs no `settings` unless a file other than that one is wanted
    saves = absolute(args.saves) if args.saves else userdata / "saves"
    profile = {"engine": str(engine), "games": games, "saves": {key: str(saves) for key in games},
               "out": str(absolute(args.out)) if args.out else None}
    # a macOS build is a bundle: the program is in Contents/MacOS and the resources in Contents/Resources
    bundle_resources = engine.parent.parent / "Resources" / "resources"
    if engine.parent.name == "MacOS" and bundle_resources.is_dir():
        profile["resources"] = str(bundle_resources)
    profile = {key: value for key, value in profile.items() if value is not None}
    target = Path(args.profile)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {target}:\n{json.dumps(profile, indent=2)}")
    if not games:
        print("no game found under ~/Games: edit the `games` of the profile", file=sys.stderr)
    if not any(True for pattern in ("*.omwsave", "*/*.omwsave") for _ in saves.glob(pattern)):
        print(f"no save in {saves}: `\"save\": \"latest\"` would find none. Where the game is played the engine keeps its saves "
              "(a --user-data folder of its own?): run `profile` again with --saves <that folder>/saves", file=sys.stderr)
    return 0


def command_check(args):
    profile = load_profile(args.profile)
    config = load_round(args.round, args)
    save = resolve_save(config, profile)
    probe = Path(tempfile.mkdtemp(prefix="round-check-"))
    command = engine_command(profile, config, probe, save)
    print(json.dumps({"round": config["name"], "scenario": str(config["scenario"]), "save": str(save) if save else None,
                      "variants": [v.get("name") for v in config["variants"]], "command": command}, indent=1))
    shutil.rmtree(probe, ignore_errors=True)
    return 0


def command_build(args):
    """Brings one persistent folder up to a commit and builds the engine in it: only what changed is compiled."""
    repo, worktree, build = Path(args.repo).resolve(), Path(args.worktree).resolve(), Path(args.build_dir).resolve()
    prefix = priority_prefix("low")
    started = time.monotonic()

    def run(*command, cwd=None):
        print("+", " ".join(command), flush=True)
        subprocess.run(list(command), cwd=cwd, check=True)

    if args.ref:
        if "/" in args.ref and args.ref.startswith("origin/"):
            run("git", "-C", str(repo), "fetch", "origin", args.ref.split("/", 1)[1])
        if (worktree / ".git").exists():
            run("git", "-C", str(worktree), "checkout", "--detach", args.ref)
        else:
            run("git", "-C", str(repo), "worktree", "add", "--detach", str(worktree), args.ref)
    elif not worktree.exists():
        sys.exit(f"{worktree} does not exist: give --ref")
    commit = subprocess.run(["git", "-C", str(worktree), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    if not (build / "CMakeCache.txt").exists():
        generator = ["-G", "Ninja"] if shutil.which("ninja") else []
        launcher = ["-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"] if shutil.which("ccache") else []
        run("cmake", *generator, "-S", str(worktree), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release", *launcher,
            "-DBUILD_LAUNCHER=OFF", "-DBUILD_WIZARD=OFF", "-DBUILD_OPENCS=OFF", *args.cmake_arg)
    run(*prefix, "cmake", "--build", str(build), "--target", "openfallout", "-j", str(args.jobs or os.cpu_count() or 4))
    seconds = time.monotonic() - started
    (build / "round-build.json").write_text(json.dumps({"commit": commit, "seconds": round(seconds, 1)}), encoding="utf-8")
    print(f"built {commit[:10]} in {seconds:.0f} s: {build / 'openfallout'}")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--profile", default=str(PROFILE_DEFAULT), help="the profile of this computer (default %(default)s)")
    sub = parser.add_subparsers(dest="command", required=True)

    one = sub.add_parser("profile", help="write the profile of this computer")
    one.add_argument("--engine", required=True)
    one.add_argument("--out", help="where the results of the runs go")
    one.add_argument("--saves", help="the folder `\"save\": \"latest\"` takes the newest save from (default: the engine's own "
                                    "user-data folder)")
    one.set_defaults(handler=command_profile)

    for name, handler, text in (("run", command_run, "run a round"), ("check", command_check, "show what a run would use")):
        sp = sub.add_parser(name, help=text)
        sp.add_argument("round", nargs="?", help="the round file (JSON)")
        sp.add_argument("--scenario")
        sp.add_argument("--game")
        sp.add_argument("--save")
        sp.add_argument("--start")
        sp.add_argument("--name")
        sp.add_argument("--limit", type=int)
        sp.add_argument("--out")
        sp.add_argument("--variant", action="append", help="only this variant (repeat for more)")
        sp.add_argument("--parallel", type=int, default=1, help="how many engine starts at a time")
        sp.add_argument("--priority", choices=("low", "normal"), default="low",
                        help="low (the default) keeps the run out of the way of whoever uses the computer")
        sp.set_defaults(handler=handler)

    sp = sub.add_parser("build", help="incremental build of the engine in one persistent folder")
    sp.add_argument("--repo", default=str(ROOT))
    sp.add_argument("--worktree", default=str(ROOT.parent / "round-worktree"))
    sp.add_argument("--build-dir", default=str(ROOT.parent / "round-build"))
    sp.add_argument("--ref", help="the commit, branch or origin/<branch> to build")
    sp.add_argument("--jobs", type=int)
    sp.add_argument("--cmake-arg", action="append", default=[], help="an argument of the first cmake configuration")
    sp.set_defaults(handler=command_build)

    args = parser.parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())
