#!/usr/bin/env bash
#
# build_tutorials.sh
#
# Builds all tutorial executables using the project's CMake build system.
# Should be invoked within the Nix development shell (nix develop) so that
# LOGOS_MODULE_BUILDER_ROOT, libp2p.so, and other dependencies are available.
#
# Usage:
#   nix --extra-experimental-features 'nix-command flakes' develop --command ./tutorial/build_tutorials.sh
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

# Ensure cmake is configured. A previous failed configure can leave a
# CMakeCache.txt without a generated build system.
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] || [ ! -f "${BUILD_DIR}/Makefile" ]; then
    echo "Configuring CMake..."
    cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}"
else
    cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n1)"
    cached_libp2p="$(sed -n 's/^LIBP2P_INCLUDE_DIR:[^=]*=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n1)"
    if [[ "${cached_cxx}" == /usr/bin/* && "${cached_libp2p}" == /nix/store/* ]]; then
        cat >&2 <<EOF
The existing CMake build uses the host compiler (${cached_cxx}) with Nix libp2p.
That can produce tutorial binaries that load the host dynamic linker with Nix
glibc and abort before main with "stack smashing detected".

Recreate the build directory from inside the Nix development shell, then rerun:
  rm -rf "${BUILD_DIR}"
  nix --extra-experimental-features 'nix-command flakes' develop --command ./tutorial/build_tutorials.sh
EOF
        exit 1
    fi
fi

# Discover and build only the tutorial targets. CMake target names match the
# tutorial source basenames, e.g. tutorial_1_node_lifecycle.cpp.
echo "Building tutorials..."
tutorial_targets=()
for tutorial_src in "${SCRIPT_DIR}"/tutorial_*.cpp; do
    [ -f "${tutorial_src}" ] || continue
    tutorial_file="$(basename "${tutorial_src}")"
    tutorial_targets+=("${tutorial_file%.cpp}")
done

if [ "${#tutorial_targets[@]}" -eq 0 ]; then
    echo "No tutorial sources found in ${SCRIPT_DIR}" >&2
    exit 1
fi

cmake --build "${BUILD_DIR}" --target "${tutorial_targets[@]}" \
    -j "$(nproc 2>/dev/null || echo 4)"

echo ""
echo "✅ All tutorials compiled successfully."
echo ""
echo "Binaries in: ${BUILD_DIR}/tutorial/"
ls -1 "${BUILD_DIR}"/tutorial/tutorial_*
