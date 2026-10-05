#!/bin/bash -e

DEPS_DIR="/tmp"

source ./CI/macos/deps_versions.sh

# GitHub runner images already provide these tools. Upgrading curl on Intel
# macOS 26 builds curl and its dependencies from source and adds many minutes.
export HOMEBREW_NO_AUTO_UPDATE=1
command -v curl >/dev/null 2>&1 || brew install curl
command -v 7z >/dev/null 2>&1 || brew install p7zip

pip install aqtinstall
aqt install-qt -O /tmp/Qt mac desktop $QT_VER && rm aqtinstall.log

./CI/macos/fetch_deps_manifest.sh "$DEPS_DIR/openmw-manifest.txt"

{ read -r URL && read -r HASH FILE; } < $DEPS_DIR/openmw-manifest.txt

curl -fSL -R -J $URL -o $DEPS_DIR/$FILE
echo "${HASH:?}  ${FILE:?}" | sha512sum
7z x -y -o$DEPS_DIR/openmw-deps-pre $DEPS_DIR/$FILE && \
    mv $DEPS_DIR/openmw-deps-pre/*/ $DEPS_DIR/openmw-deps/ && \
    rmdir $DEPS_DIR/openmw-deps-pre

command -v cmake >/dev/null 2>&1 || brew install cmake
