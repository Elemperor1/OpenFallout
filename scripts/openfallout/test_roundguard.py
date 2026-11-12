#!/usr/bin/env python3
"""Tests of round/roundguard.py, the guards of a round on a computer somebody works at: the readers of `vm_stat`, `pmset` and `ps`, the
lock, the kill switch, the pre-flight, the WindowServer heartbeat and the evidence. A stand-in for the computer stands for the Mac."""
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "round"))
import roundguard  # noqa: E402

GIB = roundguard.GIB

VM_STAT = """Mach Virtual Memory Statistics: (page size of 16384 bytes)
Pages free:                               100000.
Pages active:                             400000.
Pages inactive:                           150000.
Pages speculative:                         10000.
Pages throttled:                               0.
"""


class FakeSystem(roundguard.System):
    """A computer whose readers give what the test says. The defaults are a healthy, idle Mac."""
    name = "fake Mac"
    strict = True
    settles = True
    heartbeat = True

    def __init__(self, **values):
        self.values = {"free": 8 * GIB, "swap": 0, "therm": [], "table": [(1, "/sbin/launchd")], "load": 1.0, "cpus": 8, "boot": 1000,
                       "reports": [], "pid": 100, "ping": True, "log": "runningboardd line"}
        self.values.update(values)

    def free_bytes(self): return self.values["free"]
    def swap_bytes(self): return self.values["swap"]
    def therm_problems(self): return self.values["therm"]
    def processes(self): return self.values["table"]
    def load(self): return self.values["load"]
    def cpus(self): return self.values["cpus"]
    def boot_time(self): return self.values["boot"]
    def reports(self): return self.values["reports"]
    def window_server_pid(self): return self.values["pid"]
    def ping(self): return self.values["ping"]
    def runningboardd_log(self): return self.values["log"]


class Parsers(unittest.TestCase):
    def test_vm_stat_counts_free_inactive_and_speculative_pages(self):
        self.assertEqual(roundguard.parse_vm_stat(VM_STAT), (100000 + 150000 + 10000) * 16384)
        self.assertIsNone(roundguard.parse_vm_stat("nothing"))
        self.assertIsNone(roundguard.parse_vm_stat(None))

    def test_swap_usage(self):
        text = "total = 2048.00M  used = 1217.25M  free = 830.75M  (encrypted)"
        self.assertEqual(roundguard.parse_swap_used(text), int(1217.25 * 1024 ** 2))
        self.assertEqual(roundguard.parse_swap_used("total = 3.00G  used = 1.50G  free = 1.50G"), int(1.5 * GIB))
        self.assertIsNone(roundguard.parse_swap_used("nope"))

    def test_thermal_state(self):
        clean = ("Note: No thermal warning level has been recorded\nNote: No performance warning level has been recorded\n"
                 "Note: No CPU power status has been recorded\n")
        self.assertEqual(roundguard.parse_therm(clean), [])
        self.assertEqual(roundguard.parse_therm("CPU_Scheduler_Limit\t= 100\nCPU_Speed_Limit \t= 100\n"), [])
        slowed = roundguard.parse_therm("CPU_Scheduler_Limit\t= 100\nCPU_Speed_Limit \t= 70\n")
        self.assertEqual(len(slowed), 1)
        self.assertIn("CPU_Speed_Limit is 70", slowed[0])
        self.assertTrue(roundguard.parse_therm("Thermal warning level: 2\n"))

    def test_process_table_and_the_busy_report(self):
        table = roundguard.parse_process_table(
            "    1 /sbin/launchd\n  501 /Applications/Xcode.app/Contents/Developer/usr/bin/clang -c a.c\n  502 ninja -j4\n"
            "  600 /Applications/OpenFallout.app/Contents/MacOS/openfallout --skip-menu\n  700 /Applications/GeForceNOW.app/Contents/MacOS/GeForceNOW\n"
            "  801 /Applications/Google Chrome.app/Contents/Frameworks/Google Chrome Helper (Renderer) --type=renderer\n"
            "  802 /Applications/Google Chrome.app/Contents/Frameworks/Google Chrome Helper --type=gpu-process\n"
            "  900 /usr/bin/clangd --background\n  901 python3 round.py run x.json\n")
        self.assertEqual(table[0], (1, "/sbin/launchd"))
        busy = roundguard.busy_report(table, own_pids=(901,))
        self.assertEqual(busy["builds"], ["clang (501)", "ninja (502)"])        # clangd is a language server, not a build
        self.assertEqual(busy["engines"], [600])
        self.assertTrue(busy["streaming"])
        self.assertEqual(busy["helpers"], 2)
        self.assertEqual(roundguard.program_name("/usr/bin/ccache cc -c x.c"), "ccache")
        self.assertEqual(roundguard.program_name(""), "")

    def test_boot_time_and_pid(self):
        self.assertEqual(roundguard.parse_boot_time("{ sec = 1762829685, usec = 123456 } Tue Nov 11 02:54:45 2026"), 1762829685)
        self.assertIsNone(roundguard.parse_boot_time("garbage"))
        self.assertEqual(roundguard.parse_pid("690\n"), 690)
        self.assertIsNone(roundguard.parse_pid(""))

    def test_crash_reports_that_count(self):
        day = 86400
        t = 100 * day
        files = [("/L/.WindowServer-2026-11-11-205038.ips", t + 5000), ("/L/WindowServer_2026.crash", 100),
                 ("/L/Retired/panic-full-2026-11-11.0002.panic", t + 5100), ("/L/ResetCounter-2026-11-11.diag", t + 5200),
                 ("/L/bluetoothd-1.ips", t + 5300), ("/L/old.panic", t - 40 * day)]
        # the boot at t + 1000: the WindowServer report after it counts, the one before the boot does not
        found = roundguard.crash_reports(files, boot_time=t + 1000, acknowledged=None, now=t + 6000)
        self.assertEqual(found["window_server"], [".WindowServer-2026-11-11-205038.ips"])
        # with no acknowledgement the panics of the last 30 days count: all but the very old one
        self.assertEqual(found["panics"], ["ResetCounter-2026-11-11.diag", "panic-full-2026-11-11.0002.panic"])
        # acknowledged after the panic and before the reset counter: only the reset counter is left
        found = roundguard.crash_reports(files, boot_time=t + 1000, acknowledged=t + 5150, now=t + 6000)
        self.assertEqual(found["panics"], ["ResetCounter-2026-11-11.diag"])
        # a month later, with no acknowledgement, they are all too old to count
        found = roundguard.crash_reports(files, boot_time=t + 1000, acknowledged=None, now=t + 31 * day)
        self.assertEqual(found["panics"], [])


