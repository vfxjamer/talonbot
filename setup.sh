#!/usr/bin/env bash
set -e

echo "=== Talos V1 System Setup ==="

apt-get update -qq
apt-get install -y -qq \
    cmake \
    git \
    gcc \
    g++ \
    python3 \
    python3-pip \
    build-essential \
    wget \
    curl \
    unzip \
    bc \
    2>&1 | tail -3

mkdir -p /workspace/libs
cd /workspace/libs

if [ ! -d "GigaLearnCPP" ]; then
    echo "Cloning GigaLearnCPP..."
    git clone --recurse-submodules https://github.com/ZealanL/GigaLearnCPP-Leak.git GigaLearnCPP
fi

cd /workspace/libs

if [ ! -d "GigaLearnCPP/libtorch" ]; then
    echo "Downloading libtorch..."
    if command -v nvidia-smi &> /dev/null; then
        TORCH_URL="https://download.pytorch.org/libtorch/cu118/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcu118.zip"
    else
        TORCH_URL="https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcpu.zip"
    fi
    wget -q "$TORCH_URL" -O libtorch.zip || {
        echo "Failed to download libtorch. Place it manually in GigaLearnCPP/libtorch/"
        exit 1
    }
    unzip -q libtorch.zip -d GigaLearnCPP/
    rm libtorch.zip
    echo "libtorch extracted."
fi

cd /workspace/libs
if [ ! -d "collision_meshes" ]; then
    echo "Fetching collision meshes..."
    git clone --depth 1 https://github.com/ZealanL/RLArenaCollisionDumper.git _mesh 2>/dev/null || true
    if [ -d "_mesh/collision_meshes" ]; then
        cp -r _mesh/collision_meshes .
        rm -rf _mesh
        echo "Collision meshes obtained."
    else
        mkdir -p collision_meshes
        rm -rf _mesh
        echo "WARNING: Could not fetch collision meshes. Place manually in /workspace/libs/collision_meshes/"
    fi
fi

cd /workspace/libs/GigaLearnCPP
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo 2>&1 | tail -5
cmake --build . --config Release --target GigaLearnCPP -j$(nproc) 2>&1 | tail -10

echo "=== Setup complete ==="
echo "GigaLearnCPP: /workspace/libs/GigaLearnCPP"
echo "Meshes:       /workspace/libs/collision_meshes"
echo "Set GIGALEARN_ROOT=/workspace/libs/GigaLearnCPP before building TalonBot"
