#pragma once
#include <iostream>
#include <chrono>

//===========================================================================
// GATv2 over a tree
// ===========================================================================
//
// DFS traversal over an existing tree.
//
// For each node (depth >= 1):
//   1. Node FFT = Parent FFT + node delta.
//   2. Neighbors = the last HISTORY_K ancestors.
//   3. GATv2 → embedding.
//
// ===========================================================================
inline void RunGATv2OverTree(
    TreeStateWrapper& St,
    PipelineContext& ctx,
    const FFTWorking& rootFFT
)
{
    constexpr int MaxDepth = 128;

    FFTWorking* fftHistory      = ctx.fft_working_history;
    float*      fftInterleaved  = ctx.fft_interleaved;

    if (!fftHistory || !fftInterleaved)
    {
        std::cerr << "[GATv2] ERROR: ring buffer not allocated\n";
        return;
    }
// --------------------------------------------------------
// DFS frame stack
// --------------------------------------------------------
    struct Frame
    {
        std::uint32_t nodeIndex;
        std::uint16_t nextChild;
    };

    Frame stack[MaxDepth];
    std::size_t depth = 0;

    stack[0] = { 0, 0 };
    
// -------------------------------------------------------- 
// Root: FFT to slot 0. 
// --------------------------------------------------------
    
    std::memcpy(&fftHistory[0], &rootFFT, sizeof(FFTWorking));

    InterleaveFFT(
        &fftHistory[0].real[0][0][0],
        &fftHistory[0].imag[0][0][0],
        fftInterleaved + 0 * FFT_FLOATS
    );
// --------------------------------------------------------
// Statistics
// --------------------------------------------------------
    std::uint32_t gatv2_calls   = 0;
    std::uint32_t gatv2_ok      = 0;
    std::uint32_t max_depth_seen = 0;

    std::uint64_t total_ns = 0;

    // --------------------------------------------------------
    // DFS
    // --------------------------------------------------------
    while (true)
    {
        Frame& current = stack[depth];

  // ====================================================
// GATv2 for the current node
// ====================================================
        if (depth >= 1)
        {
            const int num_ancestors =
                (depth < PipelineContext::HISTORY_K)
                    ? static_cast<int>(depth)
                    : PipelineContext::HISTORY_K - 1;

            const float* neighbors[PipelineContext::HISTORY_K - 1];

            for (int j = 0; j < num_ancestors; ++j)
            {
                const int ancestor_depth = static_cast<int>(depth) - 1 - j;
                const int slot = ancestor_depth % PipelineContext::HISTORY_K;
                neighbors[j] = fftInterleaved + slot * FFT_FLOATS;
            }

            const int node_slot = static_cast<int>(depth)
                % PipelineContext::HISTORY_K;

            const float* node_fft = fftInterleaved + node_slot * FFT_FLOATS;

            alignas(64) float embedding[DefaultGATv2Config::EMBED_DIM];

            auto t0 = std::chrono::high_resolution_clock::now();

            const bool ok = ctx.gatv2->forward(
                node_fft,
                neighbors,
                num_ancestors,
                embedding,
                *ctx.gatv2_workspace
            );

            auto t1 = std::chrono::high_resolution_clock::now();

            total_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    t1 - t0
                ).count()
            );

            ++gatv2_calls;
            if (ok) ++gatv2_ok;

            if (depth > max_depth_seen)
                max_depth_seen = static_cast<std::uint32_t>(depth);
        }

   // ====================================================
// Go to child / go back
// ====================================================
        const std::uint16_t childCount =
            St.tree.ChildCount(current.nodeIndex);

        if (current.nextChild < childCount)
        {
            const std::uint32_t childIdx =
                St.tree.ChildBegin(current.nodeIndex) + current.nextChild;
            ++current.nextChild;

            if (depth + 1 >= static_cast<std::size_t>(MaxDepth))
                continue;

            // FFT dziecka = FFT rodzica + delta.
            const int parent_slot = static_cast<int>(depth)
                % PipelineContext::HISTORY_K;

            const int child_slot = (static_cast<int>(depth) + 1)
                % PipelineContext::HISTORY_K;

            std::memcpy(
                &fftHistory[child_slot],
                &fftHistory[parent_slot],
                sizeof(FFTWorking)
            );

            ApplyFftDelta<4>(
                fftHistory[child_slot],
                St.tree.At(childIdx).fftDelta
            );

            InterleaveFFT(
                &fftHistory[child_slot].real[0][0][0],
                &fftHistory[child_slot].imag[0][0][0],
                fftInterleaved + child_slot * FFT_FLOATS
            );

            ++depth;
            stack[depth] = { childIdx, 0 };
            continue;
        }

        if (depth == 0)
            break;

        --depth;
    }

  // --------------------------------------------------------
// Final statistics
// --------------------------------------------------------
    const double total_ms = total_ns / 1.0e6;
    const double per_call_us =
        (gatv2_calls > 0)
            ? (total_ns / 1000.0 / gatv2_calls)
            : 0.0;

    std::cerr
        << "[GATv2] calls: " << gatv2_calls
        << ", ok: " << gatv2_ok
        << ", max_depth: " << max_depth_seen
        << ", total: " << total_ms << " ms"
        << ", avg/call: " << per_call_us << " us\n";

  // Extrapolation to 800,000 nodes.
    if (gatv2_calls > 0)
    {
        const double per_call_ms = total_ms / gatv2_calls;
        const double total_800k_s = per_call_ms * 800000.0 / 1000.0;

        std::cerr
            << "[GATv2] extrapolated for 800k nodes: "
            << total_800k_s << " s ("
            << total_800k_s / 60.0 << " min)\n";
    }
}
