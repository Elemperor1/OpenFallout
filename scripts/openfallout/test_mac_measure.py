"""Tests of mac_measure.py: the command lines it builds, what it reads from a run, and the comparison of two runs."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mac_measure  # noqa: E402


class SceneCommandTests(unittest.TestCase):
    def test_new_vegas_interior_starts_the_cell_with_the_probe_in_a_private_folder(self):
        command = mac_measure.engine_command(
            engine="/build/openfallout", resources="/build/resources", scene=mac_measure.SCENES["nv-interior"],
            game_data=["/games/nv/Data"], run_dir=Path("/runs/r1"), probe_dir=Path("/repo/perfprobe"))
        self.assertEqual(command, [
            "/build/openfallout", "--resources", "/build/resources",
            "--config", "/runs/r1/config", "--replace", "config", "--user-data", "/runs/r1/userdata",
            "--data-local", "/runs/r1/userdata/data",
            "--data", "/games/nv/Data", "--data", "/repo/perfprobe", "--data", "/runs/r1/data",
            "--content", "FalloutNV.esm", "--content", "perfprobe.omwscripts",
            "--skip-menu", "--no-grab", "--start", "GSDocMitchellHouse",
        ])


class CaptureCommandTests(unittest.TestCase):
    def test_time_profile_attaches_to_the_running_engine_for_the_measured_phases(self):
        self.assertEqual(mac_measure.xctrace_command("time", pid=4242, trace=Path("/runs/r1/time.trace"), seconds=70), [
            "xcrun", "xctrace", "record", "--template", "Time Profiler", "--attach", "4242",
            "--time-limit", "70s", "--output", "/runs/r1/time.trace", "--no-prompt",
        ])

    def test_each_capture_kind_names_its_instruments_template(self):
        templates = {kind: mac_measure.xctrace_command(kind, 1, Path("t.trace"), 5)[4]
                     for kind in ("time", "alloc", "game", "metal")}
        self.assertEqual(templates, {"time": "Time Profiler", "alloc": "Allocations",
                                     "game": "Game Performance", "metal": "Metal System Trace"})


PROBE = "L@0x1[scripts/perfprobe.lua]:\tPERF\t"
LOG = f"""[23:59:58.000 I] OpenFallout version 0.52.0
[23:59:58.100 I] Loading content file FalloutNV.esm
[23:59:59.500 I] Starting a new game
[23:59:59.600 I] Loading cell GSDocMitchellHouse
[00:00:01.750 I] {PROBE}begin\tphases=3
[00:00:01.750 I] {PROBE}start\tsettle\tcell=FormId:0x1 world=nil grid=- pos=1,2,3
[00:00:21.750 I] {PROBE}phase\tsettle\tsecs=20.0\tframes=900 mean=22.2 p50=20.0 p95=40.0 p99=80.0 max=300.0 over33=90 over100=3\tfps=45.0\tcell=x
[00:00:51.750 I] {PROBE}phase\tstand\tsecs=30.0\tframes=3000 mean=10.0 p50=9.5 p95=12.5 p99=20.0 max=40.0 over33=2 over100=0\tfps=100.0\tcell=x
[00:01:31.750 I] {PROBE}phase\tturn\tsecs=40.0\tframes=3200 mean=12.5 p50=11.0 p95=18.0 p99=30.0 max=90.0 over33=8 over100=0\tfps=80.0\tcell=x
[00:01:31.760 I] {PROBE}done\thitches=3\tcell=x
"""


class LogTests(unittest.TestCase):
    def test_reads_frame_times_cell_load_and_start_up_across_midnight(self):
        result = mac_measure.read_log(LOG)
        self.assertEqual(result["version"], "OpenFallout version 0.52.0")
        self.assertAlmostEqual(result["cell_load_s"], 2.25)
        self.assertAlmostEqual(result["startup_s"], 3.75)
        self.assertTrue(result["done"])
        self.assertEqual(result["phases"]["stand"], {"secs": 30.0, "frames": 3000, "mean": 10.0, "p50": 9.5, "p95": 12.5,
                                                     "p99": 20.0, "max": 40.0, "over33": 2, "over100": 0, "fps": 100.0})
        self.assertEqual(result["phases"]["turn"]["p95"], 18.0)

    def test_a_run_that_never_reached_the_scene_has_no_cell_load(self):
        result = mac_measure.read_log("[10:00:00.000 I] Starting a new game\n")
        self.assertIsNone(result["cell_load_s"])
        self.assertFalse(result["done"])
        self.assertEqual(result["phases"], {})


FOOTPRINT = """======================================================================
openfallout [4242]: 64-bit    Footprint: 1803 MB (16384 bytes per page)
======================================================================

