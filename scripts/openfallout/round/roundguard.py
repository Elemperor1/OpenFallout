#!/usr/bin/env python3
"""Guards of a round on a computer that somebody works at (macOS first).

On 2026-11-12 a Mac panicked during a round: WindowServer was blocked for 40 s behind a stuck system daemon, the watchdog killed
it, its replacement never finished starting and `watchdogd` brought the machine down 120 s later. The engine (hidden, but still a
client of WindowServer) was one of the processes that were stuck; why the daemon stalled was not found. These are the guards that
follow from reading that incident (scripts/openfallout/round/README.md, "Guards"), kept apart from `round.py` so that each can be
tested with stand-ins for the computer:

- one lock for a build or a run (never two heavy things at once) and a pause after each, so that a build and an engine never
  overlap and a run does not start on a machine that has just been loaded;
- a pre-flight that reads the state of the computer (WindowServer crashed since boot, a panic nobody has looked at, little free
  memory or much swap, thermal pressure, a high load, a build or the game running, cloud-game streaming, many browser helpers) and
  WARNS about each; it refuses only when the kill switch is set or there is no window system to ask;
- a heartbeat that asks WindowServer for its window list every 2 s from one thread of the runner (no process is spawned) and
  ends the engine at once when the answer is more than 10 s late or WindowServer is another process than at the start (the
  stall of 2026-11-12 lasted 40 s; a busy machine answers in under a second);
- a kill-switch file (`STOP` in the folder of the results) and a wall-time limit for the whole round;
- evidence for next time: the log of `runningboardd` and the new crash reports, copied into the folder of the run.

Nothing here needs root, and nothing here runs without a round or a build asking for it.
"""
import ctypes
import json
import os
import re
import subprocess
import threading
import time
from pathlib import Path

GIB = 1024 ** 3
MIN_FREE_BYTES = 4 * GIB          # free + inactive + speculative memory
MAX_SWAP_BYTES = 8 * GIB
SETTLE_SECONDS = 15               # between a build or a run and the next one
VARIANT_GAP_SECONDS = 10          # between two engine starts of one round
MAX_BUILD_JOBS = 6
MAX_BROWSER_HELPERS = 12
HEARTBEAT_INTERVAL = 2.0
HEARTBEAT_STALL = 10.0
HEARTBEAT_FIRST_ANSWER = 15.0
PID_INTERVAL = 10.0
WALL_SECONDS = 3600               # the whole round, all variants
PANIC_LOOKBACK_DAYS = 30

BUILD_TOOLS = frozenset(["ninja", "make", "gmake", "clang", "clang++", "cc", "c++", "cc1", "cc1plus", "ccache", "ld"])
REPORT_DIRS = ("/Library/Logs/DiagnosticReports", "~/Library/Logs/DiagnosticReports")


class Refused(Exception):
    """The round or build must not start; `reasons` says why, each one a sentence."""

    def __init__(self, reasons):
        super().__init__("; ".join(reasons))
        self.reasons = list(reasons)


# ----------------------------------------------------------------------------------------------------------------------- parsers

def parse_vm_stat(text):
    """Bytes of memory that can be had at once from `vm_stat`: free + inactive + speculative pages. None when the text has no page size."""
    size = re.search(r"page size of (\d+) bytes", text or "")
    if not size:
        return None
    pages = 0
    found = False
    for name in ("free", "inactive", "speculative"):
        match = re.search(rf"(?im)^Pages {name}:\s*(\d+)\.?", text)
        if match:
            pages += int(match.group(1))
            found = True
    return pages * int(size.group(1)) if found else None


def parse_swap_used(text):
    """Bytes of swap in use from `sysctl -n vm.swapusage` (`total = 2048.00M  used = 1217.25M  free = 830.75M  (encrypted)`)."""
    match = re.search(r"used\s*=\s*([\d.]+)\s*([KMGT]?)", text or "")
    if not match:
        return None
    unit = {"": 1, "K": 1024, "M": 1024 ** 2, "G": 1024 ** 3, "T": 1024 ** 4}[match.group(2)]
    return int(float(match.group(1)) * unit)


