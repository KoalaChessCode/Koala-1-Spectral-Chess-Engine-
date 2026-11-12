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
    bool forward_ok = false;
    {
        std::lock_guard<std::mutex> lock(ctx.gatv2_mutex);
        forward_ok = ctx.gatv2->forward(
            node_fft, neighbors, num_neighbors, embedding,
            *ctx.gatv2_workspace);
    }
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

inline void UpdateFFTHistoryOnly(
    TreeStateWrapper& St,
    PipelineContext& ctx,
    std::uint32_t nodeIndex,
    int depth,
    const FFTWorking& workingFFT
) {
    if (nodeIndex >= St.tree.size || depth < 0 ||
        ctx.fft_working_history == nullptr ||
        ctx.fft_interleaved == nullptr ||
        ctx.fft_history_nodes == nullptr)
        return;

    const int slot = depth % PipelineContext::HISTORY_K;
    std::memcpy(
        &ctx.fft_working_history[slot],
        &workingFFT,
        sizeof(FFTWorking));
    InterleaveFFT(
        &ctx.fft_working_history[slot].real[0][0][0],
        &ctx.fft_working_history[slot].imag[0][0][0],
        ctx.fft_interleaved + slot * FFT_FLOATS);
    ctx.fft_history_nodes[slot] = nodeIndex;
}

inline void EvaluateSiblingBatchGAT(
    TreeStateWrapper& St,
    PipelineContext& ctx,
    std::uint32_t parentIndex,
    int parentDepth,
    const FFTWorking& parentFFT,
    bool parentWhiteToMove
) {
    if (!ctx.gatv2 || !ctx.gatv2_workspace ||
        !ctx.gatv2_batch_fft || !ctx.puct_ready ||
        !ctx.gatv2_embeddings || parentIndex >= St.tree.size)
        return;

    const std::uint32_t firstChild = St.tree.childBegin[parentIndex];
    const std::uint16_t childCount = St.tree.childCount[parentIndex];
    if (childCount == 0 || childCount > MAX_GAT_BATCH_NODES ||
        firstChild + childCount > St.tree.size)
        return;

    {
        std::lock_guard<std::mutex> lock(ctx.puct_mutex);
        for (std::uint16_t i = 0; i < childCount; ++i)
            ctx.puct_ready[firstChild + i] = false;
    }

    for (std::uint16_t i = 0; i < childCount; ++i)
    {
        FFTWorking staged_fft;
        std::memcpy(&staged_fft, &parentFFT, sizeof(FFTWorking));
        ApplyFftDelta<4>(
            staged_fft, St.tree.At(firstChild + i).fftDelta);
        float* staged = ctx.gatv2_batch_fft +
            static_cast<std::size_t>(i) * FFT_FLOATS;
        InterleaveFFT(
            &staged_fft.real[0][0][0],
            &staged_fft.imag[0][0][0],
            staged);
    }

    const int childDepth = parentDepth + 1;
    int maxDepth = ctx.max_depth_seen.load(std::memory_order_relaxed);
    while (childDepth > maxDepth &&
           !ctx.max_depth_seen.compare_exchange_weak(
               maxDepth, childDepth, std::memory_order_relaxed)) {}
    const float* neighbors[PipelineContext::HISTORY_K - 1];
    int num_ancestors = 0;
    std::uint32_t ancestor = parentIndex;
    for (int j = 0;
         j < childDepth && j < PipelineContext::HISTORY_K - 1;
         ++j)
    {
        const int ancestorDepth = childDepth - 1 - j;
        const int slot = ancestorDepth % PipelineContext::HISTORY_K;
        if (ancestor >= St.tree.size)
            break;
        if (ctx.fft_history_nodes[slot] == ancestor)
            neighbors[num_ancestors++] =
                ctx.fft_interleaved + slot * FFT_FLOATS;
        ancestor = St.tree.parentIndex[ancestor];
    }

    ctx.gatv2_calls.fetch_add(childCount, std::memory_order_relaxed);
    const auto forwardStart = std::chrono::steady_clock::now();
    bool forwardOk = false;
    {
        std::lock_guard<std::mutex> lock(ctx.gatv2_mutex);
        forwardOk = ctx.gatv2->forward_batch(
            ctx.gatv2_batch_fft,
            childCount,
            neighbors,
            num_ancestors,
            ctx.gatv2_workspace->batch_h_src.get(),
            *ctx.gatv2_workspace);
    }
    const auto forwardEnd = std::chrono::steady_clock::now();
    ctx.total_ns.fetch_add(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            forwardEnd - forwardStart).count()), std::memory_order_relaxed);
    if (forwardOk)
        ctx.gatv2_ok.fetch_add(childCount, std::memory_order_relaxed);
    ReportGATv2Stats(ctx);
    if (!forwardOk)
        return;

    const bool childWhiteToMove = !parentWhiteToMove;
    for (std::uint16_t i = 0; i < childCount; ++i)
    {
        const std::uint32_t childIndex = firstChild + i;
        const float* embedding = ctx.gatv2_workspace->batch_h_src.get() +
            static_cast<std::size_t>(i) * DefaultGATv2Config::EMBED_DIM;
        bool finite = true;
        float sum = 0.0f;
        for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k)
        {
            finite = finite && std::isfinite(embedding[k]);
            sum += embedding[k];
        }
        if (!finite || !std::isfinite(sum))
            continue;

        float* currentEmbedding = ctx.gatv2_embeddings +
            static_cast<std::size_t>(childIndex) *
                DefaultGATv2Config::EMBED_DIM;
        std::memcpy(
            currentEmbedding,
            embedding,
            DefaultGATv2Config::EMBED_DIM * sizeof(float));

        const float whiteValue =
            std::tanh(sum / DefaultGATv2Config::EMBED_DIM);
        const float gatValue = childWhiteToMove ? whiteValue : -whiteValue;
        float policyPrior = 1.0f;
        const std::uint32_t parent = St.tree.parentIndex[childIndex];
        if (parent >= St.tree.size)
            continue;

        const float* parentEmbedding = ctx.gatv2_embeddings +
            static_cast<std::size_t>(parent) *
                DefaultGATv2Config::EMBED_DIM;
        float dot = 0.0f;
        float normParentSq = 0.0f;
        float normChildSq = 0.0f;
        for (int k = 0; k < DefaultGATv2Config::EMBED_DIM; ++k)
        {
            const float p = parentEmbedding[k];
            const float c = currentEmbedding[k];
            if (!std::isfinite(p) || !std::isfinite(c))
            {
                finite = false;
                break;
            }
            dot += p * c;
            normParentSq += p * p;
            normChildSq += c * c;
        }
        if (!finite || !std::isfinite(dot) ||
            !std::isfinite(normParentSq) || !std::isfinite(normChildSq))
            continue;
        constexpr float EPS = 1.0e-8f;
        if (normParentSq > EPS && normChildSq > EPS)
        {
            float cosine = dot /
                (std::sqrt(normParentSq) * std::sqrt(normChildSq));
            if (!std::isfinite(cosine))
                continue;
            cosine = std::clamp(cosine, -1.0f, 1.0f);
            policyPrior = std::exp(cosine - 1.0f);
        }

        std::lock_guard<std::mutex> lock(ctx.puct_mutex);
        init_puct_state(ctx.puct_states[childIndex], gatValue, policyPrior);
        ctx.puct_ready[childIndex] = true;
    }
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

    UpdateFFTHistoryOnly(St, ctx, childIndex, depth, workingFFT);
    const int node_slot = depth % PipelineContext::HISTORY_K;

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
        }

    }
}