Auxiliary data:
    phys_footprint: 1803 MB
    phys_footprint_peak: 2.1 GB
"""


class FootprintTests(unittest.TestCase):
    def test_reads_the_footprint_and_its_peak_in_mib(self):
        self.assertEqual(mac_measure.read_footprint(FOOTPRINT), {"footprint_mib": 1803.0, "peak_mib": 2150.4})

    def test_gives_nothing_for_a_process_that_is_gone(self):
        self.assertIsNone(mac_measure.read_footprint("footprint: no process matching 4242\n"))


def result(scene, stand_p50, cell_load, peak, **outcome):
    return {"scene": scene, "cell_load_s": cell_load, "memory": {"peak_mib": peak},
            "done": True, "stopped": None, "exit_status": 0, **outcome,
            "phases": {"stand": {"p50": stand_p50, "p95": stand_p50 * 2}, "turn": {"p50": 10.0, "p95": 20.0}}}


class CompareTests(unittest.TestCase):
    def test_compares_the_medians_of_each_scene_and_metric(self):
        before = [result("nv-busy", 10.0, 2.0, 2000.0), result("nv-busy", 12.0, 3.0, 2100.0),
                  result("nv-busy", 50.0, 2.5, 2050.0)]
        after = [result("nv-busy", 9.0, 2.5, 2050.0)]
        rows = {(row[0], row[1]): row[2:] for row in mac_measure.compare(before, after)}
        self.assertEqual(rows[("nv-busy", "stand p50 ms")], (12.0, 9.0, -25.0))
        self.assertEqual(rows[("nv-busy", "stand p95 ms")], (24.0, 18.0, -25.0))
        self.assertEqual(rows[("nv-busy", "cell load s")], (2.5, 2.5, 0.0))
        self.assertEqual(rows[("nv-busy", "peak footprint MiB")], (2050.0, 2050.0, 0.0))

    def test_runs_that_did_not_finish_cleanly_are_left_out_of_the_medians(self):
        good = result("nv-busy", 10.0, 2.0, 2000.0)
        failed = [result("nv-busy", 90.0, 9.0, 9000.0, stopped="time limit 300 s"),
                  result("nv-busy", 90.0, 9.0, 9000.0, exit_status=-10),
                  result("nv-busy", 90.0, 9.0, 9000.0, done=False)]
        rows = {(row[0], row[1]): row[2:] for row in mac_measure.compare([good] + failed, failed)}
        self.assertEqual(rows[("nv-busy", "stand p50 ms")], (10.0, None, None))
        self.assertEqual(rows[("nv-busy", "cell load s")], (2.0, None, None))

    def test_names_the_runs_it_left_out_and_why(self):
        failed = [result("nv-busy", 90.0, 9.0, 9000.0, stopped="time limit 300 s", run="nv-busy-2"),
                  result("fo3-busy", 90.0, 9.0, 9000.0, exit_status=-10),
                  result("fo3-interior", 90.0, 9.0, 9000.0, done=False)]
        self.assertEqual(mac_measure.left_out([result("nv-busy", 10.0, 2.0, 2000.0)] + failed),
                         ["nv-busy-2: stopped (time limit 300 s)", "fo3-busy: exit status -10",
                          "fo3-interior: the probe did not finish"])

    def test_a_scene_measured_on_one_side_only_shows_a_gap(self):
        rows = mac_measure.compare([result("fo3-busy", 10.0, 2.0, 2000.0)], [])
        self.assertIn(("fo3-busy", "stand p50 ms", 10.0, None, None), rows)


if __name__ == "__main__":
    unittest.main()
