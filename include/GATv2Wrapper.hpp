#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
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
    if (force) {
        ctx.gatv2_last_report_ns.store(now_ns, std::memory_order_relaxed);
    } else {
        auto last_ns = ctx.gatv2_last_report_ns.load(std::memory_order_relaxed);
        if (last_ns != 0 && now_ns - last_ns < 350'000'000)
            return;
        if (!ctx.gatv2_last_report_ns.compare_exchange_strong(
                last_ns, now_ns, std::memory_order_relaxed))
            return;
    }

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
// GATv2 receives only the node representation and verified neighbors.
// ============================================================
inline bool EvaluateGATv2(
    PipelineContext& ctx,
    const float* node_fft,
    const float* const neighbors[],
    int num_neighbors,
    float* embedding
) {
    if (!ctx.gatv2 || !ctx.gatv2_workspace || !node_fft || !embedding)
        return false;

    ctx.gatv2_calls.fetch_add(1, std::memory_order_relaxed);
    const auto forward_start = std::chrono::steady_clock::now();
    const bool forward_ok = ctx.gatv2->forward(
        node_fft, neighbors, num_neighbors, embedding, *ctx.gatv2_workspace);
    const auto forward_end = std::chrono::steady_clock::now();
    ctx.total_ns.fetch_add(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            forward_end - forward_start).count()), std::memory_order_relaxed);

    if (forward_ok)
        ctx.gatv2_ok.fetch_add(1, std::memory_order_relaxed);
    ReportGATv2Stats(ctx);

    if (!forward_ok)
        return false;
    for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k)
        if (!std::isfinite(embedding[k]))
            return false;
    return true;
}

// Prepare search-local FFT history and commit the model result to this node.
inline void EvaluateSingleNodeGAT(
    TreeStateWrapper& St,
    PipelineContext& ctx,
    uint32_t childIndex,
    int depth,
    const FFTWorking& workingFFT,
    bool whiteToMove
) {
    if (childIndex >= PipelineContext::TREE_SIZE ||
        childIndex >= St.tree.size || ctx.puct_ready == nullptr)
        return;
    {
        std::lock_guard<std::mutex> lock(ctx.puct_mutex);
        ctx.puct_ready[childIndex] = false;
    }

    // Synchronize this node's actual FFT into its depth slot.
    const int node_slot = depth % PipelineContext::HISTORY_K;
    std::memcpy(&ctx.fft_working_history[node_slot], &workingFFT, sizeof(FFTWorking));
    InterleaveFFT(
        &ctx.fft_working_history[node_slot].real[0][0][0], 
        &ctx.fft_working_history[node_slot].imag[0][0][0], 
        ctx.fft_interleaved + node_slot * FFT_FLOATS
    );
    ctx.fft_history_nodes[node_slot] = childIndex;

    const float* node_fft = ctx.fft_interleaved + node_slot * FFT_FLOATS;

    // Include only ring entries that match this node's actual ancestors.
    const float* neighbors[PipelineContext::HISTORY_K - 1];
    int num_ancestors = 0;
    std::uint32_t ancestor = childIndex == 0
        ? 0 : St.tree.parentIndex[childIndex];
    for (int j = 0; j < depth && j < PipelineContext::HISTORY_K - 1; ++j) {
        const int ancestor_depth = depth - 1 - j;
        const int slot = ancestor_depth % PipelineContext::HISTORY_K;
        if (ancestor >= St.tree.size)
            break;
        if (ctx.fft_history_nodes[slot] == ancestor)
            neighbors[num_ancestors++] = ctx.fft_interleaved + slot * FFT_FLOATS;
        ancestor = St.tree.parentIndex[ancestor];
    }

    alignas(64) float embedding[DefaultGATv2Config::EMBED_DIM];
    int max_depth = ctx.max_depth_seen.load(std::memory_order_relaxed);
    while (depth > max_depth &&
           !ctx.max_depth_seen.compare_exchange_weak(
               max_depth, depth, std::memory_order_relaxed)) {}
    if (EvaluateGATv2(ctx, node_fft, neighbors, num_ancestors, embedding)) {

        float* current_emb = ctx.gatv2_embeddings + childIndex * DefaultGATv2Config::EMBED_DIM;
        std::memcpy(current_emb, embedding, DefaultGATv2Config::EMBED_DIM * sizeof(float));

        float sum = 0.0f;
        for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k) sum += current_emb[k];
        // The model score has a fixed White perspective. PUCT stores values
        // from the perspective of the player to move at each node.
        const float whiteValue =
            std::tanh(sum / DefaultGATv2Config::EMBED_DIM);
        const float gat_value = whiteToMove ? whiteValue : -whiteValue;

       
        // 4. Prior from cosine similarity to parent
        float policy_prior = 1.0f;

        if (depth >= 1) {
            const uint32_t parent_idx = St.tree.parentIndex[childIndex];

            // Nie odczytuj embeddingu spoza drzewa.
            if (parent_idx >= St.tree.size) {
                return;
            }

            const float* parent_emb =
                ctx.gatv2_embeddings +
                static_cast<std::size_t>(parent_idx) *
                    DefaultGATv2Config::EMBED_DIM;

            float dot = 0.0f;
            float norm_p_sq = 0.0f;
            float norm_c_sq = 0.0f;

            for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k) {
                const float p = parent_emb[k];
                const float c = current_emb[k];

                if (!std::isfinite(p) || !std::isfinite(c))
                    return;

                dot       += p * c;
                norm_p_sq += p * p;
                norm_c_sq += c * c;
            }

            constexpr float EPS = 1.0e-8f;
            constexpr float TEMPERATURE = 1.0f; // baseline

            if (!std::isfinite(dot) ||
                !std::isfinite(norm_p_sq) ||
                !std::isfinite(norm_c_sq)) {
                return;
            }

            if (norm_p_sq > EPS && norm_c_sq > EPS) {
                float cosine =
                    dot / (std::sqrt(norm_p_sq) * std::sqrt(norm_c_sq));

                if (!std::isfinite(cosine))
                    return;

                cosine = std::clamp(cosine, -1.0f, 1.0f);

                policy_prior =
                    std::exp((cosine - 1.0f) / TEMPERATURE);
            }
        }

        // 5. Existing Bayesian PUCT initialization — unchanged.
        {
            std::lock_guard<std::mutex> lock(ctx.puct_mutex);
            init_puct_state(
                ctx.puct_states[childIndex],
                gat_value,
                policy_prior
            );
            ctx.puct_ready[childIndex] = true;

            if (childIndex != 0 &&
                ctx.puct_queue_count < PipelineContext::TREE_SIZE) {
                const std::size_t tail =
                    (ctx.puct_queue_head + ctx.puct_queue_count) %
                    PipelineContext::TREE_SIZE;
                ctx.puct_ready_queue[tail] = childIndex;
                ++ctx.puct_queue_count;
            }
        }
        if (childIndex != 0)
            ctx.puct_cv.notify_one();

    }
}
