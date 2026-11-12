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
    round.py preflight                                     what the guards say about this computer now (starts nothing)
    round.py stop | resume                                 the kill switch: stops a running round and refuses new ones | lifts it
    round.py ack-panic                                     after somebody has looked at a panic: the pre-flight stops warning about it

The round file is JSON: `name`, `game` (nv, fo3 or ttw: a key of the profile's `games`), `scenario` (a Lua file, relative to
the round file), `save` (a file, "latest" for the newest save of the profile's `saves` folder, or none: then `start` is a cell
for `--start`), `settings` ({"Section": {"key": "value"}} merged over the profile's settings file), `resolution`,
`framerate`, `limit` (seconds), `env` (environment variables of the engine), `variants` (a list of {"name", "settings", "env"}: one
engine start each) and `extra_args`.

The guards (roundguard.py) keep a round and a build from loading a computer somebody works at: one lock for a build or a run, a
pre-flight that refuses on a stressed computer, a WindowServer heartbeat that ends the engine at once when the window system stops
answering, a kill-switch file and a time limit. See scripts/openfallout/round/README.md, "Guards".
"""
import argparse
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
sys.path.insert(0, str(HERE))
import roundguard  # noqa: E402  (next to this file)

SAVE_SUFFIXES = (".fos", ".ess", ".omwsave", ".fo3save")
ROOT = HERE.parents[2]
PROFILE_DEFAULT = Path(os.environ.get("OPENFALLOUT_ROUND_PROFILE", Path.home() / ".config" / "openfallout-rounds.json"))
IS_MAC = platform.system() == "Darwin"
IS_WINDOWS = os.name == "nt"

# What every run changes in the settings, under what the round file asks for: a plain window of a fixed size (a screenshot is
# exactly that many pixels), no vsync and a low frame rate limit (it is a check, not a game, and it shares the computer; every
# frame of a hidden window still asks the window system for a flush, so fewer frames ask less of it)
DEFAULT_SETTINGS = {
    "Video": {"window mode": "2", "vsync mode": "0", "framerate limit": "20", "resolution x": "1280", "resolution y": "720"},
}

# How often the runner asks which application is frontmost (`lsappinfo`, two processes each time): often enough to see a run take
# the focus, rarely enough not to add to the load of a computer that is already busy
FOCUS_INTERVAL = 5.0

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
    """Every few seconds (`FOCUS_INTERVAL`), while the engine runs, notes whether it is the frontmost application. A headless run must never be:
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
            self.stop_event.wait(FOCUS_INTERVAL)

    def stop(self):
        self.stop_event.set()
        self.join(timeout=5)


# ---------------------------------------------------------------------------------------------------------------- screenshots

def picture_is_complete(path):
    """False for a file the engine began to write and did not finish (empty, a PNG with no end chunk, a JPEG with no end marker): it is
    not a picture and must not count as evidence."""
    try:
        if path.stat().st_size == 0:
            return False
        suffix = path.suffix.lower()
        with path.open("rb") as stream:
            if suffix == ".png":
                if stream.read(8) != b"\x89PNG\r\n\x1a\n":
                    return False
                stream.seek(-12, os.SEEK_END)
                return stream.read(12)[4:8] == b"IEND"
            if suffix in (".jpg", ".jpeg"):
                stream.seek(-2, os.SEEK_END)
                return stream.read(2) == b"\xff\xd9"
        return True
    except OSError:
        return False


def convert_picture(source, target, width=1280):
    """Writes `source` as a JPEG of quality 85 at most `width` pixels wide (Pillow when it is there, `sips` on macOS, else the
    file is copied as it is). Gives the path written, or None where the file is not a whole picture (the shot is then missing)."""
    if not picture_is_complete(source):
        return None
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
    except (OSError, ValueError):
        # Pillow cannot read it (truncated or damaged)
        return None
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
    order they were taken, and the scenario logged `shot <name> n=<k>` for each, so the k-th file is the one of the k-th line (one
    write queue thread by default, and `r.shot` waits for its file: the writes cannot swap unless `preload num threads` is above 1 and
    two shots are asked for back to back)."""
    files = sorted((userdata / "screenshots").glob("screenshot*.*")) if (userdata / "screenshots").is_dir() else []
    shots_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for index, (number, name) in enumerate(names):
        if index >= len(files):
            written.append((number, name, None))
            continue
        safe = safe_name(name) or "shot"
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
    # `--replace config` leaves the player's configuration folder out before anything in it is read (a file of theirs that does not
    # parse cannot stop a round); the engine's own openfallout.cfg and the run's are the only ones. The run's cfg says `replace=config` too.
    command += ["--config", str(run_dir / "config"), "--replace", "config", "--user-data", str(run_dir / "userdata")]
    # The engine's own openfallout.cfg (the one `replace=config` keeps) says `data-local=?userdata?data`, and `?userdata?` is the
    # player's data folder, not the one of `--user-data`: a private, empty one keeps the add-ons the player installed out of the
    # run (a master of the other game in it would change which game is chosen)
    command += ["--data-local", str(run_dir / "userdata" / "data")]
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


def player_config_dir():
    """The folder of the player's own configuration (where the engine keeps its openfallout.cfg and settings.cfg)."""
    home = Path.home()
    if IS_MAC:
        return home / "Library" / "Preferences" / "openfallout"
    if IS_WINDOWS:
        return home / "Documents" / "My Games" / "OpenFallout"
    return Path(os.environ.get("XDG_CONFIG_HOME") or home / ".config") / "openfallout"


def player_user_data_dir():
    """The folder where the engine keeps the saves of the player by default."""
    home = Path.home()
    if IS_MAC:
        return home / "Library" / "Application Support" / "openfallout"
    if IS_WINDOWS:
        # the engine's user data folder is its configuration folder there (components/files/windowspath.cpp)
        return player_config_dir()
    # the engine's own default honours XDG_DATA_HOME (components/files/linuxpath.cpp)
    return Path(os.environ.get("XDG_DATA_HOME") or home / ".local" / "share") / "openfallout"


def prepare_run(profile, config, variant, run_dir, save):
    """Makes the private folders of one start: config (settings), userdata (a copy of the save) and data (the scenario)."""
    for sub in ("config", "userdata/saves", "userdata/data", "data/scripts", "shots"):
        (run_dir / sub).mkdir(parents=True, exist_ok=True)
    # The settings of the player (view distance, field of view, terrain and the rest) are the bottom layer of the run's, then the
    # settings file of the profile if it has one. The run's `openfallout.cfg` says `replace=config`, which also takes the player's
    # settings.cfg out of the engine's own chain of defaults, so they are laid here
    base = ""
    mine = player_config_dir() / "settings.cfg"
    if mine.exists():
        base = mine.read_text(encoding="utf-8", errors="replace")
    profile_layer = {}
    if profile.get("settings") and Path(profile["settings"]).expanduser().exists():
        profile_layer = parse_settings(Path(profile["settings"]).expanduser().read_text(encoding="utf-8", errors="replace"))
    video = {"resolution x": str(config["resolution"][0]), "resolution y": str(config["resolution"][1])} if config.get("resolution") else {}
    if config.get("framerate"):
        video["framerate limit"] = str(config["framerate"])
    layers = [profile_layer, DEFAULT_SETTINGS, {"Video": video}, config.get("settings"), variant.get("settings")]
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


def kill_engine(process):
    """Ends the engine's process group at once (SIGKILL): an engine that waits for a window system that does not answer cannot act on
    a polite SIGTERM, and every second it stays is a second more that it holds up. Gives whether the engine is confirmed gone (the
    process was reaped): on a stalled system it may not be, and the runner must not take that for an end."""
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        return True
    try:
        process.wait(timeout=10)
        return True
    except subprocess.TimeoutExpired:
        return False


def stop_engine(process, guard=None, patience=30.0):
    """Asks the engine's process group to quit (a polite SIGTERM, the engine quits on its own way), and ends it if it does not within
    `patience` seconds, or at once when the guard trips while it waits (the kill switch, a window system that stopped answering).
    Gives (the engine is confirmed gone, the reason that the guard gave while it waited or None)."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return True, None
    waited = 0.0
    while waited < patience:
        try:
            process.wait(timeout=0.5)
            return True, None
        except subprocess.TimeoutExpired:
            waited += 0.5
        tripped = guard.problem() if guard is not None else None
        if tripped:
            return kill_engine(process), tripped
    return kill_engine(process), None


def engine_environment(extra, is_mac):
    """The environment of the engine: the environment of the runner, a headless run that asks the window system for as little as it
    can (SDL is told to leave the screensaver alone, which on macOS is a power assertion `PreventUserIdleDisplaySleep` held for the whole
    run; the application is an accessory one: no Dock icon, no menu bar, never frontmost), and what the round and the variant say
    over that (`"OPENFALLOUT_HEADLESS_POLICY": "foreground"` keeps the engine an ordinary application)."""
    env = {"OPENFALLOUT_HEADLESS": "1", "SDL_VIDEO_ALLOW_SCREENSAVER": "1"}
    if is_mac:
        env["OPENFALLOUT_HEADLESS_POLICY"] = "accessory"
    env.update(extra or {})
    return dict(os.environ, **env)


def run_engine(command, run_dir, limit, env_extra=None, priority="low", guard=None):
    """Starts the engine at low priority and waits for it; gives (exit status, seconds, focus sampler, timed out, ended early,
    guard reason). The run ends when the engine does, when the scenario ended and its pictures are written (`ended early`: the runner
    stops the engine, which would otherwise wait out a delay made for slow pictures), when `limit` seconds are over, or when the guard
    says so (the window system stopped answering, the kill switch, the wall-time limit of the round): then the engine is killed at once."""
    import pty
    env = engine_environment(env_extra, IS_MAC)
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
    tripped = None
    gone = True
    while True:
        try:
            process.wait(timeout=0.5)
            break
        except subprocess.TimeoutExpired:
            pass
        tripped = guard.problem() if guard is not None else None
        if tripped:
            gone = kill_engine(process)
            break
        if pictures.poll():
            ended_early = True
            gone, tripped = stop_engine(process, guard)
            break
        if time.monotonic() - started >= limit:
            timed_out = True
            gone, tripped = stop_engine(process, guard)
            break
    if not gone:
        # The process was not reaped after a SIGKILL (a stalled system): another build or round must not start on top of it, so the
        # kill switch is set, which every later one is refused for until somebody has looked and resumed
        reason = f"the engine (pid {process.pid}) did not end after SIGKILL; look at the process list, then run `round.py resume`"
        print(reason, flush=True)
        # The round is not a pass with an engine that may still run: the reason is the guard's, which the report and the exit say
        tripped = tripped or reason
        if guard is not None:
            guard.state.stop(reason + "\n")
    sampler.stop()
    os.close(master)
    return process.returncode, time.monotonic() - started, sampler, timed_out, ended_early, tripped


def missing_content(text):
    """The content files that the save wanted and the run did not load (the engine warns once for each)."""
    # a file name can hold spaces ("My Terrain Mod.esp"): take everything up to the " is missing" of the engine's message
    return re.findall(r"Saved game dependency (.+?) is missing\.?[ \t]*$", text, re.MULTILINE)


def log_tail(text, count=8):
    """The last lines of the engine's log, each without its time stamp and cut at 200 characters."""
    return [line.split("]", 1)[-1].strip()[:200] for line in text.splitlines() if line.strip()][-count:]


def window_server_text(window_server):
    """`yes (pid 690)`, `NO (690 -> 28288)` or `not measured`, for the report and the summary."""
    if not window_server or window_server.get("same") is None:
        return "not measured"
    if window_server["same"]:
        return f"yes (pid {window_server['start']})"
    return f"NO ({window_server['start']} -> {window_server['end'] if window_server['end'] is not None else 'gone'})"


def write_report(run_dir, config, variant, status, seconds, sampler, timed_out, lines, problems, levels, shots, extra_shots,
                 command, tail=(), content_missing=(), ended_early=False, guard_reason=None, window_server=None, policy=None,
                 evidence=()):
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
    # A guard that ended the engine (the window system stopped answering, the kill switch, the time limit) fails the round, whatever the log says
    if guard_reason:
        verdict = "FAIL"
        exit_note = f"; STOPPED BY A GUARD: {guard_reason}"
    report = {"round": config["name"], "variant": name, "verdict": verdict, "exit_status": status, "exit_note": exit_note.lstrip("; "), "finished": finished,
              "seconds": round(seconds, 1),
              "timed_out": timed_out, "stopped_by_runner": ended_early, "focus_samples": sampler.samples, "focus_front": sampler.front,
              "warnings": levels["W"], "errors": levels["E"], "scenario_errors": errors, "missing_shots": missing,
              "content_not_loaded": not_loaded, "content_not_loaded_serious": serious,
              "guard_tripped": guard_reason, "window_server": window_server, "headless_policy": policy, "evidence": list(evidence),
              "shots": [{"n": n, "name": shot, "file": str(path) if path else None} for n, shot, path in shots],
              "round_lines": lines, "command": command}
    (run_dir / "report.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    out = [f"# Round {config['name']} / {name}: {verdict}", "",
           f"- exit status {status}{' (timed out and stopped)' if timed_out else ''}, {seconds:.0f} s, "
           f"scenario {'finished' if finished else 'NOT finished'}",
           f"- focus: {focus}",
           *([f"- STOPPED BY A GUARD, the engine was killed: {guard_reason}"] if guard_reason else []),
           *([f"- WindowServer same pid: {window_server_text(window_server)}"] if window_server else []),
           *([f"- activation policy: {policy}"] if policy else []),
           *([f"- evidence copied to diagnostics/: {', '.join(evidence)}"] if evidence else []),
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


def headless_policy(text):
    """`accessory set` (or `refused`) from the engine's log line `Headless: activation policy accessory set`, None when it has none."""
    match = re.search(r"Headless: activation policy (\w+) (set|refused)", text)
    return f"{match.group(1)} {match.group(2)}" if match else None


def safe_name(text):
    """A name for a folder or a file: what is not a letter, a digit or `_.-` becomes `-`, and a plus sign becomes `plus` first, so that
    two variants named `yaw+90` and `yaw-90` do not share a folder (round 14F: the first was never run)."""
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", str(text).replace("+", "plus"))


def run_variant(profile, config, variant, out_dir, save, priority="low", guard=None, system=None):
    name = safe_name(variant.get("name", "run"))
    run_dir = out_dir / name
    prepare_run(profile, config, variant, run_dir, save)
    command = engine_command(profile, config, run_dir, save)
    env = {**config.get("env", {}), **variant.get("env", {})}
    began = time.time()
    status, seconds, sampler, timed_out, ended_early, tripped = run_engine(command, run_dir, config["limit"] + 30, env, priority, guard)
    log = run_dir / "config" / "openfallout.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    (run_dir / "openfallout.log").write_text(text, encoding="utf-8")
    lines = round_lines(text)
    shots, extra = collect_shots(run_dir / "userdata", run_dir / "shots", shot_names(lines), config.get("shot_width", 1280))
    # What an investigation of a stalled window system would want, kept with the run (a Mac: the new crash reports and the log of runningboardd)
    evidence = roundguard.collect_evidence(system, run_dir, began) if system is not None and system.heartbeat else []
    return write_report(run_dir, config, variant, status, seconds, sampler, timed_out, lines, log_problems(text),
                        count_levels(text), shots, extra, command, tail=log_tail(text),
                        content_missing=missing_content(text), ended_early=ended_early, guard_reason=tripped,
                        window_server=guard.window_server() if guard is not None else None, policy=headless_policy(text),
                        evidence=evidence)


def make_system():
    """What the guards read from this computer (a Mac: memory, swap, thermal state, processes, crash reports, WindowServer)."""
    return roundguard.default_system(IS_MAC)


def guard_root(args, with_out=True):
    """The folder of the guards' files (lock, kill switch, acknowledgement): the folder of the results that the profile names, or the
    `--out` of the commands that are about the guards (`preflight`, `stop`, `resume`, `ack-panic`). A run passes `with_out=False`: its
    `--out` moves its results only, and a build and a run of one profile must meet the same lock."""
    path = Path(args.profile)
    profile = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    out = (getattr(args, "out", None) if with_out else None) or profile.get("out")
    return absolute(out) if out else Path(tempfile.gettempdir()) / "openfallout-rounds"


def check_computer(system, state, heartbeat=True):
    """The pre-flight: prints what it read and what worries it, and gives the reasons to refuse (an empty list: go on). With `heartbeat`
    the window system is asked once on the way: a heartbeat that cannot reach it would guard nothing."""
    refusals, warnings, facts = roundguard.preflight(system, state)
    if heartbeat and system.heartbeat:
        answered = roundguard.try_ping(system.ping)
        facts["window system"] = "answers" if answered else "NO ANSWER"
        if not answered:
            refusals.append("WindowServer did not answer the heartbeat's test call within 10 s (or the call cannot be made): "
                            "the guard that ends the engine when the window system stops answering would not work")
    print(roundguard.describe_preflight(refusals, warnings, facts), flush=True)
    return refusals, warnings


def wait_settled(state, system, seconds=roundguard.SETTLE_SECONDS, sleep=time.sleep, clock=time.monotonic):
    """Gives the computer a rest after a build or a run that ended a moment ago (a Mac only): waits out what is left of `seconds`."""
    if not system.settles:
        return 0.0
    remaining = state.settle_remaining(seconds=seconds)
    if remaining <= 0:
        return 0.0
    print(f"waiting {remaining:.0f} s: a build or a round ended a moment ago, and the computer is given a rest before the next one", flush=True)
    until = clock() + remaining
    while clock() < until and not state.stopped():
        sleep(min(1.0, max(0.0, until - clock())))
    return remaining


def refusal_exit(refusals):
    print("REFUSED, nothing was started:\n" + "\n".join(f"  - {reason}" for reason in refusals), file=sys.stderr, flush=True)
    return 4


def command_run(args):
    if IS_WINDOWS:
        sys.exit("round.py run works on macOS and Linux only: it gives the engine a pseudo-terminal and stops it by process group")
    if args.parallel != 1:
        print(f"--parallel {args.parallel}: the variants run one after the other (never more than one engine at a time)", flush=True)
    profile = load_profile(args.profile)
    config = load_round(args.round, args)
    save = resolve_save(config, profile)
    out_root = absolute(args.out or profile.get("out") or Path(tempfile.gettempdir()) / "openfallout-rounds")
    # The guards' files stay in one place whatever folder this run puts its results in: a build and a run of one profile must meet the
    # same lock, kill switch and acknowledgement (`--out` moves the results only)
    state, system = roundguard.State(guard_root(args, with_out=False)), make_system()
    variants = config["variants"]
    if args.variant:
        variants = [v for v in variants if v.get("name") in args.variant]
        if not variants:
            sys.exit(f"no variant named {', '.join(args.variant)} in round {config['name']}")
    try:
        with roundguard.Lock(state.lock_file, f"round {config['name']}"):
            refusals, warnings = check_computer(system, state)
            if refusals:
                return refusal_exit(refusals)
            wait_settled(state, system)
            return run_round(args, profile, config, save, out_root, variants, state, system, warnings)
    except roundguard.Refused as refused:
        return refusal_exit(refused.reasons)


def run_round(args, profile, config, save, out_root, variants, state, system, warnings=()):
    """The round itself, under the lock and after the pre-flight: the variants one after the other, with the heartbeat watching the window
    system and the kill switch and the time limit watching the clock."""
    out_dir = out_root / (time.strftime("%Y%m%d-%H%M%S-") + config["name"])
    out_dir.mkdir(parents=True)
    heartbeat = roundguard.Heartbeat(system.ping, system.window_server_pid) if system.heartbeat else None
    guard = roundguard.RunGuard(state, heartbeat, wall_seconds=args.wall)
    if heartbeat is not None:
        heartbeat.start()
    print(f"round {config['name']}: {len(variants)} start(s), one at a time, results in {out_dir}; "
          f"kill switch: touch {state.stop_file}", flush=True)
    results, skipped = [], []
    try:
        for index, variant in enumerate(variants):
            if index and system.settles:
                time.sleep(roundguard.VARIANT_GAP_SECONDS)
            if guard.problem():
                skipped.append(variant.get("name", "run"))
                continue
            report = run_variant(profile, config, variant, out_dir, save, args.priority, guard, system)
            results.append(report)
            print(f"  {report['variant']}: {report['verdict']} in {report['seconds']} s, "
                  f"{len(report['shots'])} shot(s), {report['errors']} E / {report['warnings']} W", flush=True)
    finally:
        if heartbeat is not None:
            heartbeat.stop()
        state.heavy_ended("round")
    rows = ["# Round " + config["name"], "", "| variant | verdict | seconds | shots | errors | warnings | focus |", "|---|---|---|---|---|---|---|"]
    for report in sorted(results, key=lambda r: r["variant"]):
        focus = f"{report['focus_front']}/{report['focus_samples']}" if report["focus_samples"] else "-"
        rows.append(f"| {report['variant']} | {report['verdict']} | {report['seconds']} | "
                    f"{sum(1 for s in report['shots'] if s['file'])}/{len(report['shots'])} | {report['errors']} | "
                    f"{report['warnings']} | {focus} |")
    rows.append("")
    rows.append(f"- WindowServer same pid: {window_server_text(guard.window_server())}")
    rows.extend(f"- pre-flight warning: {warning}" for warning in warnings)
    if guard.tripped:
        rows.append(f"- STOPPED BY A GUARD, the engine was killed and the round ended: {guard.tripped}")
    if skipped:
        rows.append(f"- not run: {', '.join(skipped)}")
    (out_dir / "summary.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
    print("\n".join(rows))
    print(f"summary: {out_dir / 'summary.md'}")
    if guard.tripped:
        return 3
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
    userdata = player_user_data_dir()
    # The settings.cfg of the player's own configuration folder is laid under the run's by `prepare_run`, so the profile needs no
    # `settings` unless a file over that one is wanted
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
    """Brings one persistent folder up to a commit and builds the engine in it: only what changed is compiled. At most six jobs, at low
    priority, under the same lock as a run, and only on a computer the pre-flight accepts."""
    jobs = args.jobs or min(roundguard.MAX_BUILD_JOBS, os.cpu_count() or 4)
    if jobs > roundguard.MAX_BUILD_JOBS and not args.allow_heavy:
        print(f"--jobs {jobs}: held to {roundguard.MAX_BUILD_JOBS}, a bigger build slows a computer somebody works at to a crawl for as long "
              f"as it lasts (`--allow-heavy` when the computer is free to be used up)", flush=True)
        jobs = roundguard.MAX_BUILD_JOBS
    state, system = roundguard.State(guard_root(args)), make_system()
    try:
        with roundguard.Lock(state.lock_file, "build"):
            refusals, _ = check_computer(system, state, heartbeat=False)
            if refusals:
                return refusal_exit(refusals)
            wait_settled(state, system)
            try:
                return build_engine(args, jobs)
            finally:
                state.heavy_ended("build")
    except roundguard.Refused as refused:
        return refusal_exit(refused.reasons)


def build_engine(args, jobs):
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
    run(*prefix, "cmake", "--build", str(build), "--target", "openfallout", "-j", str(jobs))
    seconds = time.monotonic() - started
    (build / "round-build.json").write_text(json.dumps({"commit": commit, "seconds": round(seconds, 1)}), encoding="utf-8")
    print(f"built {commit[:10]} in {seconds:.0f} s: {build / 'openfallout'}")
    return 0


def command_preflight(args):
    """What the guards say about this computer now. Reads the state of the computer and asks the window system once; starts no engine
    and no build."""
    state, system = roundguard.State(guard_root(args)), make_system()
    refusals, _ = check_computer(system, state)
    return 4 if refusals else 0


def command_stop(args):
    state = roundguard.State(guard_root(args))
    state.stop(args.note or "")
    print(f"kill switch set: {state.stop_file}. A round that is running ends its engine at once, and no round or build starts, until `round.py resume`.")
    return 0


def command_resume(args):
    state = roundguard.State(guard_root(args))
    state.resume()
    print(f"kill switch lifted ({state.stop_file} removed)")
    return 0


def command_ack_panic(args):
    state = roundguard.State(guard_root(args))
    state.acknowledge(args.note or "")
    print(f"panic acknowledged ({state.ack_file}): the pre-flight stops warning about the panic reports up to now")
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
        sp.add_argument("--parallel", type=int, default=1, help="ignored above 1: the variants run one after the other, never more than one engine at a time")
        sp.add_argument("--priority", choices=("low", "normal"), default="low",
                        help="low (the default) keeps the run out of the way of whoever uses the computer")
        if name == "run":
            sp.add_argument("--wall", type=float, default=roundguard.WALL_SECONDS,
                            help="seconds for the whole round, all its variants; the engine is killed when they are over (default %(default)s)")
        sp.set_defaults(handler=handler)

    sp = sub.add_parser("build", help="incremental build of the engine in one persistent folder")
    sp.add_argument("--repo", default=str(ROOT))
    sp.add_argument("--worktree", default=str(ROOT.parent / "round-worktree"))
    sp.add_argument("--build-dir", default=str(ROOT.parent / "round-build"))
    sp.add_argument("--ref", help="the commit, branch or origin/<branch> to build")
    sp.add_argument("--jobs", type=int, help=f"compile jobs (default and highest {roundguard.MAX_BUILD_JOBS})")
    sp.add_argument("--allow-heavy", action="store_true", help=f"allow more than {roundguard.MAX_BUILD_JOBS} jobs: only when the computer is free to be used up")
    sp.add_argument("--cmake-arg", action="append", default=[], help="an argument of the first cmake configuration")
    sp.set_defaults(handler=command_build)

    sp = sub.add_parser("preflight", help="what the guards say about this computer now (starts nothing)")
    sp.add_argument("--out", help="the folder of the results (where the guards keep their files)")
    sp.set_defaults(handler=command_preflight)

    for name, handler, text in (("stop", command_stop, "set the kill switch: a round that runs ends its engine, none starts"),
                                ("resume", command_resume, "lift the kill switch"),
                                ("ack-panic", command_ack_panic, "say that the last panic has been looked at, so that rounds may start")):
        sp = sub.add_parser(name, help=text)
        sp.add_argument("--out", help="the folder of the results (where the guards keep their files)")
        if name != "resume":
            sp.add_argument("--note", help="a word for the file (who looked, or why)")
        sp.set_defaults(handler=handler)

    args = parser.parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())
