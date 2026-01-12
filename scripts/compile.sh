#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BUILD_TYPE="Release"
CLEAN_FIRST=0
QT_DIR_OVERRIDE=""

usage() {
    cat <<'EOF'
Usage: scripts/compile.sh [--debug|--release] [--clean] [--qt-dir PATH]

Builds nim-libp2p via nimble and compiles the Qt libp2p module plugin.

  --debug        Build with Debug configuration
  --release      Build with Release configuration (default)
  --clean        Remove build directory and nimble artifacts
  --qt-dir PATH  Explicit Qt installation to use
  -h, --help     Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --clean)
            CLEAN_FIRST=1
            shift
            ;;
        --qt-dir)
            QT_DIR_OVERRIDE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -n "${QT_DIR_OVERRIDE}" ]]; then
    QT_DIR="${QT_DIR_OVERRIDE}"
fi

if [[ -n "${QT_DIR:-}" ]]; then
    export QT_DIR
    export CMAKE_PREFIX_PATH="${QT_DIR}"
    echo "Using Qt from: ${QT_DIR}"
    CMAKE_EXTRA_ARGS=("-DCMAKE_PREFIX_PATH=${QT_DIR}")
else
    CMAKE_EXTRA_ARGS=()
    echo "Warning: QT_DIR not set; relying on system Qt lookup" >&2
fi

if [[ ${CLEAN_FIRST} -eq 1 ]]; then
    echo "Cleaning build directory: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"

    echo "Cleaning nimble artifacts..."
    nimble clean || true

    echo "Removing generated libp2p artifacts..."
    rm -rf "${ROOT_DIR}/lib"
fi

echo "Building libp2p via nimble..."

if [[ "${BUILD_TYPE}" == "Debug" ]]; then
    nimble build -d:debug
else
    nimble build
fi

echo "Configuring Qt plugin (type=${BUILD_TYPE})..."

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DLIBP2P_USE_SYSTEM=OFF \
    -DLIBP2P_ROOT="${ROOT_DIR}" \
    "${CMAKE_EXTRA_ARGS[@]}"

echo "Building libp2p_module_plugin..."
cmake --build "${BUILD_DIR}" --target libp2p_module_plugin --config "${BUILD_TYPE}"

uname_s="$(uname -s)"
if [[ "${uname_s}" == "Darwin" ]]; then
    LIB_SUFFIX=.dylib
elif [[ "${uname_s}" == MINGW* || "${uname_s}" == MSYS* ]]; then
    LIB_SUFFIX=.dll
else
    LIB_SUFFIX=.so
fi

BIN_PATH="${BUILD_DIR}/modules/libp2p_module_plugin${LIB_SUFFIX}"
if [[ -f "${BIN_PATH}" ]]; then
    echo "Build succeeded: ${BIN_PATH}"
else
    echo "Build finished. Check ${BUILD_DIR}/modules for artifacts."
fi

