#!/usr/bin/env python3
"""Fingerprint build environments and preserve unchanged checkout timestamps."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


STATE_FILE = ".ci-source-state.json"


def tracked_files(root):
    output = subprocess.check_output(["git", "ls-files", "-z"], cwd=root)
    return [Path(os.fsdecode(name)) for name in output.split(b"\0") if name]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_key(root, compiler, dependency_files, mode):
    # A build tree contains absolute paths, generated rules and toolchain state.
    # Never fall back to a cache with a different environment fingerprint.
    identity = {
        "root": str(root.resolve()),
        "mode": mode,
        "image": os.environ.get("ImageVersion", ""),
        "compiler": str(Path(shutil.which(compiler) or compiler).resolve()),
        "cmake": subprocess.check_output(["cmake", "--version"], text=True),
        "environment": {key: os.environ.get(key, "") for key in (
            "VCToolsVersion", "WindowsSDKVersion", "SDKROOT", "DEVELOPER_DIR",
            "CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS",
        )},
        "rules": {},
        "dependencies": [digest(Path(path)) for path in dependency_files],
    }
    if os.name == "nt":
        # cl.exe doesn't support --version; its content identifies MSVC exactly.
        identity["compiler_version"] = digest(Path(identity["compiler"]))
    else:
        identity["compiler_version"] = subprocess.check_output(
            [compiler, "--version"], text=True
        )
        if shutil.which("xcrun"):
            identity["sdk"] = subprocess.check_output(
                ["xcrun", "--show-sdk-path"], text=True
            )
    for relative in tracked_files(root):
        if (relative.name == "CMakeLists.txt" or relative.suffix == ".cmake"
                or relative.parts[0] == "CI"
                or relative.parts[:2] == (".github", "workflows")):
            path = root / relative
            if path.is_file() and not path.is_symlink():
                identity["rules"][relative.as_posix()] = digest(path)
    return hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()


def snapshot(root, build):
    files = {}
    for relative in tracked_files(root):
        path = root / relative
        if path.is_file() and not path.is_symlink():
            files[relative.as_posix()] = {
                "sha256": digest(path), "mtime_ns": path.stat().st_mtime_ns,
            }
    build.mkdir(parents=True, exist_ok=True)
    (build / STATE_FILE).write_text(json.dumps({
        "version": 1, "root": str(root.resolve()), "files": files,
    }, sort_keys=True))
    print(f"Recorded timestamps for {len(files)} tracked files")


def restore(root, build):
    if not build.exists():
        print("No incremental build cache restored; starting a fresh build")
        return
    try:
        state = json.loads((build / STATE_FILE).read_text())
        if state["version"] != 1 or state["root"] != str(root.resolve()):
            raise ValueError("incompatible source snapshot")
        files = state["files"]
        if not isinstance(files, dict) or any(
                not isinstance(value, dict)
                or not isinstance(value.get("sha256"), str)
                or not isinstance(value.get("mtime_ns"), int)
                for value in files.values()):
            raise ValueError("invalid source snapshot")
    except (OSError, ValueError, KeyError, TypeError):
        shutil.rmtree(build)
        print("Discarded incomplete/incompatible build cache")
        return
    unchanged = changed = 0
    # New/changed files must be newer than all restored build outputs, even if
    # a checkout or filesystem supplied an older timestamp.
    newest_output = max((path.stat().st_mtime_ns for path in build.rglob("*")
                         if path.is_file()), default=0)
    changed_time = max(time.time_ns(), newest_output + 1_000_000_000)
    for relative in tracked_files(root):
        path = root / relative
        if not path.is_file() or path.is_symlink():
            continue
        previous = files.get(relative.as_posix(), {})
        if previous.get("sha256") == digest(path):
            modified = previous["mtime_ns"]
            unchanged += 1
        else:
            modified = changed_time
            changed += 1
        os.utime(path, ns=(path.stat().st_atime_ns, modified))
    print(f"Restored timestamps for {unchanged} unchanged files; {changed} require rebuilding")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("key", "snapshot", "restore"))
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--compiler", default="cc")
    parser.add_argument("--dependency-file", action="append", default=[])
    parser.add_argument("--mode", default="")
    args = parser.parse_args()
    root = Path.cwd().resolve()
    requested_build = Path(args.build_dir)
    build = requested_build.resolve()
    if build == root or root not in build.parents or requested_build.is_symlink():
        parser.error("build directory must be a directory beneath the checkout")
    if args.command == "key":
        print(build_key(root, args.compiler, args.dependency_file, args.mode))
    elif args.command == "snapshot":
        snapshot(root, build)
    else:
        restore(root, build)


if __name__ == "__main__":
    main()
