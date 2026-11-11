#!/usr/bin/env python3
"""Tests of round/round.py, the runner of the scripted checks: the settings merge, the reading of the log, the order of the
screenshots, the command line, and a whole run against a stand-in for the engine (a script that writes a log and a picture)."""
import importlib.util
import json
import os
import stat
import struct
import sys
import tempfile
import textwrap
import unittest
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("round_runner", HERE / "round" / "round.py")
round_runner = importlib.util.module_from_spec(spec)
sys.modules["round_runner"] = round_runner
spec.loader.exec_module(round_runner)


def tiny_png(path):
    """A 2x2 PNG, written by hand so that no imaging library is needed."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + b"\xff\x00\x00" * 2 for _ in range(2))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class Settings(unittest.TestCase):
    def test_parse_and_merge(self):
        base = "# comment\n[Video]\nresolution x = 1920\nvsync mode = 1\n\n[Camera]\nviewing distance = 7168\n"
        merged = round_runner.merge_settings(base, {"Video": {"resolution x": 1280, "window mode": 2}},
                                             {"Terrain": {"game lod debug": "nodes"}}, None)
        parsed = round_runner.parse_settings(merged)
        self.assertEqual(parsed["Video"], {"resolution x": "1280", "vsync mode": "1", "window mode": "2"})
        self.assertEqual(parsed["Camera"], {"viewing distance": "7168"})
        self.assertEqual(parsed["Terrain"], {"game lod debug": "nodes"})

    def test_later_layer_wins(self):
        merged = round_runner.merge_settings("", {"A": {"k": "1"}}, {"A": {"k": "2"}})
        self.assertEqual(round_runner.parse_settings(merged), {"A": {"k": "2"}})

    def test_merge_does_not_change_a_dict_base(self):
        base = {"A": {"k": "1"}}
        round_runner.merge_settings(base, {"A": {"k": "2"}})
        self.assertEqual(base, {"A": {"k": "1"}})


class Log(unittest.TestCase):
    TEXT = "\n".join([
        "[10:00:00.100 I] Starting",
        "[10:00:01.000 W] Cannot read texture 0x01234abc for ref 12",
        "[10:00:01.100 W] Cannot read texture 0x0abcdef0 for ref 99",
        "[10:00:01.200 E] Failed to open meshes/a.nif",
        "[10:00:02.000 I] ROUND\tbegin\treal=3.1",
        "[10:00:02.500 I] ROUND\tshot\te1\tn=1",
        "[10:00:03.000 W] ROUND\twarning that is a scenario line",
        "[10:00:04.000 I] ROUND\tshot\te2\tn=2",
        "[10:00:05.000 I] ROUND\tdone\tend\treal=5.0 steps=4",
    ])

    def test_round_lines(self):
        lines = round_runner.round_lines(self.TEXT)
        self.assertEqual(lines[0], "begin\treal=3.1")
        self.assertEqual(len(lines), 5)

    def test_problems_fold_numbers_and_skip_round_lines(self):
        problems = round_runner.log_problems(self.TEXT)
        self.assertEqual([(p[0], p[2]) for p in problems], [("W", 2), ("E", 1)])
        self.assertEqual(round_runner.count_levels(self.TEXT), {"W": 2, "E": 1})

    def test_problem_limit_is_per_level(self):
        text = "\n".join(f"[10:00:00.000 W] kind{letter} happened" for letter in "abcdefghijkl")
        self.assertEqual(len(round_runner.log_problems(text, limit=10)), 10)

    def test_shot_names(self):
        self.assertEqual(round_runner.shot_names(round_runner.round_lines(self.TEXT)), [(1, "e1"), (2, "e2")])


class Focus(unittest.TestCase):
    def test_parse_front_pid(self):
        self.assertEqual(round_runner.parse_front_pid("ASN:0x0-0x2b02b:", '"pid"=4242'), 4242)
        self.assertIsNone(round_runner.parse_front_pid("", '"pid"=4242'))
        self.assertIsNone(round_runner.parse_front_pid("ASN:0x0-0x2b02b:", "nothing"))

    def test_sampler_counts_front(self):
        answers = iter([1, 2, 7, None, 7])
        sampler = round_runner.FocusSampler(7, sample=lambda: next(answers, 1))
        sampler.stop_event.wait = lambda timeout: None  # no sleeping in the test
        calls = {"n": 0}

        def is_set():
            calls["n"] += 1
            return calls["n"] > 5
        sampler.stop_event.is_set = is_set
        sampler.run()
        self.assertEqual((sampler.samples, sampler.front, sampler.available), (4, 2, True))


class Commands(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="round-test-"))
        self.profile = {"engine": str(self.tmp / "bin" / "openfallout"),
                        "games": {"nv": {"data": [str(self.tmp / "nv" / "Data")], "content": ["FalloutNV.esm"]}},
                        "saves": {"nv": str(self.tmp / "saves")}}
        (self.tmp / "saves").mkdir()
        scenario = self.tmp / "s.lua"
        scenario.write_text("return function(r) end", encoding="ascii")
        self.round_file = self.tmp / "r.json"
        self.round_file.write_text(json.dumps({"name": "t", "scenario": "s.lua", "save": "latest",
                                               "variants": [{"name": "a"}, {"name": "b", "settings": {"X": {"y": "1"}}}]}))

    class Args:
        scenario = game = save = start = name = limit = None

    def test_newest_save(self):
        old, new = self.tmp / "saves" / "a.fos", self.tmp / "saves" / "b.fos"
        old.write_bytes(b"1")
        new.write_bytes(b"2")
        os.utime(old, (1, 1))
        self.assertEqual(round_runner.newest_save(self.tmp / "saves"), new)

    def test_newest_save_looks_in_the_folders_of_the_characters(self):
        # the engine keeps its saves as saves/<Character>/<slot>.omwsave
        old, new = self.tmp / "saves" / "Doc" / "a.omwsave", self.tmp / "saves" / "Sunny" / "b.omwsave"
        for path, stamp in ((old, 1), (new, 5)):
            path.parent.mkdir()
            path.write_bytes(b"x")
            os.utime(path, (stamp, stamp))
        self.assertEqual(round_runner.newest_save(self.tmp / "saves"), new)

    def test_relative_paths_become_absolute(self):
        # the engine runs inside the folder of its run, so a path left relative would be read from there
        cwd = Path.cwd()
        os.chdir(self.tmp)
        try:
            profile = dict(self.profile, engine="bin/openfallout", resources="res",
                           games={"nv": {"data": ["nv/Data"], "content": []}})
            config = round_runner.load_round(self.round_file, self.Args())
            command = round_runner.engine_command(profile, config, round_runner.absolute("out/run"), None)
        finally:
            os.chdir(cwd)
        self.assertEqual(command[0], str(self.tmp.resolve() / "bin" / "openfallout"))
        for flag in ("--resources", "--config", "--user-data"):
            self.assertTrue(Path(command[command.index(flag) + 1]).is_absolute(), flag)
        for index, word in enumerate(command):
            if word == "--data":
                self.assertTrue(Path(command[index + 1]).is_absolute(), command[index + 1])

    def test_command_is_headless_and_private(self):
        config = round_runner.load_round(self.round_file, self.Args())
        save = self.tmp / "saves" / "q.fos"
        save.write_bytes(b"x")
        command = round_runner.engine_command(self.profile, config, self.tmp / "run", save)
        for flag in ("--headless", "--no-grab", "--skip-menu"):
            self.assertIn(flag, command)
        self.assertEqual(command[command.index("--config") + 1], str(self.tmp / "run" / "config"))
        self.assertEqual(command[command.index("--user-data") + 1], str(self.tmp / "run" / "userdata"))
        self.assertEqual(command[command.index("--load-savegame") + 1], str(self.tmp / "run" / "userdata" / "saves" / "q.fos"))
        self.assertIn("round.omwscripts", command)
        self.assertLess(command.index("FalloutNV.esm"), command.index("round.omwscripts"))

    def test_start_instead_of_save(self):
        config = round_runner.load_round(self.round_file, self.Args())
        config["save"] = None
        config["start"] = "WastelandNV:-18,0"
        command = round_runner.engine_command(self.profile, config, self.tmp / "run", None)
        self.assertEqual(command[command.index("--start") + 1], "WastelandNV:-18,0")
        self.assertNotIn("--load-savegame", command)

    def test_prepare_run_copies_and_never_touches_the_original(self):
        config = round_runner.load_round(self.round_file, self.Args())
        config["resolution"], config["framerate"] = [800, 600], 20
        save = self.tmp / "saves" / "q.fos"
        save.write_bytes(b"x")
        run = self.tmp / "run"
        round_runner.prepare_run(self.profile, config, config["variants"][1], run, save)
        settings = round_runner.parse_settings((run / "config" / "settings.cfg").read_text())
        self.assertEqual(settings["Video"]["resolution x"], "800")
        self.assertEqual(settings["Video"]["framerate limit"], "20")
        self.assertEqual(settings["X"], {"y": "1"})
        self.assertEqual((run / "userdata" / "saves" / "q.fos").read_bytes(), b"x")
        self.assertTrue((run / "data" / "scripts" / "roundscenario.lua").exists())
        self.assertEqual(save.read_bytes(), b"x")

    def test_missing_scenario_is_an_error(self):
        self.round_file.write_text(json.dumps({"name": "t"}))
        with self.assertRaises(SystemExit):
            round_runner.load_round(self.round_file, self.Args())


class WholeRun(unittest.TestCase):
    """A run against a script that behaves like the engine: it reads --config and --user-data, writes the log there and
    one screenshot per `shot` line, and exits."""

    def make_engine(self, tmp, body):
        engine = tmp / "bin" / "openfallout"
        engine.parent.mkdir()
        engine.write_text("#!" + sys.executable + "\n" + textwrap.dedent(body), encoding="utf-8")
        engine.chmod(engine.stat().st_mode | stat.S_IXUSR)
        return engine

    ENGINE = """
        import os, sys, pathlib, struct, zlib
        args = sys.argv[1:]
        config = pathlib.Path(args[args.index("--config") + 1])
        userdata = pathlib.Path(args[args.index("--user-data") + 1])
        assert os.environ.get("OPENFALLOUT_HEADLESS") == "1" and "--headless" in args
        shots = userdata / "screenshots"
        shots.mkdir(parents=True, exist_ok=True)
        def png(path):
            def chunk(kind, data):
                body = kind + data
                return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
            raw = b"".join(b"\\x00" + b"\\xff\\x00\\x00" * 2 for _ in range(2))
            path.write_bytes(b"\\x89PNG\\r\\n\\x1a\\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 2, 0, 0, 0))
                             + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
        lines = ["[10:00:00.000 I] start", "[10:00:01.000 W] Cannot read 0x1234"]
        for n, name in enumerate(("first", "second"), 1):
            lines.append("[10:00:02.000 I] ROUND\\tshot\\t%s\\tn=%d" % (name, n))
            if n <= @WRITTEN@:
                png(shots / ("screenshot%03d.png" % (n - 1)))
        if @DONE@:
            lines.append("[10:00:03.000 I] ROUND\\tdone\\tend")
        (config / "openfallout.log").write_text("\\n".join(lines) + "\\n")
        sys.exit(@STATUS@)
    """

    def run_round(self, status, done=True, written=2):
        tmp = Path(tempfile.mkdtemp(prefix="round-run-"))
        engine = self.make_engine(tmp, self.ENGINE.replace("@STATUS@", str(status)).replace("@DONE@", str(done))
                                  .replace("@WRITTEN@", str(written)))
        (tmp / "s.lua").write_text("return function(r) end")
        profile = {"engine": str(engine), "resources": str(tmp),
                   "games": {"nv": {"data": [str(tmp)], "content": ["FalloutNV.esm"]}}, "out": str(tmp / "out")}
        (tmp / "profile.json").write_text(json.dumps(profile))
        (tmp / "r.json").write_text(json.dumps({"name": "fake", "scenario": "s.lua", "variants": [{"name": "one"}, {"name": "two"}]}))
        code = round_runner.main(["--profile", str(tmp / "profile.json"), "run", str(tmp / "r.json"), "--parallel", "2"])
        return tmp, code

    def test_passing_run(self):
        tmp, code = self.run_round(0)
        self.assertEqual(code, 0)
        (run,) = list((tmp / "out").iterdir())
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual([s["name"] for s in report["shots"]], ["first", "second"])
        self.assertTrue(all(s["file"] and Path(s["file"]).exists() for s in report["shots"]))
        self.assertEqual(report["warnings"], 1)
        self.assertIn("one", (run / "summary.md").read_text())
        self.assertIn("two", (run / "summary.md").read_text())
        self.assertTrue((run / "two" / "report.md").read_text().startswith("# Round fake / two: PASS"))

    def test_a_crash_after_the_scenario_ended_is_a_note(self):
        tmp, code = self.run_round(3)
        self.assertEqual(code, 0)
        (run,) = list((tmp / "out").iterdir())
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["exit_status"], 3)
        self.assertIn("after the scenario had finished", report["exit_note"])
        self.assertIn("after the scenario had finished", (run / "one" / "report.md").read_text())

    def test_a_screenshot_that_was_asked_for_and_never_written_fails_the_round(self):
        tmp, code = self.run_round(0, written=1)
        self.assertEqual(code, 1)
        (run,) = list((tmp / "out").iterdir())
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "FAIL")
        self.assertEqual(report["missing_shots"], ["second"])
        text = (run / "one" / "report.md").read_text()
        self.assertIn("MISSING, so the round fails: second", text)

    def test_an_engine_that_ends_before_the_scenario_fails(self):
        tmp, code = self.run_round(3, done=False)
        self.assertEqual(code, 1)
        (run,) = list((tmp / "out").iterdir())
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("before the scenario finished", report["exit_note"])


if __name__ == "__main__":
    unittest.main()
