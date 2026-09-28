#include <atomic>



struct PipelineContext 
{
  
    std::atomic<bool> guard{true};

ku
    inline void Stop() noexcept {
        guard.store(false, std::memory_order_relaxed);
    }

    inline void Start() noexcept {
        guard.store(true, std::memory_order_relaxed);
    }

    inline bool IsRunning() const noexcept {
        return guard.load(std::memory_order_relaxed);
    }
};
