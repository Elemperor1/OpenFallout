# LuaJIT under the hardened runtime (macOS)

The macOS package ships **LuaJIT built with `LUAJIT_ENABLE_OSX_HRT`** and the game is signed with the
**`com.apple.security.cs.allow-jit`** entitlement (`files/mac/openfallout.entitlements`). The compiler stays on under
the hardened runtime. This page records why, and how to check it again after a dependency update.

Decided 2026-11-12 (ticket #183, spec #166 stories 3 and 23), on an Apple Silicon Mac, macOS 27.2.1, Apple clang 21.0.0.

## The three options and what decided

| Option | LuaJIT | Entitlement | Result |
| --- | --- | --- | --- |
| 1 (ships) | built with `LUAJIT_ENABLE_OSX_HRT` | `allow-jit` | compiler on, hot loop compiled; packaged app PASS |
| 2 (fallback) | the dependency set's build (mprotect) | `allow-unsigned-executable-memory` | compiler on in the probe; not launched as a package |
| 3 (last resort) | any | none | not needed |

The packaged-app check (`CI/check_mac_package.py`) and a small probe decided, not documentation. Option 2 also works,
but it allows writable and executable memory for the whole process, so option 1 is the one that ships.

What goes wrong without the right pair (probe below, hardened runtime):

- The dependency set's LuaJIT with `allow-jit` only, or with no entitlement: the process is **killed at the first
  compiled trace** (exit 137, crash report `EXC_BAD_ACCESS`, `SIGKILL (Code Signature Invalid)`, namespace
  CODESIGNING, "Invalid Page", in `lua_pcall`). In the game that is a crash at start with a crash dialog.
- The HRT build with no JIT entitlement: no crash; the hardened runtime refuses the `MAP_JIT` memory and LuaJIT turns
  its compiler off (`jit.status()` false, 1 trace aborted with error 27). Lua still runs, slower.

The packaged-app check now has a static `luajit` step that reads the bundled LuaJIT (does it import
`pthread_jit_write_protect_np`?) and the engine's signature and entitlements, and says which compiler state the game
will have. It fails the crash case and the "off" case (when `--expect-jit on`, the default) before starting anything.

## Where LuaJIT comes from, and what changed

The macOS dependency set is the prebuilt vcpkg archive of `OpenMW/openmw-deps-build` (`CI/macos/deps_versions.sh`,
`CI/macos/fetch_deps_manifest.sh`). Release 2026-09-17 holds vcpkg port `luajit` 2026-01-09, LuaJIT commit
`707c12bf00dafdfd3899b1a6c36435dbbf6c7022` (from `share/luajit/vcpkg.spdx.json`), built for macOS 14.0. That library
imports `mprotect` and not `pthread_jit_write_protect_np`: it is not an HRT build.

This repository therefore rebuilds it: `CI/macos/build_luajit_hrt.sh`, called by `CI/before_install.macos.sh` after
the archive is unpacked, downloads the same LuaJIT commit (read from the SBOM, so a dependency update moves it along),
builds it with `XCFLAGS=-DLUAJIT_ENABLE_OSX_HRT` for macOS 14.8 (the flag needs 11.0 or later), refuses a result that
does not import `pthread_jit_write_protect_np`, and puts it in place of the prebuilt library under the same name and
install name. The dependency cache key includes the script and its patch.

`CI/macos/luajit-osx-hrt-void-return.patch`: with the flag on, `lj_mcode.c` does not compile with current clang
(`error: void function 'mcode_setprot' should not return a value [-Wreturn-mismatch]`, a `return 0;` in a `void`
function on the HRT path). The patch changes it to `return;`. The line is the same at the dependency set's commit and
on LuaJIT's `v2.1` branch as of 2026-11-11 (`63c8f1a9`); the script skips the patch once it no longer applies.

### What a release needs outside this repository

The rebuild can go once the dependency set itself ships an HRT LuaJIT. The change belongs in `OpenMW/openmw-deps-build`
(nothing was pushed there): for the macOS triplets, build the `luajit` port with `-DLUAJIT_ENABLE_OSX_HRT` in its C
flags (for example in the triplet: `if(PORT STREQUAL "luajit") string(APPEND VCPKG_C_FLAGS " -DLUAJIT_ENABLE_OSX_HRT")
endif()`, with `VCPKG_CXX_FLAGS` set alongside as vcpkg requires) and add the void-return patch to the port (or an
overlay port) until LuaJIT carries the fix. Then check that the published library imports `pthread_jit_write_protect_np`
and delete `CI/macos/build_luajit_hrt.sh` and its call.

A release also needs a Developer ID identity (`OPENFALLOUT_CODESIGN_IDENTITY`): with it, every bundled library carries
the team's ID and library validation stays on. CI imports it from secrets and notarizes the image: see [mac-release.md](mac-release.md).

## Evidence

### Probe (no game)

A command-line program linked against each LuaJIT, run from the shell. It runs a hot loop with a trace listener and
prints `jit.status()` and the compiled and aborted trace counts:

```c
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
static const char *script =
  "local jit = require('jit')\n"
  "local stop, abort, why = 0, 0, ''\n"
  "jit.attach(function(what, tr, func, pc, otr, oex)\n"
  "  if what == 'stop' then stop = stop + 1 elseif what == 'abort' then abort = abort + 1; why = tostring(otr)..' '..tostring(oex) end\n"
  "end, 'trace')\n"
  "local s = 0\n"
  "for i = 1, 2000000 do s = s + (i % 7) * 3 end\n"
  "jit.attach(function() end)\n"
  "local st = {jit.status()}\n"
  "local flags = {} for i = 2, #st do flags[#flags+1] = tostring(st[i]) end\n"
  "print(string.format('%s; jit.status: %s (%s); compiled traces: %d, aborted: %d %s; sum %d',\n"
  "  jit.version, tostring(st[1]), table.concat(flags, ' '), stop, abort, why, s))\n";
int main(void) {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dostring(L, script)) { printf("error: %s\n", lua_tostring(L, -1)); return 1; }
  lua_close(L);
  return 0;
}
```

Built with `clang -O1 jitprobe.c -o jitprobe libluajit-5.1.2.dylib -Wl,-rpath,@executable_path`, the library next to it
signed `codesign -f -s -`, and the program signed either plain ad hoc (`codesign -f -s -`) or hardened ad hoc
(`codesign -f -s - --options runtime --entitlements E`), where E always holds
`com.apple.security.cs.disable-library-validation` (an ad hoc signature has no Team ID; this key changes nothing about
JIT memory) plus the entitlement under test. Output (`deps` = the dependency set's library, `hrt` = the same commit
built with the flag by `CI/macos/build_luajit_hrt.sh`):

```
deps  plain     flags=0x2(adhoc)            : exit=0   jit.status: true; compiled traces: 1, aborted: 0
deps  none      flags=0x10002(adhoc,runtime): exit=137 (SIGKILL, Code Signature Invalid)
deps  allowjit  flags=0x10002(adhoc,runtime): exit=137 (SIGKILL, Code Signature Invalid)
deps  unsigned  flags=0x10002(adhoc,runtime): exit=0   jit.status: true; compiled traces: 1, aborted: 0
hrt   plain     flags=0x2(adhoc)            : exit=0   jit.status: true; compiled traces: 1, aborted: 0
hrt   none      flags=0x10002(adhoc,runtime): exit=0   jit.status: false; compiled traces: 0, aborted: 1 (error 27)
hrt   allowjit  flags=0x10002(adhoc,runtime): exit=0   jit.status: true; compiled traces: 1, aborted: 0
hrt   unsigned  flags=0x10002(adhoc,runtime): exit=0   jit.status: true; compiled traces: 1, aborted: 0
```

Every line that ran printed `LuaJIT 2.1.1767980792` with flags `fold cse dce fwd dse narrow loop abc sink fuse`.

### Packaged app

The CI-style package of ticket #181 (vcpkg deps 2026-09-17, launcher and engine), with its LuaJIT replaced by the
output of `CI/macos/build_luajit_hrt.sh`, re-signed by `cmake/SignMacApplications.cmake` with
`OPENFALLOUT_CODESIGN_IDENTITY=-` and `OPENFALLOUT_CODESIGN_LOCAL_TEST=1` (hardened ad hoc; the engine carries
`allow-jit` plus the local-test `disable-library-validation`):

```
$ CI/check_mac_package.py OpenFallout.app --smoke run --expect-jit on
PASS layout
PASS signatures: 67 Mach-O files and 6 nested bundles verified, app signature: ad hoc
PASS info.plist
PASS openal: links @executable_path/../Frameworks/libopenal.1.25.0.dylib -> Contents/Frameworks/libopenal.1.25.0.dylib
PASS library-validation: hardened runtime, library validation disabled by entitlement
PASS luajit: Contents/Frameworks/libluajit-5.1.2.1.1767980792.dylib is a hardened-runtime build (MAP_JIT); hardened runtime with allow-jit, so the compiler is expected on
PASS smoke: started in OFTestCell, LuaJIT compiler: on (fold cse dce fwd dse narrow loop abc sink fuse); hot loop: 1 compiled trace(s), 0 aborted, quit peacefully
package check: PASS
```

The same package with the dependency set's own LuaJIT and the same entitlements was not started: the static step
says `FAIL luajit: ... is a default build (mprotect, no LUAJIT_ENABLE_OSX_HRT); under the hardened runtime without
allow-unsigned-executable-memory the engine is killed at its first compiled trace (Code Signature Invalid)`.

### The engine's compiler line

The engine runs its hot-loop probe and logs `LuaJIT compiler: on|off (<flags>); hot loop: <n> compiled trace(s), <m>
aborted` once per run only when the environment has `OPENFALLOUT_LUAJIT_PROBE=1`. The smoke step of
`CI/check_mac_package.py` passes it through `scripts/openfallout/mac_launch.py --env`; to see the line in a normal run,
start the engine with that variable set. Without it the engine does no probe and logs nothing about the compiler, on
every platform. The local check above also says in its `library-validation` line that library validation is off;
release mode (`--mode release`) fails a package whose binaries carry `disable-library-validation`.

### Testing a hardened package locally

Without a certificate: build the package with `OPENFALLOUT_CODESIGN_IDENTITY=-` and `OPENFALLOUT_CODESIGN_LOCAL_TEST=1`.
The signer adds `files/mac/local-hardened-test.entitlements` (library validation off) to the release entitlements in a
file next to the staged app. The signer refuses this option with any other identity, and `CI/test_mac_bundles.py`
checks that the release entitlements never carry the key. Run `CI/check_mac_package.py` with `--smoke skip` first and
start the package only when the static steps pass.

## Not verified

- Intel Macs: the script builds the x86-64 library the same way, but nothing was run there.
- A Developer ID signature with library validation on, and notarization (no certificate on the test Mac).
- A full CI run of the script on a hosted runner; it was run locally on an unpacked copy of the dependency set.
- Longer play: the smoke run compiles one hot loop. LuaJIT switches write protection per thread; the engine's Lua
  worker thread was not exercised beyond the smoke run.
- LuaJIT commit `716f74a6` (2026-10-18, "macOS: Avoid collision of mcode with xzone malloc reclaim ring") is newer than
  the dependency set's commit and is not in this build.
