#pragma once 
#include <memory>


struct Arena
{
    static constexpr std::size_t SIZE = 256 * 1024 * 1024;//256MB
    //static constexpr std::size_t SIZE = 2ULL * 1024 * 1024 * 1024; // 2 GiB

    std::unique_ptr<std::byte[]> arena;
    std::size_t offset = 0;

    Arena()
        : arena(std::make_unique<std::byte[]>(SIZE))
    {
    }

    template<typename T>
    T* allocate()
    {
        constexpr std::size_t alignment = alignof(T);

        const std::size_t aligned =
            (offset + alignment - 1) & ~(alignment - 1);

        if (aligned + sizeof(T) > SIZE)
            return nullptr;

        T* ptr =
            reinterpret_cast<T*>(arena.get() + aligned);

        offset = aligned + sizeof(T);

        return ptr;
    }

    template<typename T>
    T* allocate(std::size_t count)
    {
        constexpr std::size_t alignment = alignof(T);

        const std::size_t aligned =
            (offset + alignment - 1) & ~(alignment - 1);

        const std::size_t bytes =
            sizeof(T) * count;

        if (aligned + bytes > SIZE)
            return nullptr;

        T* ptr =
            reinterpret_cast<T*>(arena.get() + aligned);

        offset = aligned + bytes;

        return ptr;
    }
};