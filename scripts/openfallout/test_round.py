#!/usr/bin/env python3
"""Tests of round/round.py, the runner of the scripted checks: the settings merge, the reading of the log, the order of the
screenshots, the command line, and a whole run against a stand-in for the engine (a script that writes a log and a picture)."""
import contextlib
import functools
import importlib.util
import io
import json
import os
import stat
import struct
import sys
import tempfile
import textwrap
import threading
import time
import unittest
import zlib
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("round_runner", HERE / "round" / "round.py")
round_runner = importlib.util.module_from_spec(spec)
sys.modules["round_runner"] = round_runner
spec.loader.exec_module(round_runner)


def runs_of(tmp):
    """The folders of the runs under `<tmp>/out` (the guards keep their own files in `guard/` next to them)."""
    return [path for path in (tmp / "out").iterdir() if path.name != "guard"]


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
        # the block that `lsappinfo info -only pid` prints on newer macOS versions (the filter is ignored there)
        block = ('"Google Chrome" ASN:0x0-0x1e01e:\n    bundle path="/Applications/Google Chrome.app"\n'
                 '    pid = 4242 type="Foreground" flavor=3 Version="1" fileType="APPL" creator="rimZ" arch=arm64\n'
                 '    parentPID = 1\n    parentASN="Dock" ASN:0x0-0x1c01c:\n')
        self.assertEqual(round_runner.parse_front_pid("ASN:0x0-0x1e01e:", block), 4242)
        self.assertEqual(round_runner.parse_front_pid("ASN:0x0-0x1e01e:", "    parentPID = 1\n    pid = 9\n"), 9)
        self.assertIsNone(round_runner.parse_front_pid("ASN:0x0-0x1e01e:", "    parentPID = 1\n"))
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

    @staticmethod
    def fake_save(path, *content_files):
        """A file with the header of a save that lists these content files."""
        path.parent.mkdir(parents=True, exist_ok=True)
        depe = b"".join(b"DEPE" + struct.pack("<I", len(name) + 1) + name.encode() + b"\0" for name in content_files)
        path.write_bytes(b"SAVE" + bytes(12) + b"PLNA" + struct.pack("<I", 4) + b"Doc\0" + depe + b"SCRN" + bytes(8))

    def test_latest_save_is_the_newest_of_the_game(self):
        # one folder holds the saves of both games (the engine's user data): `--game nv` must not take a newer Fallout 3 save
        nv = self.tmp / "saves" / "Doc" / "nv.omwsave"
        fo3 = self.tmp / "saves" / "Lone" / "fo3.omwsave"
        ttw = self.tmp / "saves" / "Courier" / "ttw.omwsave"
        self.fake_save(nv, "FalloutNV.esm", "DeadMoney.esm")
        self.fake_save(fo3, "Fallout3.esm")
        self.fake_save(ttw, "FalloutNV.esm", "Fallout3.esm", "TaleOfTwoWastelands.esm")
        for path, stamp in ((nv, 1), (fo3, 9), (ttw, 5)):
            os.utime(path, (stamp, stamp))
        profile = dict(self.profile, saves={"nv": str(self.tmp / "saves"), "fo3": str(self.tmp / "saves"),
                                            "ttw": str(self.tmp / "saves")})
        for game, expected in (("nv", nv), ("fo3", fo3), ("ttw", ttw)):
            self.assertEqual(round_runner.resolve_save({"game": game, "save": "latest"}, profile), expected, game)

    def test_latest_save_with_no_save_of_the_game_is_an_error(self):
        self.fake_save(self.tmp / "saves" / "Lone" / "fo3.omwsave", "Fallout3.esm")
        profile = dict(self.profile, saves={"nv": str(self.tmp / "saves")})
        with self.assertRaises(SystemExit) as raised:
            round_runner.resolve_save({"game": "nv", "save": "latest"}, profile)
        self.assertIn("no save of this game", str(raised.exception))

    def test_a_save_whose_header_cannot_be_read_is_not_turned_away(self):
        path = self.tmp / "saves" / "odd.omwsave"
        path.write_bytes(b"not a save")
        self.assertTrue(round_runner.save_is_for(path, "nv"))

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

    def test_profile_takes_a_saves_folder_and_warns_when_it_holds_no_save(self):
        class Given:
            engine = str(self.tmp / "bin" / "openfallout")
            out = None
            saves = None
            profile = str(self.tmp / "profile.json")

        (self.tmp / "bin").mkdir()
        (self.tmp / "bin" / "openfallout").write_text("x")
        (self.tmp / "Games" / "Fallout New Vegas").mkdir(parents=True)
        (self.tmp / "Games" / "Fallout New Vegas" / "FalloutNV.esm").write_text("x")
        (self.tmp / "elsewhere" / "Player").mkdir(parents=True)
        (self.tmp / "elsewhere" / "Player" / "Autosave.omwsave").write_bytes(b"1")
        for saves, warned in ((None, True), (str(self.tmp / "elsewhere"), False)):
            Given.saves = saves
            err = io.StringIO()
            with mock.patch.object(Path, "home", return_value=self.tmp), contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(err):
                round_runner.command_profile(Given)
            written = json.loads(Path(Given.profile).read_text())
            self.assertEqual("no save in" in err.getvalue(), warned, err.getvalue())
            if saves:
                self.assertEqual(written["saves"]["nv"], saves)

    def test_the_default_saves_folder_on_windows_is_the_user_data_folder_of_the_engine(self):
        with mock.patch.object(Path, "home", return_value=self.tmp), mock.patch.object(round_runner, "IS_MAC", False), \
                mock.patch.object(round_runner, "IS_WINDOWS", True):
            self.assertEqual(round_runner.player_user_data_dir(), self.tmp / "Documents" / "My Games" / "OpenFallout")

    @unittest.skipIf(round_runner.IS_MAC or round_runner.IS_WINDOWS, "only Linux follows XDG_DATA_HOME")
    def test_the_default_saves_folder_follows_xdg_data_home(self):
        class Given:
            engine = str(self.tmp / "bin" / "openfallout")
            out = None
            saves = None
            profile = str(self.tmp / "xdg-profile.json")

        (self.tmp / "bin").mkdir(exist_ok=True)
        (self.tmp / "bin" / "openfallout").write_text("x")
        (self.tmp / "Games" / "Fallout New Vegas").mkdir(parents=True, exist_ok=True)
        (self.tmp / "Games" / "Fallout New Vegas" / "FalloutNV.esm").write_text("x")
        with mock.patch.object(Path, "home", return_value=self.tmp), \
                mock.patch.dict(os.environ, {"XDG_DATA_HOME": str(self.tmp / "xdg")}), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            round_runner.command_profile(Given)
        self.assertEqual(json.loads(Path(Given.profile).read_text())["saves"]["nv"], str(self.tmp / "xdg" / "openfallout" / "saves"))

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

    def test_the_players_configuration_is_not_read(self):
        # the private openfallout.cfg says `replace=config`: the player's data=, content= and fallback= lines (which may name the
        # other game) do not count, and a content list in the profile is the whole list
        config = round_runner.load_round(self.round_file, self.Args())
        command = round_runner.engine_command(self.profile, config, self.tmp / "run", None)
        # the player's configuration folder is left out before it is parsed, and only that (a `--replace data` would drop the engine's own)
        self.assertEqual([c for i, c in enumerate(command) if command[i - 1] == "--replace"], ["config"])
        # `?userdata?` in the engine's own cfg is the player's data folder: the run names an empty one of its own
        self.assertEqual(command[command.index("--data-local") + 1], str(self.tmp / "run" / "userdata" / "data"))
        self.assertEqual([c for i, c in enumerate(command) if command[i - 1] == "--content"], ["FalloutNV.esm", "round.omwscripts"])
        variant = dict(name="v")
        round_runner.prepare_run(self.profile, config, variant, self.tmp / "run", None)
        self.assertEqual((self.tmp / "run" / "config" / "openfallout.cfg").read_text(), "replace=config\n")
        self.assertTrue((self.tmp / "run" / "userdata" / "data").is_dir())

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

    def test_the_players_settings_are_the_bottom_layer_of_the_run(self):
        # `replace=config` takes the player's settings.cfg out of the engine's own chain of defaults, so the runner lays it under
        # the settings of the profile, the round and the variant
        player = self.tmp / "player"
        player.mkdir()
        (player / "settings.cfg").write_text("[Terrain]\nviewing distance = 77777\n[Video]\nresolution x = 10\n[X]\ny = player\n")
        profile_settings = self.tmp / "profile-settings.cfg"
        profile_settings.write_text("[Terrain]\nobject paging = false\n")
        profile = dict(self.profile, settings=str(profile_settings))
        config = round_runner.load_round(self.round_file, self.Args())
        config["resolution"] = [800, 600]
        run = self.tmp / "run-player"
        with mock.patch.object(round_runner, "player_config_dir", return_value=player):
            round_runner.prepare_run(profile, config, config["variants"][1], run, None)
        settings = round_runner.parse_settings((run / "config" / "settings.cfg").read_text())
        self.assertEqual(settings["Terrain"], {"viewing distance": "77777", "object paging": "false"})
        self.assertEqual(settings["Video"]["resolution x"], "800")      # the round over the player
        self.assertEqual(settings["X"], {"y": "1"})                     # the variant over the player

    def test_missing_scenario_is_an_error(self):
        self.round_file.write_text(json.dumps({"name": "t"}))
        with self.assertRaises(SystemExit):
            round_runner.load_round(self.round_file, self.Args())


