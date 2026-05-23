#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
CHECKPOINTS_DIR="$PROJECT_DIR/checkpoints"

echo "=== Talos V1 Build & Run ==="

# Detect phase
LATEST_STEP=0
if [ -d "$CHECKPOINTS_DIR" ]; then
    for d in "$CHECKPOINTS_DIR"/*/; do
        BASENAME=$(basename "$d")
        if [[ "$BASENAME" =~ ^[0-9]+$ ]]; then
            if [ "$BASENAME" -gt "$LATEST_STEP" ]; then
                LATEST_STEP=$BASENAME
            fi
        fi
    done
fi

if [ "$LATEST_STEP" -lt 30000000000 ]; then
    PHASE=1
elif [ "$LATEST_STEP" -lt 100000000000 ]; then
    PHASE=2
elif [ "$LATEST_STEP" -lt 220000000000 ]; then
    PHASE=3
elif [ "$LATEST_STEP" -lt 340000000000 ]; then
    PHASE=4
else
    PHASE=5
fi

echo "  Detected Phase: $PHASE (at step $LATEST_STEP)"
echo "  Building project..."

cd "$PROJECT_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo 2>&1 || { echo "ERROR: CMake configuration failed!"; exit 1; }
cmake --build . --config Release --target TalonBot 2>&1 || { echo "ERROR: Build failed!"; exit 1; }

echo "  Launching TalonBot (Phase $PHASE)..."
./TalonBot