def parse_therm(text):
    """The thermal problems that `pmset -g therm` reports: [sentence]. Nothing recorded (`No thermal warning level has been
    recorded`) is no problem; a CPU speed or scheduler limit below 100 or a warning level above 0 is."""
    problems = []
    for line in (text or "").splitlines():
        limit = re.match(r"\s*(CPU_Scheduler_Limit|CPU_Speed_Limit)\s*=\s*(\d+)", line)
        if limit and int(limit.group(2)) < 100:
            problems.append(f"{limit.group(1)} is {limit.group(2)} (the processor is being slowed down)")
        warning = re.search(r"(?i)(thermal|performance) warning level\D*(\d+)", line)
        if warning and "no " not in line.lower() and int(warning.group(2)) > 0:
            problems.append(f"{warning.group(1)} warning level {warning.group(2)}")
    return problems


def parse_process_table(text):
    """[(pid, args)] from `ps -axo pid=,args=`."""
    rows = []
    for line in (text or "").splitlines():
        match = re.match(r"\s*(\d+)\s+(.*)$", line)
        if match:
            rows.append((int(match.group(1)), match.group(2)))
    return rows


def program_name(args):
    """The name of the program of a command line of `ps` ("/usr/bin/clang -c x.c" gives "clang"). A path with a space in it is cut at the space; the
    names that matter here have none."""
    first = args.split(None, 1)[0] if args.strip() else ""
    return os.path.basename(first).lower()


def busy_report(table, own_pids=()):
    """What else is running that makes a round a bad idea: {builds: [names], engines: [pids], streaming: bool, helpers: count}."""
    builds, engines, helpers, streaming = [], [], 0, False
    for pid, args in table:
        if pid in own_pids:
            continue
        name = program_name(args)
        if name in BUILD_TOOLS:
            builds.append(f"{name} ({pid})")
        if name == "openfallout":
            engines.append(pid)
        if "GeForceNOW" in args or "GeForce NOW" in args:
            streaming = True
        if "Google Chrome Helper" in args or "Chromium Helper" in args:
            helpers += 1
    return {"builds": builds, "engines": engines, "streaming": streaming, "helpers": helpers}


def parse_boot_time(text):
    """Seconds since the epoch of the boot, from `sysctl -n kern.boottime` (`{ sec = 1762829685, usec = 1 } Wed Nov 11 ...`)."""
    match = re.search(r"sec\s*=\s*(\d+)", text or "")
    return int(match.group(1)) if match else None


def parse_pid(text):
    """The first number of the output of `pgrep`."""
    match = re.search(r"\d+", text or "")
    return int(match.group()) if match else None


def crash_reports(files, boot_time, acknowledged, now):
    """Splits the crash reports ([(name, modified)]) into the ones that count: {window_server: [names since boot], panics: [names newer
    than the acknowledgement]}. A panic report (`*.panic`) or a reset counter (`ResetCounter*`) counts when it is newer than
    `acknowledged` (the time somebody said they had looked at the last one; with none, the reports of the last 30 days); a crash report of
    WindowServer (`WindowServer*.ips` or `.crash`) when it is newer than the boot."""
    horizon = acknowledged if acknowledged is not None else now - PANIC_LOOKBACK_DAYS * 86400
    window_server, panics = [], []
    for name, modified in files:
        base = os.path.basename(name)
        if re.match(r"WindowServer.*\.(ips|crash)$", base, re.IGNORECASE) or re.match(r"\.WindowServer.*\.(ips|crash)$", base):
            if boot_time is not None and modified >= boot_time:
                window_server.append(base)
        elif base.endswith(".panic") or base.startswith("ResetCounter"):
            if modified > horizon:
                panics.append(base)
    return {"window_server": sorted(set(window_server)), "panics": sorted(set(panics))}


# ------------------------------------------------------------------------------------------------------------------ the computer

class System:
    """What the guards read from the computer. The base class knows nothing (every reader gives None) and is what a computer other
    than a Mac gets: only the lock, the kill switch and the time limit guard it. `strict` says that a reader that gives None is a
    reason to refuse (on a Mac that cannot be read, nothing can be said about its state)."""
    name = "this computer"
    strict = False
    settles = False
    heartbeat = False

    def free_bytes(self): return None
    def swap_bytes(self): return None
    def therm_problems(self): return None
    def processes(self): return None
    def load(self): return None
    def cpus(self): return os.cpu_count() or 1
    def boot_time(self): return None
    def reports(self): return None
    def window_server_pid(self): return None
    def ping(self): return None
    def runningboardd_log(self): return None


