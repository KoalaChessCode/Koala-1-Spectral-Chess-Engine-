#pragma once
#include <array>
#include <utility> 

constexpr size_t HEADER_SIZE      = 64;  // bytes
constexpr size_t FIGURE_COUNT     = 32;
constexpr size_t FFT_CHANNELS     = 64;
constexpr size_t FIGURE_FEATURES  = 128; // 64 Real + 64 Imag

constexpr int N    = 32;
constexpr int F_IN = 128;

constexpr int F_L1 = 64;
constexpr int F_L2 = 32;
constexpr int F_L3 = 16;

using Matrix = std::array<std::array<float, FIGURE_FEATURES>, FIGURE_COUNT>;
alignas(64) static Matrix clean_channels;


template <size_t N>
auto& InitSpectrum(const unsigned char (&blob)[N]){
    const float* raw_floats_bin = reinterpret_cast<const float*>(blob + HEADER_SIZE);

    // Metaprogramming that unrolls the channel loop at compile time (no "for" loop in the machine code)
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        (..., ([&]() {
            size_t ch = Is; // Current channel index (0 to 31)
            
            // Direct reference access to the internal std::array for a given figure
            float* dest_real = clean_channels[ch].data();              // First 64 slots for Real
            float* dest_imag = &(clean_channels[ch][FFT_CHANNELS]);      // Next 64 slots for Imag
            
            // Starting point for reading from the interleaved binary file
            const float* read_bin = &raw_floats_bin[ch * (FFT_CHANNELS * 2)];

            // Unpacking the binary interleave (compiler vectorizes this using SIMD)
            #pragma GCC unroll 64
            for (size_t k = 0; k < FFT_CHANNELS; ++k) {
                dest_real[k] = read_bin[2 * k];
                dest_imag[k] = read_bin[2 * k + 1];
            }
        }()));
    }(std::make_index_sequence<FIGURE_COUNT>{});

    // Returns the entire packed std::array structure by reference (compiler optimizes this to 0ns)
    return clean_channels;
}