class StateAndLock(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="guard-test-"))
        self.state = roundguard.State(self.tmp)

    def test_the_kill_switch(self):
        self.assertFalse(self.state.stopped())
        self.state.stop("Jacob asked")
        self.assertTrue(self.state.stopped())
        self.assertEqual(self.state.stop_file, self.tmp / "guard" / "STOP")
        self.state.resume()
        self.assertFalse(self.state.stopped())
        self.state.resume()                                    # lifting what is not set is fine

    def test_the_acknowledgement_of_a_panic(self):
        self.assertIsNone(self.state.acknowledged())
        self.state.acknowledge("looked at it with Jacob")
        self.assertAlmostEqual(self.state.acknowledged(), time.time(), delta=5)

    def test_the_rest_after_a_build_or_a_run(self):
        self.assertEqual(self.state.settle_remaining(now=1000), 0.0)
        self.state.heavy_ended("build", now=1000)
        self.assertEqual(self.state.settle_remaining(now=1010), 5.0)
        self.assertEqual(self.state.settle_remaining(now=1020), 0.0)
        self.state.heavy_file.write_text("not json")
        self.assertEqual(self.state.settle_remaining(now=1010), 0.0)

    def test_a_lock_that_is_a_link_is_refused_and_its_target_is_left_alone(self):
        target = self.tmp / "precious.txt"
        target.write_text("keep me", encoding="utf-8")
        self.state.lock_file.parent.mkdir(parents=True, exist_ok=True)
        self.state.lock_file.symlink_to(target)
        with self.assertRaises(roundguard.Refused) as refused:
            with roundguard.Lock(self.state.lock_file, "round x"):
                self.fail("a lock that is a link must be refused")
        self.assertIn("cannot be opened safely", refused.exception.reasons[0])
        self.assertEqual(target.read_text(encoding="utf-8"), "keep me")

    def test_the_kill_switch_and_the_acknowledgement_do_not_write_through_a_link(self):
        target = self.tmp / "precious.txt"
        target.write_text("keep me", encoding="utf-8")
        self.state.folder.mkdir(parents=True, exist_ok=True)
        self.state.stop_file.symlink_to(target)
        self.state.ack_file.symlink_to(target)
        with self.assertRaises(roundguard.Refused):
            self.state.stop("note")
        with self.assertRaises(roundguard.Refused):
            self.state.acknowledge("note")
        self.state.heavy_file.symlink_to(target)
        self.state.heavy_ended("build")                       # advice only: no failure, and nothing written
        self.assertEqual(target.read_text(encoding="utf-8"), "keep me")

    def test_a_guard_folder_that_is_a_link_is_refused_and_a_link_for_the_stop_file_counts_as_set(self):
        real = self.tmp / "elsewhere"
        real.mkdir()
        state = roundguard.State(self.tmp / "results")
        (self.tmp / "results").mkdir()
        state.folder.symlink_to(real)
        with self.assertRaises(roundguard.Refused):
            state.stop("note")
        with self.assertRaises(roundguard.Refused):
            with roundguard.Lock(state.lock_file, "round x"):
                self.fail("a folder that is a link must be refused")
        self.assertEqual(list(real.iterdir()), [])
        # a dangling link where the kill switch goes is a kill switch that is set
        other = roundguard.State(self.tmp / "other")
        other.folder.mkdir(parents=True)
        other.stop_file.symlink_to(self.tmp / "nowhere")
        self.assertTrue(other.stopped())

    def test_the_lock_is_held_by_one_at_a_time_and_says_by_whom(self):
        with roundguard.Lock(self.state.lock_file, "build"):
            with self.assertRaises(roundguard.Refused) as refused:
                with roundguard.Lock(self.state.lock_file, "round x"):
                    self.fail("the second lock must be refused")
            self.assertIn("build, pid", refused.exception.reasons[0])
            self.assertIn("never more than one heavy thing", refused.exception.reasons[0])
        # released: the next one gets it
        with roundguard.Lock(self.state.lock_file, "round x"):
            pass