def run_text(command, timeout=10):
    """The standard output of a command, or None when it cannot be run or fails."""
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout, check=False)
    except (OSError, subprocess.SubprocessError):
        return None
    return result.stdout if result.returncode == 0 else None


def core_graphics_ping():
    """A function that asks WindowServer for the windows on screen (`CGWindowListCopyWindowInfo`) and gives True when it answers. It
    blocks as long as WindowServer does, which is what the heartbeat measures. None where CoreGraphics cannot be loaded."""
    try:
        graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
        foundation = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
        graphics.CGWindowListCopyWindowInfo.restype = ctypes.c_void_p
        graphics.CGWindowListCopyWindowInfo.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
        foundation.CFRelease.argtypes = [ctypes.c_void_p]
        foundation.CFRelease.restype = None
    except (OSError, AttributeError):
        return None

    def ping():
        windows = graphics.CGWindowListCopyWindowInfo(1, 0)       # kCGWindowListOptionOnScreenOnly, kCGNullWindowID
        if not windows:
            return False
        foundation.CFRelease(windows)
        return True
    return ping


class MacSystem(System):
    name = "this Mac"
    strict = True
    settles = True
    heartbeat = True

    def __init__(self):
        self._ping = core_graphics_ping()

    def free_bytes(self):
        return parse_vm_stat(run_text(["vm_stat"]))

    def swap_bytes(self):
        return parse_swap_used(run_text(["sysctl", "-n", "vm.swapusage"]))

    def therm_problems(self):
        text = run_text(["pmset", "-g", "therm"])
        return None if text is None else parse_therm(text)

    def processes(self):
        text = run_text(["ps", "-axo", "pid=,args="], timeout=20)
        return None if text is None else parse_process_table(text)

    def load(self):
        try:
            return os.getloadavg()[0]
        except OSError:
            return None

    def boot_time(self):
        return parse_boot_time(run_text(["sysctl", "-n", "kern.boottime"]))

    def reports(self):
        files, seen = [], False
        for folder in REPORT_DIRS:
            root = Path(folder).expanduser()
            if not root.is_dir():
                continue
            seen = True
            for pattern in ("*", "Retired/*"):
                for path in root.glob(pattern):
                    try:
                        if path.is_file():
                            files.append((str(path), path.stat().st_mtime))
                    except OSError:
                        pass
        return files if seen else None

    def window_server_pid(self):
        return parse_pid(run_text(["pgrep", "-x", "WindowServer"], timeout=5))

    def ping(self):
        return None if self._ping is None else self._ping()

    def runningboardd_log(self):
        return run_text(["log", "show", "--last", "5m", "--style", "compact", "--predicate", 'process == "runningboardd"'], timeout=45)


def default_system(is_mac):
    return MacSystem() if is_mac else System()


# ----------------------------------------------------------------------------------------------------------------------- state

