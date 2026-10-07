#pragma once

#include <cstddef>
#include <cstdint>


template <std::size_t N>
inline void InitSpectrum(
    const unsigned char (&blob)[N],
    FFTWorking& fft
) noexcept
{
    constexpr std::size_t HeaderSize = 64;
    constexpr std::size_t Channels   = 32;
    constexpr std::size_t Rows       = 8;
    constexpr std::size_t Cols       = 8;
    constexpr std::size_t Values     = Rows * Cols;

    static_assert(
        N >= HeaderSize + Channels * Values * sizeof(float) * 2
    );

    const float* source =
        reinterpret_cast<const float*>(blob + HeaderSize);

    for (std::size_t channel = 0;
         channel < Channels;
         ++channel)
    {
        const float* src =
            source + channel * Values * 2;

        float* real =
            &fft.real[channel][0][0];

        float* imag =
            &fft.imag[channel][0][0];

        for (std::size_t i = 0;
             i < Values;
             ++i)
        {
            real[i] = src[i * 2];
            imag[i] = src[i * 2 + 1];
        }
    }
}