class Preflight(unittest.TestCase):
    def setUp(self):
        self.state = roundguard.State(Path(tempfile.mkdtemp(prefix="guard-test-")))

    def run_preflight(self, system=None):
        return roundguard.preflight(system or FakeSystem(), self.state, now=6000, own_pids=(4242,))

    def test_a_healthy_idle_computer_has_nothing_in_the_way(self):
        refusals, warnings, facts = self.run_preflight()
        self.assertEqual((refusals, warnings), ([], []))
        self.assertEqual((facts["free_gb"], facts["swap_gb"], facts["thermal"], facts["window_server_pid"]), (8.0, 0.0, "clean", 100))
        self.assertIn("nothing in the way", roundguard.describe_preflight(refusals, warnings, facts))

    def test_a_stressed_computer_is_warned_about_not_refused(self):
        cases = {
            "memory": (FakeSystem(free=3 * GIB), "3.0 GB of memory"),
            "swap": (FakeSystem(swap=9 * GIB), "9.0 GB of swap"),
            "thermal": (FakeSystem(therm=["CPU_Speed_Limit is 70"]), "thermal pressure"),
            "load": (FakeSystem(load=8.0), "load average is 8.0 on 8 cores"),
            "build": (FakeSystem(table=[(7, "/usr/bin/clang -c a.c")]), "a build is running: clang (7)"),
            "engine": (FakeSystem(table=[(8, "/x/openfallout --skip-menu")]), "OpenFallout is running (pid 8)"),
            "streaming": (FakeSystem(table=[(9, "/Applications/GeForceNOW.app/GeForceNOW")]), "GeForce NOW"),
            "helpers": (FakeSystem(table=[(n, "Google Chrome Helper") for n in range(20, 40)]), "20 browser helper processes"),
            "window server crashed": (FakeSystem(reports=[("/L/.WindowServer-2026-11-11-205038.ips", 5000)]), "WindowServer crashed since the boot"),
            "panic": (FakeSystem(reports=[("/L/panic-full-1.panic", 5100)]), "has not been acknowledged"),
            "unreadable": (FakeSystem(free=None), "could not read the free memory"),
            "no crash reports": (FakeSystem(reports=None), "could not read the boot time or the crash reports"),
        }
        for name, (system, text) in cases.items():
            with self.subTest(name):
                refusals, warnings, _ = self.run_preflight(system)
                self.assertEqual(refusals, [])
                self.assertEqual(len(warnings), 1, warnings)
                self.assertIn(text, warnings[0])

    def test_the_two_things_that_refuse(self):
        self.state.stop()
        refusals, _, _ = self.run_preflight()
        self.assertIn("kill switch is set", refusals[0])
        self.state.resume()
        refusals, _, _ = self.run_preflight(FakeSystem(pid=None))
        self.assertIn("WindowServer is not running", refusals[0])
        # a computer with no window system to ask is not refused for lacking one
        refusals, warnings, _ = roundguard.preflight(roundguard.System(), self.state, now=6000)
        self.assertEqual((refusals, warnings), ([], []))

    def test_an_acknowledged_panic_is_no_longer_warned_about(self):
        import os
        reports = [("/L/panic-full-1.panic", 100), ("/L/.WindowServer-1.ips", 5000)]
        self.state.acknowledge("looked at it")
        os.utime(self.state.ack_file, (5500, 5500))
        _, warnings, _ = self.run_preflight(FakeSystem(reports=reports))
        self.assertEqual(len(warnings), 1)
        self.assertIn("WindowServer crashed", warnings[0])
        # after a restart the boot is newer than the crash report
        _, warnings, _ = self.run_preflight(FakeSystem(reports=reports, boot=5400))
        self.assertEqual(warnings, [])

    def test_the_test_call_to_the_window_system(self):
        self.assertTrue(roundguard.try_ping(lambda: True))
        self.assertFalse(roundguard.try_ping(lambda: False))
        self.assertFalse(roundguard.try_ping(lambda: 1 / 0))
        release = threading.Event()
        started = time.monotonic()
        self.assertFalse(roundguard.try_ping(lambda: release.wait(5), timeout=0.2))     # a call that blocks does not block the caller
        self.assertLess(time.monotonic() - started, 2)
        release.set()


