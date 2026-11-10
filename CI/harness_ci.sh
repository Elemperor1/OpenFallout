#!/usr/bin/env bash
# Run inside the Ubuntu 24.04 build container mounted at /harness.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
export CCACHE_DIR="${CCACHE_DIR:-/harness/.ccache}"
cat /etc/os-release
# archive.ubuntu.com has been unreachable from the Harness VMs while security.ubuntu.com was not: take another mirror
# for the whole run when the default one does not answer (the same sources serve CI/install_debian_deps.sh below)
source CI/apt_mirrors.sh
apt_update_with_fallback
apt-get install -y --no-install-recommends python3 software-properties-common ca-certificates
bash CI/install_debian_deps.sh gcc openmw-deps openmw-deps-dynamic
apt-get install -y --no-install-recommends qt6-l10n-tools
ccache --max-size=2G
cmake -S . -B build \
  -D CMAKE_BUILD_TYPE=Debug \
  -D CMAKE_C_COMPILER_LAUNCHER=ccache \
  -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -D OPENFALLOUT_USE_SYSTEM_RECASTNAVIGATION=ON \
  -D USE_SYSTEM_TINYXML=ON \
  -D BUILD_COMPONENTS_TESTS=ON \
  -D BUILD_OPENFALLOUT_TESTS=ON \
  -D BUILD_OPENCS_TESTS=ON \
  -D CMAKE_INSTALL_PREFIX=install
cmake --build build --parallel 8
ccache --show-stats
mkdir -p test-results
result=0
for suite in components-tests openfallout-tests openfallout-cs-tests; do
  "./build/$suite" --gtest_output="xml:test-results/$suite.xml" || result=1
done
exit "$result"
