# CI build cache contract

`CI/build_cache.py` fingerprints a build environment and reconciles source file
timestamps after a workflow restores a cached build directory. It does not
transfer or save the build directory itself: the GitHub Actions cache steps in
the [Linux](../../.github/workflows/push.yml),
[macOS](../../.github/workflows/macos.yml), and
[Windows](../../.github/workflows/windows.yml) workflows do that. The related
test-report behavior is described in [CI reporting](ci-reporting.md).

## Working directory and cache identity

Run the script from the checkout root. The command uses `Path.cwd()` as the
checkout root, and `git ls-files` is run there. The workflows run these commands
from the repository root. A different current directory changes the root and
the interpretation of relative arguments.

The `key` command prints a SHA-256 fingerprint of:

- the canonical absolute checkout root;
- the supplied mode and `ImageVersion` environment value;
- the resolved compiler path and its version (on Windows, the compiler file's
  SHA-256; elsewhere, the compiler's `--version` output);
- `cmake --version`, and the SDK path on systems where `xcrun` is available;
- `VCToolsVersion`, `WindowsSDKVersion`, `SDKROOT`, `DEVELOPER_DIR`, `CC`,
  `CXX`, `CFLAGS`, `CXXFLAGS`, and `LDFLAGS`;
- contents of tracked CMake rules (`CMakeLists.txt`, `.cmake` files, `CI/`, and
  `.github/workflows/`); and
- the contents of each explicitly supplied dependency file.

This is deliberately tied to the absolute checkout path because generated
build rules and toolchain state contain absolute paths. The snapshot also
records that same canonical root, so a snapshot from another checkout path is
incompatible. The build directory itself is not in the fingerprint. Keep the
build directory path consistent when using a custom `--build-dir`; callers
must ensure the cache key and build directory refer to the same environment.

The fingerprint is only useful if the caller uses it to namespace the external
build cache. `restore` does not recompute or validate compiler, mode, dependency,
or configure-option identity. It verifies the snapshot version and checkout
root, then reconciles tracked source files. Untracked files are not included in
the source snapshot or tracked CMake-rule fingerprint. Dependency files are
hashed only when explicitly passed to `key`; relative dependency paths are
resolved from the current working directory.

The CLI accepts `key`, `snapshot`, and `restore`. `--build-dir` defaults to
`build`; it must resolve to a directory beneath the checkout and the argument
itself cannot be a symlink. This keeps the cleanup performed by `restore`
confined to an in-checkout build directory. `--compiler`, repeated
`--dependency-file`, and `--mode` affect the key command.

## Snapshot and restore sequence

The intended sequence is:

1. Compute `key` before asking the external cache to restore a build directory.
2. After the external cache restore, run `restore` before CMake configuration
   and the build.
3. After a successful configuration/build, run `snapshot` before saving the
   build directory to the external cache.

`snapshot` walks paths reported by `git ls-files`. For each tracked regular
file that is not a symlink, it records the SHA-256 content digest and
nanosecond modification time in `build/.ci-source-state.json`, along with
snapshot version 1 and the absolute checkout root. It creates the build
directory if needed. It records source metadata only; the workflow's cache
action stores the build outputs.

`restore` handles the cache as follows:

1. If the build directory does not exist, it prints that no incremental cache
   was restored and returns without creating or deleting anything.
2. It reads and validates `.ci-source-state.json`: version must be 1, the
   recorded root must match, and `files` must be a mapping whose entries hold a
   string `sha256` value and integer `mtime_ns`.
3. It enumerates current tracked regular non-symlink files. If any path present
   in the snapshot is no longer in that set, it discards the cached build tree.
   This also covers a formerly regular tracked path that is now missing or a
   symlink. A newly tracked file does not by itself invalidate the tree.
4. Otherwise, for each current tracked file, it compares the content digest.
   Unchanged files get their recorded modification time back. Changed and new
   files receive a common time at least one second newer than the newest file
   in the restored build tree, or the current time if that is later. This makes
   normal build dependency checks see those inputs as newer than restored
   outputs, even if checkout timestamps were old. The file's current access
   time is retained.

The build directory is deleted only in these two restore cases: the snapshot is
missing, unreadable, malformed, at an unsupported version, or for a different
checkout root; or the snapshot contains a tracked regular file that is no
longer present as a current tracked regular non-symlink file. Removal is
intentional: CMake may have copied a source-only resource into the build tree,
and retaining that output after its source was removed could ship stale content.
Deletion removes the entire selected build directory; the script then returns
and leaves the next configure/build to create a fresh one. A missing build
directory is a no-op. Changed or newly added files cause timestamp updates, not
directory deletion.

## Safe use and recovery

- Run from the repository root, use the same canonical checkout path across
  cache save and restore, and keep the selected build directory consistent.
- Let the workflow's environment fingerprint determine the external cache
  namespace. Do not assume the snapshot metadata alone proves compiler,
  dependency, generator, or configure-option compatibility.
- Run `restore` only after the intended cache action has restored the intended
  in-checkout build directory. The deletion cases above remove that whole
  directory, not just `.ci-source-state.json`.
- If no cache is available, configure and build normally. If the metadata is
  invalid or a tracked file was removed, the script discards the cached tree;
  configure and build normally from an empty build directory. If changed files
  are detected, allow the regular build system to rebuild their dependents.
- `snapshot` is not a repair operation: take it after a successful build so its
  hashes and timestamps describe the source state associated with that build.

The implementation's unit tests use temporary repositories and build
directories. They cover unchanged checkout reuse, changed header/source
rebuilds despite stale timestamps, key changes, removed resources, and
incompatible snapshots. This documentation reports those as unit-test coverage;
it does not claim a live hosted cache restore, manual play, automated in-game
evidence, or engine parity.