class Clock:
    def __init__(self):
        self.now = 100.0

    def __call__(self):
        return self.now


class HeartbeatTests(unittest.TestCase):
    def make(self, ping, pids=(100,), clock=None):
        pid_answers = iter(pids)
        last = {"pid": pids[-1]}

        def pid_of():
            last["pid"] = next(pid_answers, last["pid"])
            return last["pid"]
        return roundguard.Heartbeat(ping, pid_of, clock=clock or Clock())

    def test_a_window_system_that_answers_is_no_problem(self):
        clock = Clock()
        beat = self.make(lambda: True, clock=clock)
        for _ in range(5):
            beat.poll_once()
            clock.now += 2
        self.assertIsNone(beat.problem())
        self.assertEqual((beat.answers, beat.same_pid(), beat.start_pid), (5, True, 100))

    def test_an_answer_that_is_more_than_5_s_late_is_the_problem(self):
        clock = Clock()
        beat = self.make(lambda: True, clock=clock)
        beat.poll_once()
        clock.now += 9.9
        self.assertIsNone(beat.problem())
        clock.now += 0.2
        self.assertIn("has not answered for 10 s", beat.problem())

    def test_a_call_that_is_stuck_is_the_problem_while_it_is_stuck(self):
        clock = Clock()
        beat = self.make(lambda: True, clock=clock)
        beat.poll_once()
        beat.busy_since = clock.now + 2                     # the next call began 2 s later and has not come back
        clock.now += 13
        self.assertIn("has not answered for 11 s", beat.problem())

    def test_a_window_system_that_never_answers_is_a_problem_after_15_s(self):
        clock = Clock()
        beat = self.make(lambda: False, clock=clock)
        beat.poll_once()
        clock.now += 14
        self.assertIsNone(beat.problem())
        clock.now += 2
        self.assertIn("not answered once in 16 s", beat.problem())

    def test_a_call_that_raises_did_not_answer(self):
        clock = Clock()
        beat = self.make(lambda: 1 / 0, clock=clock)
        beat.poll_once()
        self.assertEqual(beat.answers, 0)

    def test_another_window_server_process_is_the_problem(self):
        clock = Clock()
        beat = self.make(lambda: True, pids=(690, 690, 690, 28288), clock=clock)
        beat.poll_once()
        self.assertIsNone(beat.problem())
        clock.now += 10          # the pid is read again every 10 s
        beat.poll_once()
        self.assertIsNone(beat.problem())
        clock.now += 10
        beat.poll_once()
        self.assertIn("another process (pid 690, now 28288)", beat.problem())
        self.assertFalse(beat.same_pid())

    def test_a_window_server_that_is_gone(self):
        clock = Clock()
        beat = self.make(lambda: True, pids=(690, None), clock=clock)
        beat.poll_once()
        clock.now += 10
        beat.poll_once()
        self.assertEqual(beat.problem(), "WindowServer is gone")

    def test_the_pid_that_could_not_be_read_is_not_measured(self):
        beat = self.make(lambda: True, pids=(None,))
        beat.poll_once()
        self.assertIsNone(beat.same_pid())

    def test_the_thread_finds_a_call_that_never_returns(self):
        release = threading.Event()
        beat = roundguard.Heartbeat(lambda: release.wait(10), lambda: 5, interval=0.01, stall=0.2, first_answer=0.5, pid_interval=0.05)
        beat.start()
        try:
            deadline = time.monotonic() + 5
            while beat.problem() is None and time.monotonic() < deadline:
                time.sleep(0.02)
            self.assertIn("has not answered", beat.problem() or "")
        finally:
            release.set()
            beat.stop()


