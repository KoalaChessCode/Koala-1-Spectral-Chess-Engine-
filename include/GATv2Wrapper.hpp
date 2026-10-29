#pragma once
#include <iostream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include "BayesPUCT.hpp"
#include "GATv2.hpp"

inline void ReportGATv2Stats(PipelineContext& ctx, bool force = false) {
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::now();
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    auto last_ns = ctx.gatv2_last_report_ns.load(std::memory_order_relaxed);
    if (!force && last_ns != 0 && now_ns - last_ns < 350'000'000)
        return;
    if (!ctx.gatv2_last_report_ns.compare_exchange_strong(
            last_ns, now_ns, std::memory_order_relaxed))
        return;

    const auto calls = ctx.gatv2_calls.load(std::memory_order_relaxed);
    const auto ok = ctx.gatv2_ok.load(std::memory_order_relaxed);
    const auto max_depth = ctx.max_depth_seen.load(std::memory_order_relaxed);
    const auto total = ctx.total_ns.load(std::memory_order_relaxed);
    static std::mutex output_mutex;
    std::lock_guard<std::mutex> lock(output_mutex);
    const double total_ms = total / 1.0e6;
    const double per_call_us = calls > 0 ? total / 1000.0 / calls : 0.0;
    std::cerr << std::fixed << std::setprecision(3)
              << "[GATv2] calls: " << calls
              << ", ok: " << ok
              << ", max_depth: " << max_depth
              << ", total: " << total_ms << " ms"
              << ", avg/call: " << per_call_us << " us\n";
}

// ============================================================
// EVALUATION OF A SINGLE NODE (without DFS loop)
// ============================================================
inline void EvaluateSingleNodeGAT(
    TreeStateWrapper& St,
    PipelineContext& ctx,
    uint32_t childIndex,
    int depth,
    const FFTWorking& workingFFT
) {
 // 1. Synchronizing FFT to a circular buffer
    const int node_slot = depth % PipelineContext::HISTORY_K;
    std::memcpy(&ctx.fft_working_history[node_slot], &workingFFT, sizeof(FFTWorking));
    InterleaveFFT(
        &ctx.fft_working_history[node_slot].real[0][0][0], 
        &ctx.fft_working_history[node_slot].imag[0][0][0], 
        ctx.fft_interleaved + node_slot * FFT_FLOATS
    );

    const float* node_fft = ctx.fft_interleaved + node_slot * FFT_FLOATS;

    // 2. Retrieving neighbors' history (HISTORY_K)
    const int num_ancestors = (depth < PipelineContext::HISTORY_K) ? depth : PipelineContext::HISTORY_K - 1;
    const float* neighbors[PipelineContext::HISTORY_K - 1];
    
    for (int j = 0; j < num_ancestors; ++j) {
        const int ancestor_depth = depth - 1 - j;
        const int slot = ancestor_depth % PipelineContext::HISTORY_K;
        neighbors[j] = ctx.fft_interleaved + slot * FFT_FLOATS;
    }

    // 3. Evaluation via GATv2
    alignas(64) float embedding[DefaultGATv2Config::EMBED_DIM];

    ctx.gatv2_calls.fetch_add(1, std::memory_order_relaxed);
    int max_depth = ctx.max_depth_seen.load(std::memory_order_relaxed);
    while (depth > max_depth &&
           !ctx.max_depth_seen.compare_exchange_weak(
               max_depth, depth, std::memory_order_relaxed)) {}
    const auto forward_start = std::chrono::steady_clock::now();
    const bool forward_ok = ctx.gatv2->forward(
        node_fft, neighbors, num_ancestors, embedding, *ctx.gatv2_workspace);
    const auto forward_end = std::chrono::steady_clock::now();
    ctx.total_ns.fetch_add(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            forward_end - forward_start).count()), std::memory_order_relaxed);
    if (forward_ok)
        ctx.gatv2_ok.fetch_add(1, std::memory_order_relaxed);
    ReportGATv2Stats(ctx);

    if (forward_ok) {
        float* current_emb = ctx.gatv2_embeddings + childIndex * DefaultGATv2Config::EMBED_DIM;
        std::memcpy(current_emb, embedding, DefaultGATv2Config::EMBED_DIM * sizeof(float));

        float sum = 0.0f;
        for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k) sum += current_emb[k];
        float gat_value = std::tanh(sum / DefaultGATv2Config::EMBED_DIM);

        // 4. Calculating the prior (cosine to parent)
        float policy_prior = 1.0f;
        if (depth >= 1) {
            const uint32_t parent_idx = St.tree.parentIndex[childIndex];
            const float* parent_emb = ctx.gatv2_embeddings + parent_idx * DefaultGATv2Config::EMBED_DIM;
            float dot = 0.0f, norm_p = 0.0f, norm_c = 0.0f;
            for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k) {
                dot += parent_emb[k] * current_emb[k];
                norm_p += parent_emb[k] * parent_emb[k];
                norm_c += current_emb[k] * current_emb[k];
            }
            policy_prior = std::exp(dot / (std::sqrt(norm_p) * std::sqrt(norm_c) + 1e-8f));
        }

       // 5. PUCT initialization
        init_puct_state(ctx.puct_states[childIndex], gat_value, policy_prior);
    }
}
