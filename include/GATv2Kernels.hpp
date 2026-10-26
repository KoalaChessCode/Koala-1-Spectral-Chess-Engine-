#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

struct GATv2GpuContext;

// ============================================================
// create: przyjmuje TABLICĘ wskaźników per głowa.
//
//   W_heads_src[m] -> blok [D][K] (HIDDEN_DIM × K floatów)
//   W_heads_tgt[m] -> blok [D][K]
//
// create() SKLEJA w jeden ciągły bufor [H][D][K] na GPU.
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


// Zwraca liczbę GPU widzianych przez runtime Clang/LLVM 17.
// Kompilowane Clangiem — używa libomp+libomptarget LLVM 17,
// a NIE libgomp GCC (który nie zna gfx803).
int gatv2_gpu_available() noexcept;

#ifdef __cplusplus
}
#endif