class RunGuardTests(unittest.TestCase):
    def setUp(self):
        self.state = roundguard.State(Path(tempfile.mkdtemp(prefix="guard-test-")))

    def test_the_kill_switch_ends_the_run_and_the_reason_stays(self):
        guard = roundguard.RunGuard(self.state)
        self.assertIsNone(guard.problem())
        self.state.stop()
        self.assertIn("kill switch was set", guard.problem())
        self.state.resume()
        self.assertIn("kill switch was set", guard.problem())           # once tripped, always tripped

    def test_the_wall_time_limit(self):
        clock = Clock()
        guard = roundguard.RunGuard(self.state, wall_seconds=60, clock=clock)
        clock.now += 59
        self.assertIsNone(guard.problem())
        clock.now += 2
        self.assertIn("wall-time limit", guard.problem())

    def test_the_heartbeat_is_asked_and_its_pids_reported(self):
        clock = Clock()
        beat = roundguard.Heartbeat(lambda: True, lambda: 690, clock=clock)
        beat.poll_once()
        guard = roundguard.RunGuard(self.state, beat, clock=clock)
        self.assertIsNone(guard.problem())
        self.assertEqual(guard.window_server(), {"start": 690, "end": 690, "same": True, "answers": 1, "slowest_answer": 0.0})
        self.assertIsNone(roundguard.RunGuard(self.state).window_server())


class Evidence(unittest.TestCase):
    def test_new_reports_and_the_log_of_runningboardd_are_kept_with_the_run(self):
        tmp = Path(tempfile.mkdtemp(prefix="guard-test-"))
        new, old, other = tmp / ".WindowServer-1.ips", tmp / "WindowServer-0.ips", tmp / "Chrome.ips"
        for path in (new, old, other):
            path.write_text(path.name)
        system = FakeSystem(reports=[(str(new), 5000), (str(old), 100), (str(other), 5000)])
        run_dir = tmp / "run"
        run_dir.mkdir()
        written = roundguard.collect_evidence(system, run_dir, since=1000)
        self.assertEqual(sorted(written), [".WindowServer-1.ips", "runningboardd-last-5m.log"])
        self.assertEqual((run_dir / "diagnostics" / ".WindowServer-1.ips").read_text(), ".WindowServer-1.ips")
        self.assertEqual((run_dir / "diagnostics" / "runningboardd-last-5m.log").read_text(), "runningboardd line")
        # nothing to keep: no folder
        quiet = tmp / "quiet"
        quiet.mkdir()
        self.assertEqual(roundguard.collect_evidence(FakeSystem(log=None), quiet, since=1000), [])
        self.assertFalse((quiet / "diagnostics").exists())


if __name__ == "__main__":
    unittest.main()