class State:
    """The folder where the guards keep their files, `guard/` in the folder of the results of the rounds: `round.lock`, `STOP` (the kill
    switch), `panic-ack` (when somebody said they had looked at the last panic) and `last-heavy.json` (when the last build or run ended)."""

    def __init__(self, results_folder):
        self.folder = Path(results_folder) / "guard"

    @property
    def stop_file(self): return self.folder / "STOP"
    @property
    def lock_file(self): return self.folder / "round.lock"
    @property
    def ack_file(self): return self.folder / "panic-ack"
    @property
    def heavy_file(self): return self.folder / "last-heavy.json"

    def stopped(self):
        return self.stop_file.exists()

    def _write(self, path, text):
        """Writes one of the guards' files in a folder that is the user's own and not through a link that somebody else planted (the
        folder may be in a shared temporary folder): a refusal, not a write, when it is not so."""
        self.folder.mkdir(parents=True, exist_ok=True)
        if os.stat(self.folder).st_uid != os.getuid():
            raise Refused([f"the folder of the guards ({self.folder}) belongs to another user"])
        try:
            descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_NOFOLLOW, 0o600)
        except OSError as error:
            raise Refused([f"{path} cannot be written safely: {error.strerror}"])
        with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
            handle.write(text)

    def stop(self, note=""):
        self._write(self.stop_file, note or "stopped\n")

    def resume(self):
        try:
            self.stop_file.unlink()
        except FileNotFoundError:
            pass

    def acknowledged(self):
        try:
            return self.ack_file.stat().st_mtime
        except OSError:
            return None

    def acknowledge(self, note=""):
        self._write(self.ack_file, (note or "looked at") + "\n")

    def heavy_ended(self, kind, now=None):
        # (the rest after a heavy thing is advice: a file that cannot be written safely is no reason to fail what just ended)
        try:
            self._write(self.heavy_file, json.dumps({"kind": kind, "ended": time.time() if now is None else now}))
        except Refused:
            pass

    def settle_remaining(self, now=None, seconds=SETTLE_SECONDS):
        """Seconds still to wait since the last build or run ended (0 when none, or long ago)."""
        try:
            ended = json.loads(self.heavy_file.read_text(encoding="utf-8"))["ended"]
        except (OSError, ValueError, KeyError, TypeError):
            return 0.0
        return max(0.0, ended + seconds - (time.time() if now is None else now))


