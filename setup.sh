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
# Fix case-sensitive include paths (Kaggle Linux filesystem is case-sensitive)
sed -i 's|#include "../Framework.h"|#include "Framework.h"|' \
    GigaLearnCPP/GigaLearnCPP/RLGymCPP/src/RLGymCPP/CommonValues.h 2>/dev/null || true
sed -i 's|#include "../OBSBuilders/OBSBuilder.h"|#include "../ObsBuilders/ObsBuilder.h"|' \
    GigaLearnCPP/GigaLearnCPP/RLGymCPP/src/RLGymCPP/EnvSet/EnvSet.h 2>/dev/null || true
# Fix C++20: high_resolution_clock != steady_clock on GCC
sed -i 's|std::chrono::high_resolution_clock|std::chrono::steady_clock|g' \
    GigaLearnCPP/GigaLearnCPP/src/public/GigaLearnCPP/Util/Timer.h 2>/dev/null || true
# Fix C++20: std::iterator removed + invalid typename usage
python3 -c "
import sys
path = 'GigaLearnCPP/GigaLearnCPP/src/private/GigaLearnCPP/Util/Models.h'
with open(path) as f:
    c = f.read()
c = c.replace(
    'class ModelIterator : public std::iterator<std::forward_iterator_tag, typename Model*> {',
    'class ModelIterator {\n\t\tpublic:\n\t\t\tusing iterator_category = std::forward_iterator_tag;\n\t\t\tusing value_type = Model*;\n\t\t\tusing difference_type = std::ptrdiff_t;\n\t\t\tusing pointer = Model**;\n\t\t\tusing reference = Model*&;'
)
c = c.replace('typename Model*&', 'Model*&')
with open(path, 'w') as f:
    f.write(c)
" 2>/dev/null || true
# Fix -fPIC: RocketSim target needs explicit POSITION_INDEPENDENT_CODE
ROCKET_CMAKE="GigaLearnCPP/GigaLearnCPP/RLGymCPP/RocketSim/CMakeLists.txt"
if ! grep -q "POSITION_INDEPENDENT_CODE" "$ROCKET_CMAKE" 2>/dev/null; then
    echo 'set_target_properties(RocketSim PROPERTIES POSITION_INDEPENDENT_CODE ON)' >> "$ROCKET_CMAKE"
fi

# ── Find Torch cmake path ──────────────────────────────────
TORCH_DIR=$(python3 -c "
import torch, os
d = os.path.dirname(torch.__file__)
p = os.path.join(d, 'share', 'cmake', 'Torch')
if os.path.isfile(os.path.join(p, 'TorchConfig.cmake')):
    print(p)
else:
    print('')
" 2>/dev/null || echo "")

if [ -z "$TORCH_DIR" ]; then
    echo "Downloading libtorch (~2GB)..."
    if command -v nvidia-smi &>/dev/null; then
        URL="https://download.pytorch.org/libtorch/cu124/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcu124.zip"
    else
        URL="https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcpu.zip"
    fi
    apt-get install -y -qq wget unzip 2>/dev/null
    wget -q --show-progress "$URL" -O /tmp/libt.zip
    unzip -q /tmp/libt.zip -d /workspace/libs/GigaLearnCPP/
    rm /tmp/libt.zip
    TORCH_DIR="/workspace/libs/GigaLearnCPP/libtorch/share/cmake/Torch"
fi

echo "Torch_DIR: $TORCH_DIR"

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
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DTorch_DIR="$TORCH_DIR" 2>&1
cmake --build . --config Release --target GigaLearnCPP -j$(nproc) 2>&1

echo "=== Setup complete ==="
echo "GigaLearnCPP: /workspace/libs/GigaLearnCPP"
echo "Meshes:       /workspace/libs/collision_meshes"
echo "Set GIGALEARN_ROOT=/workspace/libs/GigaLearnCPP before building TalonBot"
