#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

cd "$SCRIPT_DIR"

# Configure if the build directory doesn't exist yet
if [ ! -d "$BUILD_DIR" ]; then
    echo "==> Configuring…"
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi

echo "==> Building…"
cmake --build build -j"$(sysctl -n hw.logicalcpu)"

echo ""
echo "Done. Run with:"
echo "  open ./build/story-teller.app"
echo "  # or directly:"
echo "  ./build/story-teller.app/Contents/MacOS/story-teller [file.md|file.html]"
