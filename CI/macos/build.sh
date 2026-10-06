#!/bin/bash
set -euxo pipefail

cd build

if [[ -n "${MACOS_AMD64:-}" ]]; then
    arch -x86_64 make -j "$(sysctl -n hw.logicalcpu)" "${1:-package}"
else
    make -j "$(sysctl -n hw.logicalcpu)" "${1:-package}"
fi
