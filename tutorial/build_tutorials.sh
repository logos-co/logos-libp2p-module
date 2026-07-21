#!/usr/bin/env bash
#
# build_tutorials.sh
#
# Builds all tutorial executables using the project's CMake build system.
# Should be invoked within the Nix development shell (nix develop) so that
# LOGOS_MODULE_BUILDER_ROOT, libp2p.so, and other dependencies are available.
#
# Usage:
#   nix develop --command ./tutorial/build_tutorials.sh
#   # or from inside the dev shell:
#   ./tutorial/build_tutorials.sh
#
# The script cleans only the tutorial targets so existing test/example builds
# remain intact.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_DIR}"

# Build directory (same as the project's standard cmake build)
BUILD_DIR="${PROJECT_DIR}/build"

# Ensure cmake is configured
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}"
fi

# Build only the tutorial targets
echo "Building tutorials..."
cmake --build "${BUILD_DIR}" --target \
    tutorial_1_node_lifecycle \
    tutorial_2_custom_config \
    tutorial_3_connecting_peers \
    tutorial_4_custom_protocol \
    tutorial_5_kademlia_basics \
    tutorial_6_kademlia_providers \
    tutorial_7_gossipsub \
    tutorial_8_service_discovery \
    tutorial_9_peerstore \
    tutorial_10_circuit_relay \
    -j "$(nproc 2>/dev/null || echo 4)"

echo ""
echo "✅ All tutorials compiled successfully."
echo ""
echo "Binaries in: ${BUILD_DIR}/"
ls -1 "${BUILD_DIR}"/tutorial_* 2>/dev/null
