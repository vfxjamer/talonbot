#!/usr/bin/env bash
set -e

echo "=== Talos V1 System Setup ==="

apt-get update -qq
apt-get install -y -qq \
    cmake git gcc g++ python3 python3-pip build-essential wget curl unzip bc \
    2>&1 | tail -3

mkdir -p /workspace/libs
cd /workspace/libs

# ── Clone/refresh GigaLearnCPP ─────────────────────────────
rm -rf GigaLearnCPP
echo "Cloning GigaLearnCPP..."
git clone --recurse-submodules https://github.com/ZealanL/GigaLearnCPP-Leak.git GigaLearnCPP
# Fix case-sensitive include paths (Kaggle Linux filesystem is case-sensitive)
sed -i 's|#include "../Framework.h"|#include "Framework.h"|' \
    GigaLearnCPP/GigaLearnCPP/RLGymCPP/src/RLGymCPP/CommonValues.h 2>/dev/null || true
sed -i 's|#include "../OBSBuilders/OBSBuilder.h"|#include "../ObsBuilders/ObsBuilder.h"|' \
    GigaLearnCPP/GigaLearnCPP/RLGymCPP/src/RLGymCPP/EnvSet/EnvSet.h 2>/dev/null || true
# Fix C++20: high_resolution_clock != steady_clock on GCC
sed -i 's|std::chrono::high_resolution_clock|std::chrono::steady_clock|g' \
    GigaLearnCPP/GigaLearnCPP/src/public/GigaLearnCPP/Util/Timer.h 2>/dev/null || true
# Fix case-sensitive OBSBuilders -> ObsBuilders (Linux)
ln -sfn ObsBuilders GigaLearnCPP/RLGymCPP/src/RLGymCPP/OBSBuilders 2>/dev/null || true
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

# ── Install CUDA 12.4 PyTorch with P100 (sm_60) support ──
echo "Reinstalling PyTorch with CUDA 12.4 (P100 sm_60 kernels)..."
pip install --force-reinstall torch==2.5.1 --index-url https://download.pytorch.org/whl/cu124 2>&1 | tail -5
if ! python3 -c "import torch; print(torch.__version__)" 2>/dev/null; then
    echo "pip reinstall failed. Falling back to libtorch download..."
    URL="https://download.pytorch.org/libtorch/cu124/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcu124.zip"
    wget -q --show-progress "$URL" -O /tmp/libt.zip
    unzip -q /tmp/libt.zip -d /workspace/libs/
    rm /tmp/libt.zip
    TORCH_DIR="/workspace/libs/libtorch/share/cmake/Torch"
    echo "Fallback Torch_DIR: $TORCH_DIR"
else
    TORCH_DIR="$(python3 -c "import torch; print(torch.utils.cmake_prefix_path)")/Torch"
    echo "Torch_DIR: $TORCH_DIR"
fi
# Refresh linker cache so the system knows about the new torch libs
ldconfig 2>/dev/null || true

# ── Collision meshes (bundled in repo) ──────────────────────
if [ -d "/kaggle/working/talonbot/collision_meshes" ]; then
    mkdir -p /workspace/libs
    cp -r /kaggle/working/talonbot/collision_meshes /workspace/libs/
fi

# ── Build GigaLearnCPP ──────────────────────────────────────
cd /workspace/libs/GigaLearnCPP
# Add -fPIC to RocketSim target (must link PIC into shared lib)
printf '\nset_target_properties(RocketSim PROPERTIES POSITION_INDEPENDENT_CODE ON)\n' \
    >> GigaLearnCPP/RLGymCPP/RocketSim/CMakeLists.txt
# Force GigaLearnCPP to STATIC so RocketSim::Init (and all RocketSim symbols)
# are available when linking TalonBot. SHARED drops object files not referenced
# by GigaLearnCPP/RLGymCPP, but RocketSim::Init is only called from main.cpp.
sed -i 's/add_library(GigaLearnCPP SHARED/add_library(GigaLearnCPP STATIC/' \
    GigaLearnCPP/CMakeLists.txt 2>/dev/null || true
rm -rf build
mkdir build
cd build
export PATH="/usr/local/cuda/bin:$PATH"
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTorch_DIR="$TORCH_DIR" -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCUDAToolkit_ROOT=/usr/local/cuda 2>&1
cmake --build . --config Release --target GigaLearnCPP -j$(nproc) 2>&1

echo "=== Setup complete ==="
echo "GigaLearnCPP: /workspace/libs/GigaLearnCPP"
echo "Meshes:       /workspace/libs/collision_meshes"
echo "Set GIGALEARN_ROOT=/workspace/libs/GigaLearnCPP before building TalonBot"
