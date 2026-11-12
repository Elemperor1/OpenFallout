#!/usr/bin/env python3
"""Start the OpenFallout app bundle through macOS's `open`, the way that lets the system turn on Game Mode.

macOS only gives Game Mode to a game that LaunchServices started; a binary started from a terminal never gets it.
`open` is Apple's way to start a bundle from the command line and still pass arguments, environment variables and
standard output and error. This helper wraps it:

    scripts/openfallout/mac_launch.py [--app PATH] [--stdout FILE] [--stderr FILE] [--env NAME=VALUE]...
                                      [--metal-hud] [--] [engine arguments...]

Everything after `--` goes to the engine unchanged (`--config`, `--user-data`, `--skip-menu`, `--start`, ...).
The helper waits until the app has quit. Its exit status is that of `open`: non-zero when the app could not be
started. `open` does not report the app's own exit code, so a caller that needs to know how the engine ended reads
the log files.

The probe and autoplay tools keep starting the binary directly; that still works, but Game Mode stays off.
"""
import argparse
from pathlib import Path
import plistlib
import subprocess
import sys

DEFAULT_APP = "/Applications/OpenFallout.app"
ENGINE = "openfallout"


def bundle_executable(app):
    """The program `open` starts for the bundle, or None when its Info.plist cannot be read."""
    try:
        with (Path(app) / "Contents/Info.plist").open("rb") as file:
            return plistlib.load(file).get("CFBundleExecutable")
    except (OSError, plistlib.InvalidFileException):
        return None


def build_command(app, stdout, stderr, env, engine_args):
    command = ["open", "-n", "-W", "--stdout", str(stdout), "--stderr", str(stderr)]
    for assignment in env:
        command += ["--env", assignment]
    return command + ["-a", str(app), "--args", *engine_args]


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    engine_args = []
    if "--" in argv:
        split = argv.index("--")
        argv, engine_args = argv[:split], argv[split + 1:]
    parser = argparse.ArgumentParser(usage="%(prog)s [options] [-- engine arguments...]",
                                     description=__doc__.split("\n\n")[0])
    parser.add_argument("--app", default=DEFAULT_APP, help="app bundle to start (default: %(default)s)")
    parser.add_argument("--stdout", default="openfallout.stdout.log", help="file for standard output")
    parser.add_argument("--stderr", default="openfallout.stderr.log", help="file for standard error")
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE",
                        help="environment variable for the app (repeatable)")
    parser.add_argument("--metal-hud", action="store_true", help="show the Metal performance HUD (MTL_HUD_ENABLED=1)")
    options = parser.parse_args(argv)

    env = options.env + (["MTL_HUD_ENABLED=1"] if options.metal_hud else [])
    executable = bundle_executable(options.app)
    if executable not in (None, ENGINE):
        print(f"mac_launch: {options.app} starts {executable}, not {ENGINE}; the engine arguments go to that program",
              file=sys.stderr)

    stdout, stderr = Path(options.stdout).absolute(), Path(options.stderr).absolute()
    command = build_command(Path(options.app).absolute(), stdout, stderr, env, engine_args)
    return subprocess.run(command).returncode


if __name__ == "__main__":
    sys.exit(main())
