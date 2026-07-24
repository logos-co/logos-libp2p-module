#!/usr/bin/env bash
#
# build_tutorials.sh
#
# Builds all tutorial executables using the project's CMake build system.
# Should be invoked within the Nix development shell (nix develop) so that
# LOGOS_MODULE_BUILDER_ROOT, libp2p.so, and other dependencies are available.
#
# The script cleans only the tutorial targets so that other builds remain intact.
#

set -euo pipefail

if [[ -z "${IN_NIX_SHELL:-}" ]]; then
    cat >&2 <<EOF
This script must be run inside the Nix development shell.

Run:
  nix --extra-experimental-features 'nix-command flakes' develop --command ./tutorial/build_tutorials.sh
EOF
    exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." &>/dev/null && pwd)"

cd "${PROJECT_DIR}"

# Build directory (same as the project's standard cmake build)
BUILD_DIR="${PROJECT_DIR}/build"

# Ensure cmake is configured. A previous failed configure can leave a
# CMakeCache.txt without a generated build system.
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]] || [[ ! -f "${BUILD_DIR}/Makefile" ]]; then
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

    # Refresh the generated build system before building tutorial targets.
    # New tutorial_*.cpp files create new CMake targets, and target-specific
    # builds fail with "No rule to make target" until CMake regenerates.
    echo "Refreshing CMake..."
    cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}"
fi

# Discover and build only the tutorial targets. CMake target names match the
# tutorial source basenames, e.g. tutorial_1_node_lifecycle.cpp.
echo "Building tutorials..."
tutorial_targets=()
for tutorial_src in "${SCRIPT_DIR}"/tutorial_*.cpp; do
    [[ -f "${tutorial_src}" ]] || continue
    tutorial_file="$(basename "${tutorial_src}")"
    tutorial_targets+=("${tutorial_file%.cpp}")
done

if [[ "${#tutorial_targets[@]}" -eq 0 ]]; then
    echo "No tutorial sources found in ${SCRIPT_DIR}" >&2
    exit 1
fi

build_jobs="$(nproc 2>/dev/null || echo 4)"
for tutorial_target in "${tutorial_targets[@]}"; do
    cmake --build "${BUILD_DIR}" --target "${tutorial_target}" -j "${build_jobs}"
done

if [[ "$(uname -s)" == "Darwin" ]]; then
    plugin_dylib="${BUILD_DIR}/modules/libp2p_module_plugin.dylib"
    libp2p_dylib="${BUILD_DIR}/modules/libp2p.dylib"

    if [[ ! -f "${plugin_dylib}" ]]; then
        echo "Expected plugin dylib not found: ${plugin_dylib}" >&2
        exit 1
    fi
    if [[ ! -f "${libp2p_dylib}" ]]; then
        echo "Expected libp2p dylib not found: ${libp2p_dylib}" >&2
        exit 1
    fi

    libp2p_install_name="$(otool -L "${plugin_dylib}" \
        | awk '/libp2p[.]dylib/ { print $1; exit }')"

    if [[ -z "${libp2p_install_name}" ]]; then
        echo "Could not find libp2p.dylib dependency in ${plugin_dylib}" >&2
        exit 1
    fi

    if [[ "${libp2p_install_name}" != "@loader_path/libp2p.dylib" ]]; then
        echo "Patching ${plugin_dylib} libp2p.dylib dependency"
        install_name_tool -change \
            "${libp2p_install_name}" \
            "@loader_path/libp2p.dylib" \
            "${plugin_dylib}"
    fi
fi

echo ""
echo "✅ All tutorials compiled successfully."
echo ""
echo "Binaries in: ${BUILD_DIR}/tutorial/"
ls -1 "${BUILD_DIR}"/tutorial/tutorial_*
