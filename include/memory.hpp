#pragma once
#include <memory>

struct Arena
{
    std::size_t size = 0;             
    std::unique_ptr<std::byte[]> arena;
    std::size_t offset = 0;

    explicit Arena(std::size_t size_ = 256 * 1024 * 1024) 
        : size(size_),
          arena(std::make_unique<std::byte[]>(size_))
    {
    }

    template<typename T>
    T* allocate()
    {
        constexpr std::size_t alignment = alignof(T);

        const std::size_t aligned =
            (offset + alignment - 1) & ~(alignment - 1);

        if (aligned + sizeof(T) > size) 
            return nullptr;

        T* ptr = reinterpret_cast<T*>(arena.get() + aligned);
        offset = aligned + sizeof(T);
        return ptr;
    }

    template<typename T>
    T* allocate(std::size_t count)
    {
        constexpr std::size_t alignment = alignof(T);

        const std::size_t aligned =
            (offset + alignment - 1) & ~(alignment - 1);

        const std::size_t bytes = sizeof(T) * count;

        if (aligned + bytes > size)  
            return nullptr;

        T* ptr = reinterpret_cast<T*>(arena.get() + aligned);
        offset = aligned + bytes;
        return ptr;
    }
};
