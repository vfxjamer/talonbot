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
# Fix tcsetattr spam on Kaggle (non-TTY stdin) — skip terminal manipulation
python3 -c "
path = 'GigaLearnCPP/GigaLearnCPP/src/public/GigaLearnCPP/Util/KeyPressDetector.cpp'
with open(path) as f:
    c = f.read()
old = '''char GGL::KeyPressDetector::GetPressedChar() {
\t// https://stackoverflow.com/questions/421860/capture-characters-from-standard-input-without-waiting-for-enter-to-be-pressed
\tchar buf = 0;
\tstruct termios old = { 0 };
\tif (tcgetattr(0, &old) < 0)
\t\tperror(\"tcsetattr()\");
\told.c_lflag &= ~ICANON;
\told.c_lflag &= ~ECHO;
\told.c_cc[VMIN] = 1;
\told.c_cc[VTIME] = 0;
\tif (tcsetattr(0, TCSANOW, &old) < 0)
\t\tperror(\"tcsetattr ICANON\");
\tif (read(0, &buf, 1) < 0)
\t\tperror(\"read()\");
\told.c_lflag |= ICANON;
\told.c_lflag |= ECHO;
\tif (tcsetattr(0, TCSADRAIN, &old) < 0)
\t\tperror(\"tcsetattr ~ICANON\");
\treturn (buf);
}'''
new = '''char GGL::KeyPressDetector::GetPressedChar() {
\tchar buf = 0;
\tif (!isatty(STDIN_FILENO)) {
\t\tif (read(0, &buf, 1) < 0)
\t\t\tperror(\"read()\");
\t\treturn buf;
\t}
\tstruct termios old = { 0 };
\tif (tcgetattr(0, &old) < 0)
\t\tperror(\"tcsetattr()\");
\told.c_lflag &= ~ICANON;
\told.c_lflag &= ~ECHO;
\told.c_cc[VMIN] = 1;
\told.c_cc[VTIME] = 0;
\tif (tcsetattr(0, TCSANOW, &old) < 0)
\t\tperror(\"tcsetattr ICANON\");
\tif (read(0, &buf, 1) < 0)
\t\tperror(\"read()\");
\told.c_lflag |= ICANON;
\told.c_lflag |= ECHO;
\tif (tcsetattr(0, TCSADRAIN, &old) < 0)
\t\tperror(\"tcsetattr ~ICANON\");
\treturn (buf);
}'''
c = c.replace(old, new)
with open(path, 'w') as f:
    f.write(c)
print(f\"Patched {path} — isatty guard added\")
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

# ── Locate CUDA toolkit (Kaggle P100 has CUDA at /usr/local/cuda or /usr/local/cuda-12) ──
CUDA_PATH=""
for p in /usr/local/cuda /usr/local/cuda-12 /usr/local/cuda-12.4; do
    if [ -f "$p/bin/nvcc" ]; then
        CUDA_PATH="$p"
        break
    fi
done
if [ -z "$CUDA_PATH" ]; then
    echo "CUDA toolkit not found at standard paths. Installing via apt..."
    apt-get install -y -qq nvidia-cuda-toolkit 2>&1 | tail -3
    CUDA_PATH="/usr/local/cuda"
fi
echo "CUDA toolkit: $CUDA_PATH"
export PATH="$CUDA_PATH/bin:$PATH"

# ── Patch Caffe2's cuda.cmake: FindCUDA.cmake can't auto-detect ──
# on Kaggle even with CUDA_TOOLKIT_ROOT_DIR set. We patch it to succeed.
CUDA_CMAKE="/usr/local/lib/python3.12/dist-packages/torch/share/cmake/Caffe2/public/cuda.cmake"
if [ -f "$CUDA_CMAKE" ]; then
    python3 << 'PYEOF'
import re
path = "/usr/local/lib/python3.12/dist-packages/torch/share/cmake/Caffe2/public/cuda.cmake"
cuda_root = "/usr/local/cuda"
with open(path) as f:
    c = f.read()
# Make find_package(CUDA) quiet so it doesn't abort
c = c.replace("find_package(CUDA)", "find_package(CUDA QUIET)")
# Replace the 'if not found' block to set variables from our known paths
old_block = """if(NOT CUDA_FOUND)
  message(WARNING
    "Caffe2: CUDA cannot be found. Depending on whether you are building "
    "Caffe2 or a Caffe2 dependent library, the next warning / error will "
    "give you more info.")
  set(CAFFE2_USE_CUDA OFF)
  return()
endif()"""
new_block = f"""if(NOT CUDA_FOUND)
  set(CUDA_FOUND TRUE)
  set(CUDA_TOOLKIT_ROOT_DIR "{cuda_root}")
  set(CUDA_NVCC_EXECUTABLE "{cuda_root}/bin/nvcc")
  set(CUDA_INCLUDE_DIRS "{cuda_root}/include")
  set(CUDA_CUDART_LIBRARY "{cuda_root}/lib64/libcudart.so")
  set(CAFFE2_USE_CUDA ON)
  message(STATUS "Kaggle patch: CUDA manually set from {cuda_root}")
endif()"""
c = c.replace(old_block, new_block)
with open(path, 'w') as f:
    f.write(c)
print(f"Patched {path}")
PYEOF
fi

# Kaggle's CMake 3.31 tries CUDA20 dialect during GPU arch detection,
# but CUDA 11.5 (P100) doesn't support it. Skip detection, set sm_60 manually.
export TORCH_CUDA_ARCH_LIST="6.0"

cmake .. \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DTorch_DIR="$TORCH_DIR" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_CUDA_ARCHITECTURES=60 \
    -DCMAKE_CUDA_STANDARD=17 \
    -DCUDA_TOOLKIT_ROOT_DIR="$CUDA_PATH" \
    -DCUDAToolkit_ROOT="$CUDA_PATH" \
    -DCUDA_NVCC_EXECUTABLE="$CUDA_PATH/bin/nvcc" \
    -DCUDA_INCLUDE_DIRS="$CUDA_PATH/include" \
    -DCUDA_CUDART_LIBRARY="$CUDA_PATH/lib64/libcudart.so" \
    2>&1
cmake --build . --config Release --target GigaLearnCPP -j$(nproc) 2>&1

echo "=== Setup complete ==="
echo "GigaLearnCPP: /workspace/libs/GigaLearnCPP"
echo "Meshes:       /workspace/libs/collision_meshes"
echo "Set GIGALEARN_ROOT=/workspace/libs/GigaLearnCPP before building TalonBot"