class Lock:
    """One lock for a build or a run: the second to ask is refused with the name of the holder."""

    def __init__(self, path, kind):
        self.path, self.kind, self.handle = Path(path), kind, None

    def __enter__(self):
        import fcntl
        self.path.parent.mkdir(parents=True, exist_ok=True)
        # The folder is the user's own (a shared temporary folder is not a place to trust: somebody else may have made it), and the
        # lock is not a link that somebody else planted (a link would have the runner truncate and write into its target)
        if os.stat(self.path.parent).st_uid != os.getuid():
            raise Refused([f"the folder of the guards ({self.path.parent}) belongs to another user"])
        try:
            descriptor = os.open(self.path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        except OSError as error:
            raise Refused([f"the lock file ({self.path}) cannot be opened safely: {error.strerror}"])
        handle = os.fdopen(descriptor, "a+", encoding="utf-8")
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            handle.seek(0)
            holder = handle.read().strip() or "somebody"
            handle.close()
            raise Refused([f"another build or round holds the lock ({holder}); there is never more than one heavy thing at a time"])
        handle.seek(0)
        handle.truncate()
        handle.write(f"{self.kind}, pid {os.getpid()}, since {time.strftime('%H:%M:%S')}")
        handle.flush()
        self.handle = handle
        return self

    def __exit__(self, *exc):
        if self.handle is not None:
            try:
                self.handle.seek(0)
                self.handle.truncate()
            finally:
                self.handle.close()                      # closing releases the flock
                self.handle = None
        return False


# -------------------------------------------------------------------------------------------------------------------- pre-flight

def preflight(system, state, now=None, own_pids=()):
    """Looks at the computer before a build or a run. Gives (refusals, warnings, facts). A refusal is a state in which a round cannot do
    its work or must not be started at all: the kill switch is set, or there is no window system to ask. Everything else that could make
    a round a burden (little memory, much swap, thermal pressure, a high load, a build or the game running, cloud-game streaming, many
    browser helpers, a crash of WindowServer since the boot, a panic nobody has acknowledged) is a warning: it is printed and kept in the
    summary, and the round goes on, because the heartbeat ends the engine when the window system really stops answering."""
    now = time.time() if now is None else now
    refusals, warnings, facts = [], [], {}

    def unreadable(what):
        # On a computer that has no window system to protect what cannot be read is simply not checked
        if system.strict:
            warnings.append(f"could not read {what}")

    if state.stopped():
        refusals.append(f"the kill switch is set ({state.stop_file}: `round.py resume` lifts it)")

    free = system.free_bytes()
    if free is None:
        unreadable("the free memory (`vm_stat`)")
    else:
        facts["free_gb"] = round(free / GIB, 1)
        if free < MIN_FREE_BYTES:
            warnings.append(f"only {free / GIB:.1f} GB of memory is free or inactive (under {MIN_FREE_BYTES // GIB} GB)")
    swap = system.swap_bytes()
    if swap is None:
        unreadable("the swap in use (`sysctl vm.swapusage`)")
    else:
        facts["swap_gb"] = round(swap / GIB, 1)
        if swap > MAX_SWAP_BYTES:
            warnings.append(f"{swap / GIB:.1f} GB of swap is in use (over {MAX_SWAP_BYTES // GIB} GB)")
    therm = system.therm_problems()
    if therm is None:
        unreadable("the thermal state (`pmset -g therm`)")
    else:
        facts["thermal"] = "clean" if not therm else "; ".join(therm)
        warnings.extend(f"thermal pressure: {problem}" for problem in therm)
    load = system.load()
    if load is None:
        unreadable("the load average")
    else:
        cpus = system.cpus()
        facts["load"] = f"{load:.1f} of {cpus} cores"
        if load >= cpus:
            warnings.append(f"the load average is {load:.1f} on {cpus} cores")

    table = system.processes()
    if table is None:
        unreadable("the list of processes (`ps`)")
    else:
        busy = busy_report(table, own_pids or (os.getpid(),))
        facts["helpers"] = busy["helpers"]
        if busy["builds"]:
            warnings.append("a build is running: " + ", ".join(busy["builds"][:4]))
        if busy["engines"]:
            warnings.append("OpenFallout is running (pid " + ", ".join(str(p) for p in busy["engines"][:3]) + ")")
        if busy["streaming"]:
            warnings.append("a cloud-game stream (GeForce NOW) is running")
        if busy["helpers"] > MAX_BROWSER_HELPERS:
            warnings.append(f"{busy['helpers']} browser helper processes are running (over {MAX_BROWSER_HELPERS})")

    boot = system.boot_time()
    files = system.reports()
    if boot is None or files is None:
        unreadable("the boot time or the crash reports")
    else:
        found = crash_reports(files, boot, state.acknowledged(), now)
        if found["window_server"]:
            warnings.append("WindowServer crashed since the boot (" + ", ".join(found["window_server"][:3]) + ")")
        if found["panics"]:
            warnings.append("a panic or reset report has not been acknowledged (" + ", ".join(found["panics"][:3]) + "; `round.py ack-panic` "
                            "after it has been looked at)")
    pid = system.window_server_pid()
    facts["window_server_pid"] = pid
    if pid is None and system.heartbeat:
        refusals.append("WindowServer is not running (or its process cannot be found), so there is nothing to ask")
    return refusals, warnings, facts


def try_ping(ping, timeout=HEARTBEAT_STALL):
    """True when `ping` answers True within `timeout` seconds (it runs in a thread: a call that blocks does not block the caller)."""
    answer = []

    def call():
        try:
            answer.append(bool(ping()))
        except Exception:                                # a ping that raises did not answer
            answer.append(False)
    thread = threading.Thread(target=call, daemon=True)
    thread.start()
    thread.join(timeout)
    return bool(answer and answer[0])


def describe_preflight(refusals, warnings, facts):
    lines = []
    if facts:
        lines.append("pre-flight read: " + ", ".join(f"{key} {value}" for key, value in facts.items()))
    lines.extend(f"pre-flight WARNING: {warning}" for warning in warnings)
    if refusals:
        lines.append("pre-flight REFUSED:")
        lines.extend(f"  - {reason}" for reason in refusals)
    else:
        lines.append("pre-flight: nothing in the way" + (" (with the warnings above)" if warnings else ""))
    return "\n".join(lines)


# ---------------------------------------------------------------------------------------------------------------------- heartbeat

class Heartbeat(threading.Thread):
    """Asks WindowServer for its window list every 2 s from this one thread, and every 10 s which process WindowServer is. `problem()`
    says why the engine must be ended: the answer is more than 10 s late (or has never come), or WindowServer is not the process it was.
    It never spawns a process for the window list; the pid is one `pgrep` every 10 s."""

    def __init__(self, ping, pid_of, clock=time.monotonic, interval=HEARTBEAT_INTERVAL, stall=HEARTBEAT_STALL,
                 first_answer=HEARTBEAT_FIRST_ANSWER, pid_interval=PID_INTERVAL):
        super().__init__(daemon=True)
        self.ping, self.pid_of, self.clock = ping, pid_of, clock
        self.interval, self.stall, self.first_answer, self.pid_interval = interval, stall, first_answer, pid_interval
        self.stop_event = threading.Event()
        self.started_at = clock()
        self.last_ok = None
        self.busy_since = None
        self.last_pid_check = None
        self.start_pid = pid_of()
        self.current_pid = self.start_pid
        self.answers = 0
        self.slowest = 0.0

    def poll_once(self):
        """One round of the thread: the window list, and the pid when it is due. Blocks as long as WindowServer does."""
        began = self.clock()
        self.busy_since = began
        try:
            ok = bool(self.ping())
        except Exception:
            ok = False
        finished = self.clock()
        self.busy_since = None
        if ok:
            self.last_ok = finished
            self.answers += 1
            self.slowest = max(self.slowest, finished - began)
        if self.last_pid_check is None or finished - self.last_pid_check >= self.pid_interval:
            self.last_pid_check = finished
            self.current_pid = self.pid_of()

    def run(self):
        while not self.stop_event.is_set():
            self.poll_once()
            self.stop_event.wait(self.interval)

    def problem(self):
        now = self.clock()
        if self.busy_since is not None and now - self.busy_since > self.stall:
            return f"WindowServer has not answered for {now - self.busy_since:.0f} s"
        reference = self.last_ok if self.last_ok is not None else self.started_at
        limit = self.stall if self.last_ok is not None else self.first_answer
        if now - reference > limit and (self.busy_since is None):
            return (f"WindowServer has not answered for {now - reference:.0f} s" if self.last_ok is not None
                    else f"WindowServer has not answered once in {now - reference:.0f} s")
        if self.start_pid is not None and self.current_pid != self.start_pid:
            return ("WindowServer is gone" if self.current_pid is None else
                    f"WindowServer is another process (pid {self.start_pid}, now {self.current_pid})")
        return None

    def same_pid(self):
        """True, False, or None when the pid was never read."""
        if self.start_pid is None:
            return None
        return self.current_pid == self.start_pid

    def stop(self):
        self.stop_event.set()
        self.join(timeout=1)


class RunGuard:
    """What the run loop asks every half second: the first reason to end the engine, or None."""

    def __init__(self, state, heartbeat=None, wall_seconds=WALL_SECONDS, clock=time.monotonic):
        self.state, self.heartbeat, self.clock = state, heartbeat, clock
        self.deadline = clock() + wall_seconds
        self.tripped = None

    def problem(self):
        if self.tripped:
            return self.tripped
        reason = None
        if self.state.stopped():
            reason = f"the kill switch was set ({self.state.stop_file})"
        elif self.clock() >= self.deadline:
            reason = "the wall-time limit of the round is over"
        elif self.heartbeat is not None:
            reason = self.heartbeat.problem()
        if reason:
            self.tripped = reason
        return reason

    def window_server(self):
        """{start, end, same} of the pid of WindowServer over the round (None when it was not measured)."""
        if self.heartbeat is None:
            return None
        return {"start": self.heartbeat.start_pid, "end": self.heartbeat.current_pid, "same": self.heartbeat.same_pid(),
                "answers": self.heartbeat.answers, "slowest_answer": round(self.heartbeat.slowest, 2)}


# --------------------------------------------------------------------------------------------------------------------- evidence

def collect_evidence(system, run_dir, since, boot_time=None):
    """Copies what an investigation would want into `<run>/diagnostics/`: the crash reports and panics newer than `since` (epoch seconds) and
    the last five minutes of the log of `runningboardd`. Returns the names written."""
    written = []
    target = Path(run_dir) / "diagnostics"
    try:
        files = system.reports() or []
        for name, modified in files:
            base = os.path.basename(name)
            if modified >= since and re.match(r"\.?(WindowServer|bluetoothd|runningboardd).*\.(ips|crash)$|.*\.panic$|ResetCounter", base):
                target.mkdir(parents=True, exist_ok=True)
                (target / base).write_bytes(Path(name).read_bytes())
                written.append(base)
        text = system.runningboardd_log()
        if text:
            target.mkdir(parents=True, exist_ok=True)
            (target / "runningboardd-last-5m.log").write_text(text, encoding="utf-8")
            written.append("runningboardd-last-5m.log")
    except OSError:
        pass
    return written
