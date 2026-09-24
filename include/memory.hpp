struct Arena
{
    static constexpr std::size_t SIZE = 1024 * 1024;

    std::array<std::byte, SIZE> arena{};
    std::size_t offset = 0;


    template<typename T>
    T* allocate()
    {
        constexpr std::size_t alignment = alignof(T);

        std::size_t aligned =
            (offset + alignment - 1) & ~(alignment - 1);

        if (aligned + sizeof(T) > arena.size())
            return nullptr;

        T* ptr =
            reinterpret_cast<T*>(arena.data() + aligned);

        offset = aligned + sizeof(T);

        return ptr;
    }


    template<typename T>
    T* allocate(std::size_t count)
    {
        constexpr std::size_t alignment = alignof(T);

        std::size_t aligned =
            (offset + alignment - 1) & ~(alignment - 1);

        std::size_t bytes =
            sizeof(T) * count;

        if (aligned + bytes > arena.size())
            return nullptr;

        T* ptr =
            reinterpret_cast<T*>(arena.data() + aligned);

        offset = aligned + bytes;

        return ptr;
    }
};

