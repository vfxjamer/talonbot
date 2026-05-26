#!/usr/bin/env bash
set -e

echo "=== Talos V1 System Setup ==="

apt-get update -qq
apt-get install -y -qq \
    cmake git gcc g++ python3 python3-pip build-essential wget curl unzip bc \
    2>&1 | tail -3

mkdir -p /workspace/libs
cd /workspace/libs

# ── Clone GigaLearnCPP ──────────────────────────────────────
if [ ! -d "GigaLearnCPP" ]; then
    echo "Cloning GigaLearnCPP..."
    git clone --recurse-submodules https://github.com/ZealanL/GigaLearnCPP-Leak.git GigaLearnCPP
fi
# Fix wrong relative include in CommonValues.h (Framework.h is in same dir)
sed -i 's|#include "../Framework.h"|#include "Framework.h"|' \
    GigaLearnCPP/GigaLearnCPP/RLGymCPP/src/RLGymCPP/CommonValues.h 2>/dev/null || true

# ── Find or download libtorch ───────────────────────────────
TORCH_TARGET="/workspace/libs/GigaLearnCPP/libtorch"

if [ ! -d "$TORCH_TARGET" ]; then
    echo "Looking for system PyTorch (Kaggle has it)..."

    # Detect system torch from Python
    SYS_TORCH=$(python3 -c "
import torch, os
d = os.path.dirname(torch.__file__)
print(os.path.join(d, 'share', 'cmake', 'Torch'))
" 2>/dev/null || echo "")

    if [ -n "$SYS_TORCH" ] && [ -f "$SYS_TORCH/TorchConfig.cmake" ]; then
        echo "Found system torch at $SYS_TORCH — symlinking..."
        SYS_TORCH_ROOT=$(python3 -c "
import torch, os
print(os.path.dirname(torch.__file__))
")
        ln -sfn "$SYS_TORCH_ROOT" "$TORCH_TARGET"
        echo "Symlinked: $SYS_TORCH_ROOT → $TORCH_TARGET"
    else
        echo "Downloading libtorch (~2GB)..."
        if command -v nvidia-smi &>/dev/null; then
            URL="https://download.pytorch.org/libtorch/cu124/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcu124.zip"
        else
            URL="https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcpu.zip"
        fi
        wget -q --show-progress "$URL" -O /tmp/libt.zip
        unzip -q /tmp/libt.zip -d /workspace/libs/GigaLearnCPP/
        rm /tmp/libt.zip
        echo "libtorch downloaded and extracted."
    fi
fi

# Verify
if [ ! -f "$TORCH_TARGET/share/cmake/Torch/TorchConfig.cmake" ] && [ ! -f "$TORCH_TARGET/TorchConfig.cmake" ]; then
    echo "ERROR: libtorch not found at $TORCH_TARGET"
    ls "$TORCH_TARGET" 2>/dev/null || echo "(directory missing)"
    exit 1
fi

# ── Collision meshes ────────────────────────────────────────
cd /workspace/libs
if [ ! -d "collision_meshes" ]; then
    echo "Fetching collision meshes..."
    git clone --depth 1 https://github.com/ZealanL/RLArenaCollisionDumper.git _mesh 2>/dev/null || true
    if [ -d "_mesh/collision_meshes" ]; then
        cp -r _mesh/collision_meshes .
        rm -rf _mesh
    else
        mkdir -p collision_meshes
        rm -rf _mesh
        echo "WARNING: collision meshes unavailable — place manually in /workspace/libs/collision_meshes/"
    fi
fi

# ── Build GigaLearnCPP ──────────────────────────────────────
cd /workspace/libs/GigaLearnCPP
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo 2>&1 | tail -5
cmake --build . --config Release --target GigaLearnCPP -j$(nproc) 2>&1 | tail -10

echo "=== Setup complete ==="
echo "GigaLearnCPP: /workspace/libs/GigaLearnCPP"
echo "Meshes:       /workspace/libs/collision_meshes"
echo "Set GIGALEARN_ROOT=/workspace/libs/GigaLearnCPP before building TalonBot"
