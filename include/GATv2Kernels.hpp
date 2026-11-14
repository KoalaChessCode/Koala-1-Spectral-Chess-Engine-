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

#ifdef KOALA_CPU_BACKEND
static inline GATv2GpuContext* gatv2_gpu_create(
    const float* const[],
    const float* const[],
    int,
    int,
    int,
    int,
    int
) noexcept
{
    return nullptr;
}

static inline int gatv2_gpu_run(
    GATv2GpuContext*,
    const float*,
    const float*,
    int,
    float*,
    float*
) noexcept
{
    return 0;
}

static inline int gatv2_gpu_run_batch(
    GATv2GpuContext*,
    const float*,
    int,
    const float*,
    int,
    float*,
    float*
) noexcept
{
    return 0;
}

static inline void gatv2_gpu_destroy(GATv2GpuContext*) noexcept
{
}

static inline int gatv2_gpu_available() noexcept
{
    return 0;
}
#else
GATv2GpuContext* gatv2_gpu_create(
    const float* const W_heads_src[],
    const float* const W_heads_tgt[],
    int K,
    int H,
    int D,
    int max_neighbors,
    int max_batch_nodes
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

int gatv2_gpu_run_batch(
    GATv2GpuContext* ctx,
    const float* nodes,
    int batch_count,
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
#endif

#ifdef __cplusplus
}
#endif
