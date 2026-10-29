
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

#include "Tree.hpp"

constexpr float SIGMA_V_2 = 1.0f;
constexpr float PUCT_EPSILON = 1.0e-8f;

// ============================================================
// BAYESIAN PUCT STATE
// ============================================================

struct PuctState {
    uint32_t N = 0;

    float mu_0 = 0.0f;
    float precision_0 = 1.0f;


    float sum_v = 0.0f;


    float mu_N = 0.0f;
    float precision_N = 1.0f;


    float prior = 0.0f;
};

inline void init_puct_state(
    PuctState& state,
    float initial_mu,
    float initial_prior,
    float initial_precision = 1.0f
) noexcept {
    state.mu_0 = std::isfinite(initial_mu)
        ? initial_mu
        : 0.0f;

    state.precision_0 =
        std::isfinite(initial_precision) &&
        initial_precision > PUCT_EPSILON
        ? initial_precision
        : 1.0f;

    state.N = 0;
    state.sum_v = 0.0f;


    state.mu_N = state.mu_0;
    state.precision_N = state.precision_0;

    state.prior =
        std::isfinite(initial_prior) && initial_prior > 0.0f
        ? initial_prior
        : 0.0f;
}



inline void update_bayesian_state(
    PuctState& state,
    float observed_value
) noexcept {
    if (!std::isfinite(observed_value))
        return;

    if (state.N == std::numeric_limits<uint32_t>::max())
        return;

    ++state.N;
    state.sum_v += observed_value;

    const float observation_precision = 1.0f / SIGMA_V_2;

    state.precision_N =
        state.precision_0 +
        static_cast<float>(state.N) * observation_precision;

    state.mu_N =
        (state.mu_0 * state.precision_0 +
         state.sum_v * observation_precision) /
        state.precision_N;
}

// ============================================================
// CHILD SELECTION: BAYESIAN UCB + PUCT
//
// score = mu_N
//       + beta * sigma_N
//       + c_puct * P(s,a) * sqrt(N_parent + 1) / (1 + N_child)
//
// Priors are normalized locally among siblings.
// If all are zero, a uniform distribution is used.
// ============================================================

inline uint32_t select_bayes_puct_child(
    uint32_t node_idx,
    PuctState* states,
    float c_puct,
    float beta,
    const Tree<Board>& tree,
    const bool* ready = nullptr
) noexcept {
    const uint32_t begin = tree.childBegin[node_idx];
    const uint16_t count = tree.childCount[node_idx];

    if (count == 0)
        return node_idx;

    const float parent_visits =
        static_cast<float>(states[node_idx].N);

    const float parent_scale =
        std::sqrt(parent_visits + 1.0f);

    c_puct = std::max(0.0f, c_puct);
    beta = std::max(0.0f, beta);

    float prior_sum = 0.0f;
    uint16_t ready_count = 0;

    for (uint16_t i = 0; i < count; ++i) {
        const uint32_t child_idx = begin + i;
        if (ready && !ready[child_idx])
            continue;
        ++ready_count;

        const float p = states[child_idx].prior;

        if (std::isfinite(p) && p > 0.0f)
            prior_sum += p;
    }

    const bool uniform_priors =
        !std::isfinite(prior_sum) ||
        prior_sum <= PUCT_EPSILON;

    if (ready_count == 0)
        return node_idx;

    uint32_t best_child = std::numeric_limits<uint32_t>::max();
    float best_score = -std::numeric_limits<float>::infinity();

    for (uint16_t i = 0; i < count; ++i) {
        const uint32_t child_idx = begin + i;
        if (ready && !ready[child_idx])
            continue;
        const PuctState& child = states[child_idx];

        const float precision = std::max(
            child.precision_N,
            PUCT_EPSILON
        );

      
        const float sigma_N = 1.0f / std::sqrt(precision);


        const float bayes_term =
            child.mu_N + beta * sigma_N;

   
        float P = 0.0f;

        if (uniform_priors) {
            P = 1.0f / static_cast<float>(ready_count);
        } else if (std::isfinite(child.prior) &&
                   child.prior > 0.0f) {
            P = child.prior / prior_sum;
        }

  
        const float puct_term =
            c_puct * P * parent_scale /
            (1.0f + static_cast<float>(child.N));

        const float score = bayes_term + puct_term;

        if (std::isfinite(score) && score > best_score) {
            best_score = score;
            best_child = child_idx;
        }
    }

    return best_child == std::numeric_limits<uint32_t>::max()
        ? node_idx
        : best_child;
}

