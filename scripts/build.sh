#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
CONFIG="${CONFIG:-Release}"
GENERATOR="${GENERATOR:-Unix Makefiles}"

"$ROOT/scripts/setup-linux.sh" >/dev/null

# Rootless Ubuntu dev headers and linker stubs live in the offline sysroot.
if [ -d "$ROOT/deps/sysroot/usr" ]; then
    export PKG_CONFIG_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$ROOT/deps/sysroot/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}"
    export PKG_CONFIG_SYSROOT_DIR="$ROOT/deps/sysroot"
    CMAKE_EXTRA_ARGS=(
        "-DCMAKE_INCLUDE_PATH=$ROOT/deps/sysroot/usr/include"
        "-DCMAKE_LIBRARY_PATH=$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu"
    )
else
    CMAKE_EXTRA_ARGS=()
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DAUTOWAH_BUILD_TESTS=ON \
    -DAUTOWAH_BUILD_SCREENSHOT=ON \
    -DAUTOWAH_BUILD_HOST_TEST=ON \
    "${CMAKE_EXTRA_ARGS[@]}"

if [ -z "${JOBS:-}" ]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS"

VST3="$BUILD_DIR/AutoWah_artefacts/$CONFIG/VST3/Auto-Wah.vst3"
STANDALONE="$BUILD_DIR/AutoWah_artefacts/$CONFIG/Standalone/Auto-Wah"

mkdir -p "$ROOT/dist"
if [ -d "$VST3" ]; then
    rm -rf "$ROOT/dist/Auto-Wah.vst3"
    cp -a "$VST3" "$ROOT/dist/"
fi
if [ -x "$STANDALONE" ]; then
    cp -a "$STANDALONE" "$ROOT/dist/Auto-Wah"
fi

echo
echo "Built artefacts:"
[ -d "$ROOT/dist/Auto-Wah.vst3" ] && echo "  VST3        $ROOT/dist/Auto-Wah.vst3"
[ -x "$ROOT/dist/Auto-Wah" ] && echo "  Standalone  $ROOT/dist/Auto-Wah"
echo "  Tests       $BUILD_DIR/AutoWahTests_artefacts/$CONFIG/AutoWahTests"
if [ -x "$BUILD_DIR/AutoWahHostSmokeTest_artefacts/$CONFIG/AutoWahHostSmokeTest" ]; then
    echo "  Host test   $BUILD_DIR/AutoWahHostSmokeTest_artefacts/$CONFIG/AutoWahHostSmokeTest"
fi
if [ -x "$BUILD_DIR/AutoWahScreenshot_artefacts/$CONFIG/Auto-Wah Screenshot" ]; then
    echo "  Screenshot  $BUILD_DIR/AutoWahScreenshot_artefacts/$CONFIG/Auto-Wah Screenshot"
fi
