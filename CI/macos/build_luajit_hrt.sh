#!/bin/bash -e

# Rebuilds the LuaJIT of the macOS dependency set with LUAJIT_ENABLE_OSX_HRT and puts it in place of the prebuilt one.
#
#   CI/macos/build_luajit_hrt.sh [DEPS_ROOT]     (default /tmp/openmw-deps)
#
# The prebuilt LuaJIT flips page protection with mprotect; under the hardened runtime that needs the
# allow-unsigned-executable-memory entitlement, and with allow-jit alone the engine is killed at its first compiled
# trace. Built with LUAJIT_ENABLE_OSX_HRT, LuaJIT maps its machine code with MAP_JIT and switches write protection per
# thread, which allow-jit permits (files/mac/openfallout.entitlements). Decision and evidence:
# docs/openfallout/mac-luajit.md.
#
# The LuaJIT commit is the one the dependency set was built from (its vcpkg SBOM), so a dependency update moves this
# build along with it. luajit-osx-hrt-void-return.patch fixes the one line that keeps the flag from compiling with
# current clang ("void function should not return a value"); it is skipped once LuaJIT carries the fix.
# Remove this script once the dependency set itself is built with the flag (see the documentation).

DEPS_ROOT="${1:-/tmp/openmw-deps}"
HERE="$(cd "$(dirname "$0")" && pwd)"
# 11.0 is the least the flag needs (MAP_JIT); the app's own minimum is set in CI/before_script.macos.sh
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-14.8}"

if [[ "$(uname -m)" == "x86_64" ]]; then TRIPLET="x64-osx-dynamic"; else TRIPLET="arm64-osx-dynamic"; fi
INSTALLED="$DEPS_ROOT/installed/$TRIPLET"
SBOM="$INSTALLED/share/luajit/vcpkg.spdx.json"
COMMIT=$(grep -o 'git+https://github.com/LuaJIT/LuaJIT@[0-9a-f]\{40\}' "$SBOM" | head -n 1 | cut -d@ -f2)
[[ -n "$COMMIT" ]] || { echo "No LuaJIT commit in $SBOM" >&2; exit 1; }
TARGET=$(cd "$INSTALLED/lib" && ls libluajit-5.1.2.1.*.dylib | head -n 1)
[[ -f "$INSTALLED/lib/$TARGET" && ! -L "$INSTALLED/lib/$TARGET" ]] || { echo "No LuaJIT library in $INSTALLED/lib" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
curl --fail --retry 3 -sSL "https://github.com/LuaJIT/LuaJIT/archive/$COMMIT.tar.gz" | tar -xz -C "$WORK" --strip-components 1
if patch -d "$WORK" -p1 --forward --dry-run -s < "$HERE/luajit-osx-hrt-void-return.patch" > /dev/null 2>&1; then
    patch -d "$WORK" -p1 --forward -s < "$HERE/luajit-osx-hrt-void-return.patch"
fi
make -C "$WORK" -j"$(sysctl -n hw.ncpu)" XCFLAGS=-DLUAJIT_ENABLE_OSX_HRT > "$WORK/build.log" 2>&1 || { cat "$WORK/build.log"; exit 1; }

BUILT="$WORK/src/libluajit.so"
if ! nm -u "$BUILT" | grep -q '_pthread_jit_write_protect_np'; then
    echo "LuaJIT $COMMIT was built without hardened-runtime support (no pthread_jit_write_protect_np)" >&2
    exit 1
fi
install_name_tool -id "@rpath/$TARGET" "$BUILT"
cp "$BUILT" "$INSTALLED/lib/$TARGET"
echo "LuaJIT $COMMIT rebuilt with LUAJIT_ENABLE_OSX_HRT (macOS $MACOSX_DEPLOYMENT_TARGET) -> $INSTALLED/lib/$TARGET"