// ============================================================
// BACKPROPAGATION: NEGAMAX
//
// REQUIREMENT:
// The value must be expressed from the perspective of the player
// whose turn it is at the current node. Each parent level
// flips the sign.
// ============================================================

inline void backpropagate_bayes(
    uint32_t leaf_idx,
    float value,
    PuctState* states,
    const Tree<Board>& tree
) noexcept {
    if (!std::isfinite(value))
        return;

    uint32_t current = leaf_idx;

    while (current != 0) {
        update_bayesian_state(states[current], value);

        value = -value;

        current = tree.parentIndex[current];
    }

    update_bayesian_state(states[0], value);
}

// ============================================================
// SELECTION OF THE FINAL CHILD NODE
// Based first on the number of visits; in the event of a tie, use the posterior mean.
// ============================================================

inline uint32_t select_best_root_child(
    const Tree<Board>& tree,
    const PuctState* states,
    const bool* ready = nullptr
) noexcept {
    const uint32_t begin = tree.childBegin[0];
    const uint16_t count = tree.childCount[0];

    if (count == 0)
        return std::numeric_limits<uint32_t>::max();

    uint32_t best_child = std::numeric_limits<uint32_t>::max();

    for (uint16_t i = 0; i < count; ++i) {
        const uint32_t candidate = begin + i;
        if (ready && !ready[candidate])
            continue;

        if (best_child == std::numeric_limits<uint32_t>::max()) {
            best_child = candidate;
            continue;
        }

        const PuctState& a = states[candidate];
        const PuctState& b = states[best_child];

        if (a.N > b.N ||
            (a.N == b.N && a.mu_N > b.mu_N)) {
            best_child = candidate;
        }
    }

    return best_child;
}
// ============================================================
// BAYESIAN PUCT MAIN LOOP
//
// Version for an ALREADY EXPANDED TREE.
// Does not generate moves and does not perform expansion.
// ============================================================

inline uint32_t RunPUCT(
    TreeStateWrapper& St,
    PuctState* states,
    int iterations = 800,
    float c_puct = 1.0f,
    float beta = 1.0f,
    uint32_t start_node = 0,
    const bool* ready = nullptr
) {
    if (states == nullptr || iterations < 0)
        return std::numeric_limits<uint32_t>::max();

    if (St.tree.childCount[0] == 0)
        return std::numeric_limits<uint32_t>::max();

    if (ready && (start_node >= St.tree.size || !ready[start_node]))
        return std::numeric_limits<uint32_t>::max();

    if (ready) {
        bool has_ready_root_child = false;
        const uint32_t begin = St.tree.childBegin[0];
        const uint16_t count = St.tree.childCount[0];
        for (uint16_t i = 0; i < count; ++i) {
            if (ready[begin + i]) {
                has_ready_root_child = true;
                break;
            }
        }
        if (!has_ready_root_child)
            return std::numeric_limits<uint32_t>::max();
    }

    for (int iter = 0; iter < iterations; ++iter) {
        uint32_t current = start_node; // <-- START Z PODANEGO WĘZŁA

        while (St.tree.ChildCount(current) > 0) {
            const uint32_t next = select_bayes_puct_child(
                current, states, c_puct, beta, St.tree, ready
            );
            if (next == current) break;
            current = next;
        }

        const float leaf_value = states[current].mu_0;
        backpropagate_bayes(current, leaf_value, states, St.tree);
    }


    const uint32_t best_move =
        select_best_root_child(St.tree, states, ready);


    return best_move;
}
