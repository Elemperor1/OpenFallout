#!/bin/bash -e

# Downloads the dependency manifest for this runner's architecture to $1 (default /tmp/openmw-manifest.txt).
# The manifest is served from the main branch of openmw-deps, so the workflow hashes it into the dependency cache key.

source ./CI/macos/deps_versions.sh

if [[ "${MACOS_AMD64}" ]]; then
    VCPKG_FILE="vcpkg-x64-osx-dynamic"
else
    VCPKG_FILE="vcpkg-arm64-osx-dynamic"
fi

curl --fail --retry 3 -sSL "https://gitlab.com/OpenMW/openmw-deps/-/raw/main/macos/${VCPKG_FILE}-${VCPKG_TAG}-manifest.txt" -o "${1:-/tmp/openmw-manifest.txt}"
