#pragma once
#include <iostream>
#include <chrono>
#include "BayesPUCT.hpp"
#include "GATv2.hpp"

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

    if (ctx.gatv2->forward(node_fft, neighbors, num_ancestors, embedding, *ctx.gatv2_workspace)) {
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