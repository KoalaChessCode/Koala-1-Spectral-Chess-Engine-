# Koala 1 — Spectral Chess Engine

Copyright (C) 2026 KoalaChessCode  
Copyright (c) 2017 Anders Øen Fylling

An experimental chess engine exploring alternative approaches to chess AI.

## Project

Koala 1 is developed publicly as an ongoing experiment.

The project explores:

- FFT-based representations
- Graph Attention Networks (GATv2)
- PUCT Bayes search
- Experimental chess programming techniques

The goal is not simply to reproduce existing engines, but to explore a different way of representing and evaluating chess positions.

---

## Dependencies

Koala 1 requires a C++ host compiler, an LLVM/Clang GPU toolchain and AMDGPU OpenMP Offloading support.

### 1. Build Dependencies (Ubuntu / Linux Mint)

Install the standard development tools and LLVM packages:

```bash
sudo apt update

sudo apt install -y \
    git \
    build-essential \
    cmake \
    gcc \
    g++ \
    llvm-17 \
    llvm-17-dev \
    clang-17 \
    lld-17 \
    libomp-17-dev
```

**Compiler requirements:**

- GCC 12+ or Clang 15+ for host code.
- Clang 17+ for GPU kernel compilation.
- CMake 3.28 or newer.

### 2. AMD GPU Compute Dependencies

Koala 1 uses AMDGPU OpenMP Offloading.

The GPU toolchain must provide:

- Clang with `amdgcn-amd-amdhsa` target support.
- LLVM OpenMP Offloading runtime (`libomptarget`).
- OpenMP host runtime (`libomp`).
- AMDGPU device runtime and device libraries.
- ROCr HSA Runtime.
- ROCt / AMD KFD runtime support.

The HSA runtime and AMDGPU compute components must be compatible with the installed LLVM/Clang toolchain.

**Important:** Installing standard LLVM and Clang packages does not necessarily provide AMDGPU OpenMP Offloading. A compatible GPU-enabled LLVM toolchain is required.

### 3. GPU Driver

An AMDGPU-compatible Linux kernel driver is required to execute GPU kernels.

On Ubuntu and Linux Mint, the open-source `amdgpu` kernel driver is normally provided by the Linux kernel.

Verify driver availability:

```bash
lspci -k | grep -A 3 -E 'VGA|Display'
```

For AMDGPU compute, the system must also provide access to the HSA runtime and `/dev/kfd`.

A separate proprietary graphics driver installation is not normally required when the existing kernel driver already supports the GPU.

---

## Building

### 4. Clone Repository

```bash
git clone https://github.com/KoalaChessCode/Koala-1-Spectral-Chess-Engine-.git
cd Koala-1-Spectral-Chess-Engine-
```

### 5. Configure

Configure the project using CMake:

```bash
cmake -S . -B build
```

If GPU Clang is not detected automatically, specify its location and GPU architecture:

```bash
cmake -S . -B build \
    -DGPU_CLANGXX=/path/to/clang++ \
    -DGPU_ARCH=gfx803
```

`GPU_ARCH` defaults to `gfx803`.

### 6. Build

Compile Koala 1:

```bash
cmake --build build --parallel
```

CMake automatically:

- Builds the FFT data generator.
- Generates the FFT binary and C++ initializer.
- Compiles AMDGPU kernels.
- Compiles the host application.
- Links the final executable.

### 7. Run

Start the engine:

```bash
./build/Koala1
```

**Note:** The GPU compiler, OpenMP offloading runtime and AMDGPU device libraries must belong to a compatible toolchain installation.

---

## Development

The project is developed openly. New features, experiments and architectural changes will be published as development progresses.

Follow the repository to see the engine being built from the ground up.

## License

Koala 1 is released under the **GNU General Public License v3.0**.

See [`LICENSE`](LICENSE) for the full license text.

## Third-Party Software

This project includes the UCI library by Anders Fylling, licensed under the MIT License.

Repository: https://github.com/andersfylling/uci

## Status

**Near Completion / Experimental**

Koala 1 has reached a functional gameplay stage. The engine generates legal chess moves, completes full games and operates without freezing during gameplay.

The project is now approaching completion, while its experimental evaluation and search architecture continues to evolve.
