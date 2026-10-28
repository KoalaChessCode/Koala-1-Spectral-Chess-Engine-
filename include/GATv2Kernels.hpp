#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

struct GATv2GpuContext;

// ============================================================
// create: takes an ARRAY of per-head pointers.
//
//   W_heads_src[m] -> block [D][K] (HIDDEN_DIM × K floats)
//   W_heads_tgt[m] -> block [D][K]
//
// create() CONCATENATES these into a single contiguous buffer [H][D][K] on the GPU.
// ============================================================

GATv2GpuContext* gatv2_gpu_create(
    const float* const W_heads_src[],
    const float* const W_heads_tgt[],
    int K,
    int H,
    int D,
    int max_neighbors
) noexcept;

// ============================================================
// run:
//   node:         [K]
//   neighbors:    [N][K]   (ciągły bufor, host)
//   h_src:        [H*D]    (wyjście W_src * node)
//   neighbor_out: [N][H*D] (wyjście W_tgt * neighbors)
// ============================================================

int gatv2_gpu_run(
    GATv2GpuContext* ctx,
    const float* node,
    const float* neighbors,
    int N,
    float* h_src,
    float* neighbor_out
) noexcept;


void gatv2_gpu_destroy(
    GATv2GpuContext* ctx
) noexcept;

// Returns the number of GPUs visible to the Clang/LLVM 17 runtime.
// Compiled with Clang — uses LLVM 17's libomp+libomptarget,
// NOT GCC's libgomp (which does not recognize gfx803).
int gatv2_gpu_available() noexcept;

#ifdef __cplusplus
}
#endif
