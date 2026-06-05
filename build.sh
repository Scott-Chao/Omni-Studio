#!/usr/bin/env bash
#
# build.sh — CMake build script for OmniStudio
#
# Usage:
#   ./build.sh              # release build (default on Linux)
#   ./build.sh debug        # debug build
#   ./build.sh release      # release build
#   ./build.sh clean        # remove build directory
#   ./build.sh rebuild      # clean + build

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_TYPE="${1:-auto}"

# ── Platform detection ──────────────────────────────────────────────────
case "$(uname -s)" in
    Linux*)  PLATFORM="linux" ;;
    Darwin*) PLATFORM="macos" ;;
    *)       echo "Unknown platform: $(uname -s)"; exit 1 ;;
esac

# ── Help / clean / rebuild ─────────────────────────────────────────────
if [ "$BUILD_TYPE" = "-h" ] || [ "$BUILD_TYPE" = "--help" ]; then
    sed -n '/^# Usage:/,/^$/p' "$0"
    exit 0
fi

if [ "$BUILD_TYPE" = "clean" ]; then
    echo "Removing build directory..."
    rm -rf "$ROOT_DIR/build"
    echo "Done."
    exit 0
fi

if [ "$BUILD_TYPE" = "rebuild" ]; then
    echo "Clean rebuild..."
    rm -rf "$ROOT_DIR/build"
    BUILD_TYPE="auto"
fi

# ── Determine build config ─────────────────────────────────────────────
case "$BUILD_TYPE" in
    auto|release) CMAKE_BUILD="release" ;;
    debug)        CMAKE_BUILD="debug" ;;
    *)
        echo "Unknown option: $BUILD_TYPE"
        echo "Usage: $0 [debug|release|clean|rebuild]"
        exit 1
        ;;
esac

PRESET="${PLATFORM}-${CMAKE_BUILD}"
BUILD_DIR="$ROOT_DIR/build/${CMAKE_BUILD}"

echo "══════════════════════════════════════════════"
echo "  Config   : ${CMAKE_BUILD}"
echo "  Preset   : ${PRESET}"
echo "  Build dir: ${BUILD_DIR}"
echo "══════════════════════════════════════════════"

# ── Install deps hint (Linux) ──────────────────────────────────────────
if [ "$PLATFORM" = "linux" ]; then
    REQUIRED_PKGS=(
        qt6-base-dev
        qt6-webengine-dev
        qt6-pdf-dev
        qt6-svg-dev
        cmake
        g++
    )
    MISSING=()
    for pkg in "${REQUIRED_PKGS[@]}"; do
        if ! dpkg -s "$pkg" &>/dev/null 2>&1; then
            MISSING+=("$pkg")
        fi
    done
    if [ ${#MISSING[@]} -gt 0 ]; then
        echo ""
        echo "Missing system packages: ${MISSING[*]}"
        echo "Install with:"
        echo "  sudo apt install ${MISSING[*]}"
        echo ""
    fi
fi

# ── Platform-specific CMake args ──────────────────────────────────────
CMAKE_ARGS=()
if [ "$PLATFORM" = "macos" ]; then
    # Homebrew installs Qt as keg-only — CMake can't find it without the prefix
    QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
    if [ -n "$QT_PREFIX" ]; then
        CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_PREFIX")
    fi
fi

# ── CMake configure ────────────────────────────────────────────────────
echo ""
echo ">> Configuring..."
cmake --preset "$PRESET" "${CMAKE_ARGS[@]}" "$ROOT_DIR"

# ── CMake build ─────────────────────────────────────────────────────────
echo ""
echo ">> Building..."
JOBS_ARG=""
if command -v nproc &>/dev/null; then
    JOBS_ARG="-- -j$(nproc)"
fi
cmake --build "$BUILD_DIR" $JOBS_ARG

# ── Done ────────────────────────────────────────────────────────────────
echo ""
echo "Build successful!"
echo "  Binary: ${BUILD_DIR}/OmniStudio"