class WindowsRun(unittest.TestCase):
    def test_run_says_that_it_needs_macos_or_linux_on_windows(self):
        with mock.patch.object(round_runner, "IS_WINDOWS", True), self.assertRaises(SystemExit) as stop:
            round_runner.command_run(object())
        self.assertIn("macOS and Linux", str(stop.exception))


class Pictures(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(lambda: __import__("shutil").rmtree(self.tmp, ignore_errors=True))

    def test_a_file_the_engine_did_not_finish_is_not_a_picture(self):
        whole = self.tmp / "screenshot000.png"
        tiny_png(whole)
        self.assertTrue(round_runner.picture_is_complete(whole))
        empty = self.tmp / "screenshot001.png"
        empty.write_bytes(b"")
        cut = self.tmp / "screenshot002.png"
        cut.write_bytes(whole.read_bytes()[:-12])
        junk = self.tmp / "screenshot003.png"
        junk.write_bytes(b"not a png at all, but long enough to read")
        jpeg_cut = self.tmp / "screenshot004.jpg"
        jpeg_cut.write_bytes(b"\xff\xd8\xff\xe0 and no end")
        jpeg = self.tmp / "screenshot005.jpg"
        jpeg.write_bytes(b"\xff\xd8\xff\xe0 and an end\xff\xd9")
        self.assertEqual([round_runner.picture_is_complete(path) for path in (empty, cut, junk, jpeg_cut, jpeg)],
                         [False, False, False, False, True])
        missing = self.tmp / "nothing.png"
        self.assertFalse(round_runner.picture_is_complete(missing))

    def test_an_unfinished_screenshot_is_a_missing_shot_whichever_converter_is_there(self):
        userdata = self.tmp / "userdata"
        (userdata / "screenshots").mkdir(parents=True)
        tiny_png(userdata / "screenshots" / "screenshot000.png")
        (userdata / "screenshots" / "screenshot001.png").write_bytes(b"")
        # no Pillow and no sips: the copy of the file as it is must not take an unfinished one
        with mock.patch.dict(sys.modules, {"PIL": None}), mock.patch.object(round_runner, "IS_MAC", False):
            written, extra = round_runner.collect_shots(userdata, self.tmp / "shots", [(1, "a"), (2, "b")])
        self.assertEqual(extra, 0)
        self.assertTrue(written[0][2] is not None and written[0][2].exists())
        self.assertIsNone(written[1][2])

    def test_a_damaged_picture_is_a_missing_shot_where_pillow_reads_it(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest("needs Pillow")
        good = self.tmp / "good.png"
        tiny_png(good)
        data = bytearray(good.read_bytes())
        data[40:44] = b"\x00\x00\x00\x00"  # damage the compressed data, keep the signature and the end chunk
        bad = self.tmp / "bad.png"
        bad.write_bytes(bytes(data))
        self.assertTrue(round_runner.picture_is_complete(bad))
        self.assertIsNone(round_runner.convert_picture(bad, self.tmp / "bad.jpg"))
        self.assertEqual(round_runner.convert_picture(good, self.tmp / "good.jpg"), self.tmp / "good.jpg")


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
        lines += ["[10:00:01.500 W] Warning: Saved game dependency %s is missing." % name for name in @MISSING@]
        for n, name in enumerate(("first", "second"), 1):
            lines.append("[10:00:02.000 I] ROUND\\tshot\\t%s\\tn=%d" % (name, n))
            if n <= @WRITTEN@:
                png(shots / ("screenshot%03d.png" % (n - 1)))
                lines.append("[10:00:02.500 I] %s has been saved" % (shots / ("screenshot%03d.png" % (n - 1))))
        if @DONE@:
            lines.append("[10:00:03.000 I] ROUND\\tdone\\tend")
        (config / "openfallout.log").write_text("\\n".join(lines) + "\\n")
        if @LINGER@:
            # the engine stays up after its scenario, as it does while it waits out the delay before it quits
            import time
            time.sleep(@LINGER@)
        sys.exit(@STATUS@)
    """

    def run_round(self, status, done=True, written=2, extra=(), missing=(), allow=False, linger=0):
        tmp = Path(tempfile.mkdtemp(prefix="round-run-"))
        engine = self.make_engine(tmp, self.ENGINE.replace("@STATUS@", str(status)).replace("@DONE@", str(done))
                                  .replace("@WRITTEN@", str(written)).replace("@MISSING@", repr(list(missing)))
                                  .replace("@LINGER@", str(linger)))
        (tmp / "s.lua").write_text("return function(r) end")
        profile = {"engine": str(engine), "resources": str(tmp),
                   "games": {"nv": {"data": [str(tmp)], "content": ["FalloutNV.esm"]}}, "out": str(tmp / "out")}
        (tmp / "profile.json").write_text(json.dumps(profile))
        (tmp / "r.json").write_text(json.dumps({"name": "fake", "scenario": "s.lua", "variants": [{"name": "one"}, {"name": "two"}],
                                                **({"allow_missing_content": True} if allow else {})}))
        code = round_runner.main(["--profile", str(tmp / "profile.json"), "run", str(tmp / "r.json"), "--parallel", "1", *extra])
        return tmp, code

    def test_passing_run(self):
        tmp, code = self.run_round(0)
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
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
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["exit_status"], 3)
        self.assertIn("after the scenario had finished", report["exit_note"])
        self.assertIn("after the scenario had finished", (run / "one" / "report.md").read_text())

    def test_a_screenshot_that_was_asked_for_and_never_written_fails_the_round(self):
        tmp, code = self.run_round(0, written=1)
        self.assertEqual(code, 1)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "FAIL")
        self.assertEqual(report["missing_shots"], ["second"])
        text = (run / "one" / "report.md").read_text()
        self.assertIn("MISSING, so the round fails: second", text)

    def test_on_a_mac_a_round_that_could_not_measure_the_focus_does_not_pass(self):
        # no `lsappinfo` here, so every sample is empty: the round is not a proof of anything
        with mock.patch.object(round_runner, "IS_MAC", True), mock.patch.object(round_runner.shutil, "which", return_value=None), \
                mock.patch.object(round_runner, "make_system", return_value=round_runner.roundguard.System()):
            tmp, code = self.run_round(0)
        self.assertEqual(code, 1)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("NOT MEASURED", (run / "one" / "report.md").read_text())

    def test_a_variant_filter_that_matches_nothing_is_an_error(self):
        with self.assertRaises(SystemExit) as raised:
            self.run_round(0, extra=("--variant", "nope"))
        self.assertIn("no variant named nope", str(raised.exception))

    def test_a_variant_filter_runs_only_that_variant(self):
        tmp, code = self.run_round(0, extra=("--variant", "two"))
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
        self.assertEqual([p.name for p in run.iterdir() if p.is_dir()], ["two"])

    def test_an_engine_that_ends_before_the_scenario_fails(self):
        tmp, code = self.run_round(3, done=False)
        self.assertEqual(code, 1)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("before the scenario finished", report["exit_note"])
        # a round that never finished says where the log stopped
        text = (run / "one" / "report.md").read_text()
        self.assertIn("Last lines of the log", text)
        self.assertIn("ROUND\tshot\tsecond\tn=2", text)
        self.assertNotIn("10:00:02.000", text)

    def test_a_missing_script_file_of_the_save_is_a_note_and_a_missing_plugin_fails_the_round(self):
        # the save of a play session wants the plugin of the play, a script file: harmless
        tmp, code = self.run_round(0, missing=("play.omwscripts",))
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual((report["verdict"], report["content_not_loaded"], report["content_not_loaded_serious"]),
                         ("PASS", ["play.omwscripts"], []))
        self.assertIn("not loaded: play.omwscripts (script files only, harmless)", (run / "one" / "report.md").read_text())
        # a game plugin that is not loaded can change the world the save is in
        tmp, code = self.run_round(0, missing=("play.omwscripts", "Mod.esp"))
        self.assertEqual(code, 1)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual((report["verdict"], report["content_not_loaded_serious"]), ("FAIL", ["Mod.esp"]))
        self.assertIn("the round fails", (run / "one" / "report.md").read_text())
        # unless the round file allows it
        tmp, code = self.run_round(0, missing=("Mod.esp",), allow=True)
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
        self.assertIn("allowed by the round file", (run / "one" / "report.md").read_text())

    def test_missing_content_is_read_from_the_log(self):
        self.assertEqual(round_runner.missing_content("a\nWarning: Saved game dependency X.esm is missing.\n"
                                                      "Warning: Saved game dependency Y is missing.\n"), ["X.esm", "Y"])
        self.assertEqual(round_runner.missing_content("[10:00:01.500 W] Warning: Saved game dependency My Terrain Mod.esp "
                                                      "is missing.\nnext line\n"), ["My Terrain Mod.esp"])

    def test_the_runner_stops_an_engine_that_stays_up_once_the_pictures_are_written(self):
        # the engine waits out a long delay before it quits; the runner sees the end of the scenario and both pictures
        # in the log and stops it (a sleep of 120 s would otherwise outlast the test)
        started = time.monotonic()
        tmp, code = self.run_round(0, linger=120)
        self.assertLess(time.monotonic() - started, 60)
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual((report["verdict"], report["stopped_by_runner"], report["timed_out"]), ("PASS", True, False))
        self.assertIn("the runner stopped the engine", (run / "one" / "report.md").read_text())

    def test_pictures_reads_the_log_as_it_grows(self):
        log = Path(tempfile.mkdtemp(prefix="round-log-")) / "openfallout.log"
        pictures = round_runner.Pictures(log)
        self.assertFalse(pictures.poll())                      # no log yet
        log.write_text("[1 I] ROUND\tbegin\n[2 I] ROUND\tshot\ta\tn=1\n[3 I] ROUND\tshot\tb\tn=2\n")
        self.assertFalse(pictures.poll())
        with open(log, "a") as handle:
            handle.write("[4 I] ROUND\tdone\tend\n[5 I] x/screenshot000.png has been saved\n")
        self.assertFalse(pictures.poll())                      # the scenario ended, one of two pictures is written
        with open(log, "a") as handle:
            handle.write("[6 I] x/screenshot001.pn")           # a line cut in the middle is not counted yet
        self.assertFalse(pictures.poll())
        with open(log, "a") as handle:
            handle.write("g has been saved\n")
        self.assertTrue(pictures.poll())
        self.assertEqual((pictures.asked, pictures.saved, pictures.done), (2, 2, True))

    def test_a_scenario_with_no_pictures_is_over_when_it_ends(self):
        log = Path(tempfile.mkdtemp(prefix="round-log-")) / "openfallout.log"
        pictures = round_runner.Pictures(log)
        log.write_text("[1 I] ROUND\tbegin\n")
        self.assertFalse(pictures.poll())
        with open(log, "a") as handle:
            handle.write("[2 I] ROUND\tdone\tend\n")
        self.assertTrue(pictures.poll())

    def test_a_finished_round_has_no_log_tail(self):
        tmp, code = self.run_round(0)
        (run,) = runs_of(tmp)
        self.assertNotIn("Last lines of the log", (run / "one" / "report.md").read_text())

    def test_log_tail(self):
        self.assertEqual(round_runner.log_tail("[1 I] a\n\n[2 W] b\n[3 I] " + "x" * 300 + "\n", count=2),
                         ["b", "x" * 200])


if __name__ == "__main__":
    unittest.main()


class StandInSystem(round_runner.roundguard.System):
    """A computer with a window system whose answers the test decides: the heartbeat asks `ping` and reads `pid`."""
    name = "stand-in"
    heartbeat = True

    def __init__(self, ping=lambda: True, pid=100):
        self.ping_function, self.pid = ping, pid

    def ping(self): return self.ping_function()
    def window_server_pid(self): return self.pid
    def runningboardd_log(self): return "runningboardd evidence"


class GuardedRun(unittest.TestCase):
    """The guards around a whole round: the heartbeat, the kill switch, the lock, the pre-flight, one engine at a time."""
    ENGINE = WholeRun.ENGINE
    make_engine = WholeRun.make_engine
    HANGING = """
        import pathlib, sys, time
        args = sys.argv[1:]
        config = pathlib.Path(args[args.index("--config") + 1])
        (config / "openfallout.log").write_text("[10:00:00.000 I] start\\n[10:00:00.100 I] Headless: activation policy accessory set\\n")
        time.sleep(120)
    """

    def prepare(self, body=None, variants=("one", "two")):
        tmp = Path(tempfile.mkdtemp(prefix="round-guard-"))
        body = body or self.ENGINE.replace("@STATUS@", "0").replace("@DONE@", "True").replace("@WRITTEN@", "2") \
            .replace("@MISSING@", "[]").replace("@LINGER@", "0")
        engine = self.make_engine(tmp, body)
        (tmp / "s.lua").write_text("return function(r) end")
        profile = {"engine": str(engine), "resources": str(tmp), "games": {"nv": {"data": [str(tmp)], "content": ["FalloutNV.esm"]}},
                   "out": str(tmp / "out")}
        (tmp / "profile.json").write_text(json.dumps(profile))
        (tmp / "r.json").write_text(json.dumps({"name": "fake", "scenario": "s.lua", "variants": [{"name": v} for v in variants]}))
        return tmp

    def run_with(self, tmp, system, *extra):
        argv = ["--profile", str(tmp / "profile.json"), "run", str(tmp / "r.json"), *extra]
        quick = functools.partial(round_runner.roundguard.Heartbeat, interval=0.05, stall=0.4, first_answer=0.8, pid_interval=0.1)
        with mock.patch.object(round_runner, "make_system", return_value=system), \
                mock.patch.object(round_runner.roundguard, "Heartbeat", quick):
            return round_runner.main(argv)

    def test_a_healthy_round_reports_the_window_server_and_keeps_evidence(self):
        tmp = self.prepare()
        code = self.run_with(tmp, StandInSystem())
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual((report["verdict"], report["guard_tripped"]), ("PASS", None))
        self.assertEqual((report["window_server"]["start"], report["window_server"]["same"]), (100, True))
        self.assertEqual(report["evidence"], ["runningboardd-last-5m.log"])
        self.assertIn("WindowServer same pid: yes (pid 100)", (run / "summary.md").read_text())
        self.assertIn("WindowServer same pid: yes (pid 100)", (run / "one" / "report.md").read_text())
        # the guards' own files: the engine ended, the lock is released, the rest is noted
        self.assertTrue((tmp / "out" / "guard" / "last-heavy.json").exists())

    def test_a_window_system_that_stops_answering_kills_the_engine_and_ends_the_round(self):
        tmp = self.prepare(self.HANGING)
        release = threading.Event()
        calls = {"n": 0}

        def ping():
            calls["n"] += 1
            if calls["n"] <= 2:                       # the test call and the first heartbeat answer, then it hangs
                return True
            release.wait(60)
            return True
        started = time.monotonic()
        try:
            code = self.run_with(tmp, StandInSystem(ping=ping))
        finally:
            release.set()
        self.assertLess(time.monotonic() - started, 40)         # the engine sleeps for 120 s: it was killed, not waited for
        self.assertEqual(code, 3)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("WindowServer has not answered", report["guard_tripped"])
        self.assertIn("STOPPED BY A GUARD", (run / "one" / "report.md").read_text())
        self.assertEqual(report["headless_policy"], "accessory set")
        self.assertFalse((run / "two").exists())                # the round ended: the second variant never started
        summary = (run / "summary.md").read_text()
        self.assertIn("STOPPED BY A GUARD", summary)
        self.assertIn("not run: two", summary)

    def test_another_window_server_process_ends_the_round(self):
        tmp = self.prepare(self.HANGING)
        changed = threading.Event()
        threading.Timer(1.5, changed.set).start()        # WindowServer is killed and started again while the engine runs
        system = StandInSystem()
        system.window_server_pid = lambda: 28288 if changed.is_set() else 690
        code = self.run_with(tmp, system)
        self.assertEqual(code, 3)
        (run,) = runs_of(tmp)
        report = json.loads((run / "one" / "report.json").read_text())
        self.assertIn("another process (pid 690, now 28288)", report["guard_tripped"])
        self.assertIn("WindowServer same pid: NO (690 -> 28288)", (run / "summary.md").read_text())

    def test_the_kill_switch_ends_a_running_engine(self):
        tmp = self.prepare(self.HANGING)
        threading.Timer(1.5, lambda: roundguard_state(tmp).stop("test")).start()
        started = time.monotonic()
        code = self.run_with(tmp, round_runner.roundguard.System())
        self.assertLess(time.monotonic() - started, 40)
        self.assertEqual(code, 3)
        (run,) = runs_of(tmp)
        self.assertIn("kill switch", json.loads((run / "one" / "report.json").read_text())["guard_tripped"])

    def test_the_kill_switch_refuses_a_round_before_it_starts(self):
        tmp = self.prepare()
        roundguard_state(tmp).stop()
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            code = self.run_with(tmp, round_runner.roundguard.System())
        self.assertEqual(code, 4)
        self.assertEqual(runs_of(tmp), [])
        self.assertIn("kill switch is set", stderr.getvalue())

    def test_a_window_system_that_cannot_be_asked_refuses_the_round(self):
        tmp = self.prepare()
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            code = self.run_with(tmp, StandInSystem(ping=lambda: False))
        self.assertEqual(code, 4)
        self.assertEqual(runs_of(tmp), [])
        self.assertIn("did not answer the heartbeat's test call", stderr.getvalue())

    def test_a_second_round_or_build_is_refused_while_one_holds_the_lock(self):
        tmp = self.prepare()
        stderr = io.StringIO()
        with round_runner.roundguard.Lock(roundguard_state(tmp).lock_file, "build"):
            with contextlib.redirect_stderr(stderr):
                code = self.run_with(tmp, round_runner.roundguard.System())
        self.assertEqual(code, 4)
        self.assertIn("build, pid", stderr.getvalue())
        self.assertEqual(runs_of(tmp), [])

    def test_a_warning_is_in_the_summary_and_does_not_stop_the_round(self):
        tmp = self.prepare()
        system = StandInSystem()
        system.strict = True
        system.free_bytes = lambda: 1 * round_runner.roundguard.GIB
        code = self.run_with(tmp, system)
        self.assertEqual(code, 0)
        (run,) = runs_of(tmp)
        self.assertIn("pre-flight warning: only 1.0 GB of memory", (run / "summary.md").read_text())

    def test_parallel_above_one_runs_the_variants_one_after_the_other(self):
        tmp = self.prepare()
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = self.run_with(tmp, round_runner.roundguard.System(), "--parallel", "3")
        self.assertEqual(code, 0)
        self.assertIn("one after the other", out.getvalue())
        (run,) = runs_of(tmp)
        self.assertTrue((run / "one" / "report.json").exists() and (run / "two" / "report.json").exists())

    def test_the_rest_after_a_build_is_waited_out_on_a_mac_only(self):
        state = round_runner.roundguard.State(Path(tempfile.mkdtemp(prefix="guard-rest-")))
        state.heavy_ended("build")
        slept = []
        clock = {"now": 0.0}

        def sleep(seconds):
            slept.append(seconds)
            clock["now"] += seconds
        waited = round_runner.wait_settled(state, StandInSystem(), sleep=sleep, clock=lambda: clock["now"])      # StandIn does not settle
        self.assertEqual((waited, slept), (0.0, []))
        resting = StandInSystem()
        resting.settles = True
        waited = round_runner.wait_settled(state, resting, seconds=5, sleep=sleep, clock=lambda: clock["now"])
        self.assertGreater(waited, 3)
        self.assertAlmostEqual(sum(slept), waited, delta=1.5)
        # the kill switch ends the rest at once
        state.heavy_ended("run")
        state.stop()
        slept.clear()
        round_runner.wait_settled(state, resting, sleep=sleep, clock=lambda: clock["now"])
        self.assertEqual(slept, [])

    def test_the_engine_is_started_with_the_least_that_it_asks_of_the_window_system(self):
        mac = round_runner.engine_environment({}, True)
        self.assertEqual((mac["OPENFALLOUT_HEADLESS"], mac["SDL_VIDEO_ALLOW_SCREENSAVER"], mac["OPENFALLOUT_HEADLESS_POLICY"]),
                         ("1", "1", "accessory"))
        self.assertNotIn("OPENFALLOUT_HEADLESS_POLICY", {k: v for k, v in round_runner.engine_environment({}, False).items()
                                                         if k not in os.environ})
        # the round or the variant has the last word
        self.assertEqual(round_runner.engine_environment({"OPENFALLOUT_HEADLESS_POLICY": "foreground"}, True)["OPENFALLOUT_HEADLESS_POLICY"],
                         "foreground")
        self.assertEqual(round_runner.DEFAULT_SETTINGS["Video"]["framerate limit"], "20")
        self.assertEqual(round_runner.FOCUS_INTERVAL, 5.0)

    def test_the_policy_of_the_log_and_the_text_of_the_pid(self):
        self.assertEqual(round_runner.headless_policy("a\n[10:00:00.100 I] Headless: activation policy accessory set\n"), "accessory set")
        self.assertEqual(round_runner.headless_policy("Headless: activation policy prohibited refused"), "prohibited refused")
        self.assertIsNone(round_runner.headless_policy("nothing"))
        self.assertEqual(round_runner.window_server_text(None), "not measured")
        self.assertEqual(round_runner.window_server_text({"start": None, "end": None, "same": None}), "not measured")
        self.assertEqual(round_runner.window_server_text({"start": 1, "end": None, "same": False}), "NO (1 -> gone)")


class GuardedBuild(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="build-guard-"))
        (self.tmp / "profile.json").write_text(json.dumps({"out": str(self.tmp / "out")}))
        self.argv = ["--profile", str(self.tmp / "profile.json"), "build", "--repo", str(self.tmp), "--ref", "x"]

    def build(self, *extra, system=None):
        with mock.patch.object(round_runner, "make_system", return_value=system or round_runner.roundguard.System()), \
                mock.patch.object(round_runner, "build_engine", return_value=0) as built:
            code = round_runner.main(self.argv + list(extra))
        return code, built

    def test_a_build_is_held_to_six_jobs_and_notes_when_it_ended(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code, built = self.build("--jobs", "12")
        self.assertEqual(code, 0)
        self.assertEqual(built.call_args[0][1], 6)
        self.assertIn("held to 6", out.getvalue())
        self.assertTrue((self.tmp / "out" / "guard" / "last-heavy.json").exists())
        code, built = self.build("--jobs", "12", "--allow-heavy")
        self.assertEqual(built.call_args[0][1], 12)
        code, built = self.build()
        self.assertLessEqual(built.call_args[0][1], 6)

    def test_a_build_is_refused_by_the_kill_switch_and_by_a_round_that_holds_the_lock(self):
        state = roundguard_state(self.tmp)
        state.stop()
        with contextlib.redirect_stderr(io.StringIO()):
            code, built = self.build()
        self.assertEqual(code, 4)
        built.assert_not_called()
        state.resume()
        with round_runner.roundguard.Lock(state.lock_file, "round x"), contextlib.redirect_stderr(io.StringIO()):
            code, built = self.build()
        self.assertEqual(code, 4)
        built.assert_not_called()

    def test_preflight_stop_resume_and_ack_panic(self):
        base = ["--profile", str(self.tmp / "profile.json")]
        with mock.patch.object(round_runner, "make_system", return_value=round_runner.roundguard.System()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(round_runner.main(base + ["preflight"]), 0)
            self.assertEqual(round_runner.main(base + ["stop", "--note", "test"]), 0)
            self.assertTrue(roundguard_state(self.tmp).stopped())
            self.assertEqual(round_runner.main(base + ["preflight"]), 4)
            self.assertEqual(round_runner.main(base + ["resume"]), 0)
            self.assertFalse(roundguard_state(self.tmp).stopped())
            self.assertEqual(round_runner.main(base + ["ack-panic", "--note", "looked at it"]), 0)
            self.assertIsNotNone(roundguard_state(self.tmp).acknowledged())


def roundguard_state(tmp):
    """The guards' state of the folder of results `<tmp>/out`."""
    return round_runner.roundguard.State(tmp / "out")
