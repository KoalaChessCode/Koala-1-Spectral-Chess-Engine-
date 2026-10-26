#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>

#include "GATv2.hpp"
#include "Delta.hpp"
#include "memory.hpp"


struct PipelineContext 
{
    static constexpr int HISTORY_K = 32;
    std::atomic<bool> guard{true};

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
        fft_working_history = arena.template allocate<FFTWorking>(HISTORY_K);
        if (!fft_working_history) return false;

        fft_interleaved = arena.template allocate<float>(
            static_cast<std::size_t>(HISTORY_K) * FFT_FLOATS
        );
        if (!fft_interleaved) return false;

        return true;
    }


};
