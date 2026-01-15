#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BUILD_TYPE="Release"
CLEAN_FIRST=0
QT_DIR_OVERRIDE=""

usage() {
    cat <<'EOF'
Usage: scripts/compile.sh [--debug|--release] [--clean] [--qt-dir PATH]

Initialises vendored submodules, builds libp2p and compiles the Libp2p module plugin.
  --debug        Build with Debug configuration
  --release      Build with Release configuration (default)
  --clean        Remove the existing build directory and libp2p artifacts before configuring
  --qt-dir PATH  Explicit Qt installation to use (passed to CMake as CMAKE_PREFIX_PATH)
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
            if [[ $# -lt 2 ]]; then
                echo "--qt-dir requires a path argument" >&2
                exit 1
            fi
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

if [[ -z "${QT_DIR:-}" ]]; then
    if [[ -d "${HOME}/Qt" ]]; then
        qt_candidates=$(find "${HOME}/Qt" -mindepth 1 -maxdepth 1 -type d -name '6.*' 2>/dev/null | sort -Vr)
        if [[ -n "${qt_candidates}" ]]; then
            while IFS= read -r candidate; do
                [[ -z "${candidate}" ]] && continue
                for suffix in macos clang_64 gcc_64 win64_msvc2019_64 win64_msvc2017_64; do
                    if [[ -d "${candidate}/${suffix}" ]]; then
                        QT_DIR="${candidate}/${suffix}"
                        break 2
                    fi
                done
            done <<< "${qt_candidates}"
        fi
    fi
fi

if [[ -n "${QT_DIR:-}" ]]; then
    export QT_DIR
    if [[ -d "${QT_DIR}/lib/cmake/Qt6" ]]; then
        export Qt6_DIR="${QT_DIR}/lib/cmake/Qt6"
    elif [[ -d "${QT_DIR}/lib/cmake/Qt5" ]]; then
        export Qt5_DIR="${QT_DIR}/lib/cmake/Qt5"
    fi
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
    echo "Cleaning vendored generator build directory: ${ROOT_DIR}/vendor/build"
    rm -rf "${ROOT_DIR}/vendor/build"
    echo "Cleaning libp2p artifacts"
    rm -rf "${ROOT_DIR}/lib"
fi

# TODO: use or remove
# echo "Ensuring vendored dependencies are available..."
# git -C "${ROOT_DIR}" submodule update --init --recursive

echo "Building libp2p from vendored sources..."
if [[ ${CLEAN_FIRST} -eq 1 ]]; then
    "${ROOT_DIR}/build_libp2p.sh" --clean
else
    "${ROOT_DIR}/build_libp2p.sh"
fi

echo "Configuring (type=${BUILD_TYPE})..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DLOGOS_LIBP2P_MODULE_USE_VENDOR=ON \
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
