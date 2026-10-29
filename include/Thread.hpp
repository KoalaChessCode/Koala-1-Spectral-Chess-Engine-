#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <chrono>
#include <condition_variable>
#include <mutex>


#include "BayesPUCT.hpp"
#include "GATv2.hpp"
#include "Delta.hpp"
#include "memory.hpp"
#include "Tree.hpp"


struct PipelineContext 
{
    static constexpr std::size_t TREE_SIZE = 100;
    PuctState* puct_states = nullptr;
    bool* puct_ready = nullptr;
    std::uint32_t* puct_ready_queue = nullptr;
    float* gatv2_embeddings = nullptr;
    static constexpr int HISTORY_K = 32;
    std::atomic<bool> guard{true};
    std::atomic<std::uint64_t> gatv2_calls{0};
    std::atomic<std::uint64_t> gatv2_ok{0};
    std::atomic<int> max_depth_seen{0};
    std::atomic<std::uint64_t> total_ns{0};
    std::atomic<std::int64_t> gatv2_last_report_ns{0};
    std::mutex puct_mutex;
    std::condition_variable puct_cv;
    std::size_t puct_queue_head = 0;
    std::size_t puct_queue_count = 0;
    bool puct_done = false;

    DefaultGATv2Network*   gatv2           = nullptr;
    DefaultGATv2Workspace* gatv2_workspace = nullptr;

    FFTWorking* fft_working_history = nullptr;   // [HISTORY_K] // [32] = 512 KiB
    float*      fft_interleaved     = nullptr;   // [HISTORY_K][FFT_FLOATS]// // [32][4096] = 512 KiB

    inline void Stop() noexcept {
        guard.store(false, std::memory_order_relaxed);
    }

    inline void Start() noexcept {
        guard.store(true, std::memory_order_relaxed);
    }

    inline bool IsRunning() const noexcept {
        return guard.load(std::memory_order_relaxed);
    }

   template<typename ArenaType>
    bool allocate(ArenaType& arena)
{
   
    puct_states = arena.template allocate<PuctState>(TREE_SIZE);
    if (!puct_states) return false;

    puct_ready = arena.template allocate<bool>(TREE_SIZE);
    if (!puct_ready) return false;

    puct_ready_queue = arena.template allocate<std::uint32_t>(TREE_SIZE);
    if (!puct_ready_queue) return false;

   
    gatv2_embeddings = arena.template allocate<float>(
        TREE_SIZE * DefaultGATv2Config::EMBED_DIM
    );
    if (!gatv2_embeddings) return false;


    fft_working_history =
        arena.template allocate<FFTWorking>(HISTORY_K);
    if (!fft_working_history) return false;

    fft_interleaved = arena.template allocate<float>(
        static_cast<std::size_t>(HISTORY_K) * FFT_FLOATS
    );
    if (!fft_interleaved) return false;


    for (std::size_t i = 0; i < TREE_SIZE; ++i)
        std::construct_at(&puct_states[i]);

    std::fill_n(puct_ready, TREE_SIZE, false);


    std::fill_n(
        gatv2_embeddings,
        TREE_SIZE * DefaultGATv2Config::EMBED_DIM,
        0.0f
    );

    return true;
}

};
