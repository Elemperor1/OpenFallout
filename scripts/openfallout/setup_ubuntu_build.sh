#!/bin/bash
# Installs everything needed to build and test OpenFallout on Ubuntu 24.04, the Qt tools included.
#
# Ubuntu ships MyGUI 3.4.2 and a Recast without CMake package files, but the build needs MyGUI 3.4.3 and a CMake
# config for Recast. CMake would normally download both from GitHub. Where that is blocked, this script builds them
# from the source packages in the Ubuntu archive instead and installs them under $PREFIX.
#
# Run as root (or with sudo available), then follow the CMake command it prints.

set -euo pipefail

PREFIX="${PREFIX:-$HOME/openfallout-deps}"
SRC="${SRC:-$PREFIX/src}"
JOBS="${JOBS:-$(nproc)}"
ARCHIVE="https://archive.ubuntu.com/ubuntu/pool/universe"

MYGUI_TARBALL="mygui_3.4.3+dfsg.orig.tar.gz"
MYGUI_SHA256="a1accc771075652cfff0390f6a73a99d87ce52b9b00ad7a282d2e2e137713922"
RECAST_TARBALL="recastnavigation_1.6.0+dfsg.orig.tar.gz"
RECAST_SHA256="d2f4a25e6e21d72f0cea4dd45152c790ffb312d7675582126602242102bdfec7"

export DEBIAN_FRONTEND=noninteractive

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build ccache git pkg-config mold curl unzip ca-certificates \
    libboost-program-options-dev libboost-system-dev libboost-iostreams-dev libboost-filesystem-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
    libsdl2-dev libopenal-dev libbullet-dev liblz4-dev libpng-dev libjpeg-dev libluajit-5.1-dev \
    libsqlite3-dev libicu-dev libyaml-cpp-dev libopenscenegraph-dev libgl-dev libfreetype-dev \
    libgtest-dev libgmock-dev googletest \
    libunshield-dev qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools

mkdir -p "$SRC"

fetch() {
    local dir="$1" tarball="$2" sha256="$3"
    local file="$SRC/$tarball"
    # Drop an archive left by an interrupted or corrupted earlier run so it is downloaded again.
    if [ -f "$file" ] && ! echo "$sha256  $file" | sha256sum --check --status; then
        rm -f "$file"
    fi
    if [ ! -f "$file" ]; then
        curl -fsSL -o "$file.part" "$ARCHIVE/$dir/$tarball"
        echo "$sha256  $file.part" | sha256sum --check --status
        mv "$file.part" "$file"
    fi
}

if [ ! -f "$PREFIX/mygui/lib/pkgconfig/MYGUI.pc" ]; then
    fetch m/mygui "$MYGUI_TARBALL" "$MYGUI_SHA256"
    mkdir -p "$SRC/mygui"
    tar -xzf "$SRC/$MYGUI_TARBALL" -C "$SRC/mygui" --strip-components=1
    # Same options OpenMW uses when it builds MyGUI itself (extern/CMakeLists.txt).
    cmake -G Ninja -S "$SRC/mygui" -B "$SRC/mygui-build" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX/mygui" -DMYGUI_RENDERSYSTEM=4 -DMYGUI_DISABLE_PLUGINS=TRUE \
        -DMYGUI_BUILD_DEMOS=OFF -DMYGUI_BUILD_PLUGINS=OFF -DMYGUI_BUILD_TOOLS=OFF -DMYGUI_DONT_USE_OBSOLETE=ON \
        -DBUILD_SHARED_LIBS=ON
    cmake --build "$SRC/mygui-build" -j "$JOBS"
    cmake --install "$SRC/mygui-build"
fi

if [ ! -f "$PREFIX/recast/lib/cmake/recastnavigation/recastnavigation-config.cmake" ]; then
    fetch r/recastnavigation "$RECAST_TARBALL" "$RECAST_SHA256"
    mkdir -p "$SRC/recast"
    tar -xzf "$SRC/$RECAST_TARBALL" -C "$SRC/recast" --strip-components=1
    cmake -G Ninja -S "$SRC/recast" -B "$SRC/recast-build" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX/recast" -DRECASTNAVIGATION_DEMO=OFF -DRECASTNAVIGATION_TESTS=OFF \
        -DRECASTNAVIGATION_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    cmake --build "$SRC/recast-build" -j "$JOBS"
    cmake --install "$SRC/recast-build"
fi

cat <<EOF

Dependencies are ready. Configure and build from the repository root with:

  export PKG_CONFIG_PATH="$PREFIX/mygui/lib/pkgconfig"
  cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release \\
      -DCMAKE_PREFIX_PATH="$PREFIX/mygui;$PREFIX/recast" \\
      -DBUILD_LAUNCHER=ON -DBUILD_WIZARD=ON -DBUILD_OPENCS=ON -DBUILD_OPENCS_TESTS=ON \\
      -DBUILD_COMPONENTS_TESTS=ON -DBUILD_OPENFALLOUT_TESTS=ON \\
      -DOPENFALLOUT_USE_SYSTEM_RECASTNAVIGATION=ON -DOPENFALLOUT_USE_SYSTEM_GOOGLETEST=ON
  cmake --build build
  build/components-tests && build/openfallout-tests && build/openfallout-cs-tests
EOF
