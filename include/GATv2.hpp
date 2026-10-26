// include/GATv2.hpp
#pragma once

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <cmath>
#include <experimental/simd>
#include "GATv2Kernels.hpp"

namespace sx = std::experimental;



constexpr const char* KOALA_MODEL_PATH = "../weights/KoalaModel.sgat";

// ============================================================
// Embedded FFT blob
// ============================================================

alignas(64) extern unsigned char EmbeddedFftBlob[];


// ============================================================
// FFT configuration
// ============================================================

constexpr int FFT_CHANNELS = 32;
constexpr int FFT_SPATIAL  = 8;

constexpr int FFT_COMPLEX_COUNT =
    FFT_CHANNELS *
    FFT_SPATIAL *
    FFT_SPATIAL;                       // 2048

constexpr int FFT_FLOATS =
    FFT_COMPLEX_COUNT * 2;             // 4096

constexpr int FFT_SIZE =
    FFT_FLOATS;

constexpr std::size_t FFT_HEADER_SIZE = 64;

static_assert(sizeof(float) == 4);
static_assert(FFT_COMPLEX_COUNT == 2048);
static_assert(FFT_FLOATS == 4096);


// ============================================================
// GATv2 limits
// ============================================================

constexpr int MAX_HEADS     = 32;
constexpr int MAX_LAYERS    = 8;
constexpr int MAX_HIDDEN    = 64;
constexpr int MAX_NEIGHBORS = 256;

constexpr float GAT_LEAKY_SLOPE = 0.20f;


// ============================================================
// GATv2 compile-time configuration
// ============================================================

template<
    int Heads,
    int HeadDim,
    int Layers
>
struct GATv2Config
{
    static_assert(Heads > 0);
    static_assert(Heads <= MAX_HEADS);

    static_assert(HeadDim > 0);
    static_assert(HeadDim <= MAX_HIDDEN);

    static_assert(Layers > 0);
    static_assert(Layers <= MAX_LAYERS);

    static constexpr int HEADS =
        Heads;

    static constexpr int HEAD_DIM =
        HeadDim;

    static constexpr int LAYERS =
        Layers;

    static constexpr int EMBED_DIM =
        HEADS * HEAD_DIM;
};


// ============================================================
// Default configuration
//
// 16 heads
// 32 values / head
// 512-dimensional embedding
//
// L0:
//   4096 -> 512
//
// L1:
//   512 -> 512
//
// L2:
//   512 -> 512
// ============================================================

using DefaultGATv2Config =
    GATv2Config<16, 32, 3>;


// ============================================================
// Embedded FFT
// ============================================================
//
// Payload:
//
//   real, imag,
//   real, imag,
//   ...
//
// 2048 complex
// = 4096 float
// = 16384 bytes
//
// + 64 byte header
// ============================================================
//
// UWAGA:
//
// EmbeddedFftBlob musi być zdefiniowany w main.cpp jako tablica
// o znanym rozmiarze:
//
// alignas(64)
// unsigned char EmbeddedFftBlob[64 + 4096 * sizeof(float)];
//
// ============================================================

inline const float*
GetEmbeddedFFTAsFloat() noexcept
{
    return reinterpret_cast<const float*>(
        EmbeddedFftBlob +
        FFT_HEADER_SIZE
    );
}


// ============================================================
// Runtime FFT packing
// ============================================================

inline void InterleaveFFT(
    const float* __restrict real,
    const float* __restrict imag,
    float*       __restrict output
) noexcept
{
    #pragma GCC unroll 8
    for (int i = 0;
         i < FFT_COMPLEX_COUNT;
         ++i)
    {
        output[2 * i + 0] = real[i];
        output[2 * i + 1] = imag[i];
    }
}


// ============================================================
// GATv2 weights
// ============================================================

template<typename Config>
struct GATv2Weights
{
    static constexpr int NUM_HEADS =
        Config::HEADS;

    static constexpr int HIDDEN_DIM =
        Config::HEAD_DIM;

    static constexpr int NUM_LAYERS =
        Config::LAYERS;

    static constexpr int EMBED_DIM =
        Config::EMBED_DIM;


    // --------------------------------------------------------
    // Input dimension per layer
    //
    // L0:
    //   4096
    //
    // L1+:
    //   EMBED_DIM
    // --------------------------------------------------------

    int input_dim[NUM_LAYERS] = {};


    // --------------------------------------------------------
    // W_src
    //
    // [layer][head]
    //
    // hidden_dim x input_dim[layer]
    // --------------------------------------------------------

    std::unique_ptr<float[]>
        W_src[NUM_LAYERS][NUM_HEADS];


    // --------------------------------------------------------
    // W_tgt
    // --------------------------------------------------------

    std::unique_ptr<float[]>
        W_tgt[NUM_LAYERS][NUM_HEADS];


    // --------------------------------------------------------
    // Attention vector
    //
    // hidden_dim
    // --------------------------------------------------------

    std::unique_ptr<float[]>
        a[NUM_LAYERS][NUM_HEADS];


   GATv2Weights()
{
    for (int l = 0; l < NUM_LAYERS; ++l)
    {
        input_dim[l] = (l == 0) ? FFT_SIZE : EMBED_DIM;

        const std::size_t matrix_size =
            static_cast<std::size_t>(HIDDEN_DIM) *
            static_cast<std::size_t>(input_dim[l]);

        for (int m = 0; m < NUM_HEADS; ++m)
        {
            W_src[l][m] = std::make_unique<float[]>(matrix_size);
            W_tgt[l][m] = std::make_unique<float[]>(matrix_size);
            a[l][m]     = std::make_unique<float[]>(HIDDEN_DIM);
        }
    }
}
};

template<typename Config>
inline bool SaveWeights(
    const GATv2Weights<Config>* w,
    const char* path)
{
    if (!w) return false;

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    constexpr std::uint32_t MAGIC = 0x47415432;
    constexpr std::uint32_t VERSION = 1;
    out.write(reinterpret_cast<const char*>(&MAGIC), sizeof(MAGIC));
    out.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
    out.write(reinterpret_cast<const char*>(&Config::LAYERS), sizeof(int));
    out.write(reinterpret_cast<const char*>(&Config::HEADS),  sizeof(int));
    out.write(reinterpret_cast<const char*>(&Config::HEAD_DIM), sizeof(int));
    out.write(reinterpret_cast<const char*>(&Config::EMBED_DIM),  sizeof(int));
    constexpr int fft_size = FFT_SIZE;
    out.write(reinterpret_cast<const char*>(&fft_size), sizeof(int));

    for (int l = 0; l < Config::LAYERS; ++l)
    {
        const std::size_t matrix_size =
            static_cast<std::size_t>(Config::HEAD_DIM) *
            static_cast<std::size_t>(w->input_dim[l]);

        for (int m = 0; m < Config::HEADS; ++m)
        {
            out.write(reinterpret_cast<const char*>(w->W_src[l][m].get()),
                      matrix_size * sizeof(float));
            out.write(reinterpret_cast<const char*>(w->W_tgt[l][m].get()),
                      matrix_size * sizeof(float));
            out.write(reinterpret_cast<const char*>(w->a[l][m].get()),
                      Config::HEAD_DIM * sizeof(float));
        }
    }
    return out.good();
}

template<typename Config>
inline bool LoadWeights(
    GATv2Weights<Config>* w,
    const char* path)
{
    if (!w) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    std::uint32_t magic = 0, version = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (magic != 0x47415432 || version != 1) return false;

    int num_layers = 0, num_heads = 0, hidden_dim = 0, embed_dim = 0, fft_size = 0;
    in.read(reinterpret_cast<char*>(&num_layers), sizeof(int));
    in.read(reinterpret_cast<char*>(&num_heads),  sizeof(int));
    in.read(reinterpret_cast<char*>(&hidden_dim), sizeof(int));
    in.read(reinterpret_cast<char*>(&embed_dim),  sizeof(int));
    in.read(reinterpret_cast<char*>(&fft_size),   sizeof(int));

    if (num_layers != Config::LAYERS || num_heads != Config::HEADS ||
        hidden_dim != Config::HEAD_DIM || embed_dim != Config::EMBED_DIM ||
        fft_size != FFT_SIZE)
        return false;

    for (int l = 0; l < Config::LAYERS; ++l)
    {
        const std::size_t matrix_size =
            static_cast<std::size_t>(Config::HEAD_DIM) *
            static_cast<std::size_t>(w->input_dim[l]);

        for (int m = 0; m < Config::HEADS; ++m)
        {
            in.read(reinterpret_cast<char*>(w->W_src[l][m].get()),
                    matrix_size * sizeof(float));
            in.read(reinterpret_cast<char*>(w->W_tgt[l][m].get()),
                    matrix_size * sizeof(float));
            in.read(reinterpret_cast<char*>(w->a[l][m].get()),
                    Config::HEAD_DIM * sizeof(float));
        }
    }
    return in.good();
}

template<typename Config>
inline void GenerateLLortho(GATv2Weights<Config>* weights)
{
    if (!weights) return;

    std::mt19937 rng(42);
    std::normal_distribution<float> normal(0.0f, 1.0f);

    const auto init_llortho =
        [&rng, &normal](float* matrix, int rows, int cols)
    {
        for (int r = 0; r < rows; ++r)
        {
            float* row = matrix + static_cast<std::size_t>(r) * cols;

            for (int c = 0; c < cols; ++c)
                row[c] = normal(rng);

            for (int pass = 0; pass < 2; ++pass)
                for (int prev = 0; prev < r; ++prev)
                {
                    const float* q = matrix +
                        static_cast<std::size_t>(prev) * cols;
                    float dot = 0.0f;
                    for (int c = 0; c < cols; ++c)
                        dot += row[c] * q[c];
                    for (int c = 0; c < cols; ++c)
                        row[c] -= dot * q[c];
                }

            float norm_sq = 0.0f;
            for (int c = 0; c < cols; ++c)
                norm_sq += row[c] * row[c];
            float norm = std::sqrt(norm_sq);

            while (!(norm > 1.0e-12f) || !std::isfinite(norm))
            {
                for (int c = 0; c < cols; ++c)
                    row[c] = normal(rng);
                for (int pass = 0; pass < 2; ++pass)
                    for (int prev = 0; prev < r; ++prev)
                    {
                        const float* q = matrix +
                            static_cast<std::size_t>(prev) * cols;
                        float dot = 0.0f;
                        for (int c = 0; c < cols; ++c)
                            dot += row[c] * q[c];
                        for (int c = 0; c < cols; ++c)
                            row[c] -= dot * q[c];
                    }
                norm_sq = 0.0f;
                for (int c = 0; c < cols; ++c)
                    norm_sq += row[c] * row[c];
                norm = std::sqrt(norm_sq);
            }

            const float inv_norm = 1.0f / norm;
            for (int c = 0; c < cols; ++c)
                row[c] *= inv_norm;
        }
    };

    for (int l = 0; l < Config::LAYERS; ++l)
    {
        const int in_dim = (l == 0) ? FFT_SIZE : Config::EMBED_DIM;

        for (int m = 0; m < Config::HEADS; ++m)
        {
            init_llortho(weights->W_src[l][m].get(), Config::HEAD_DIM, in_dim);
            init_llortho(weights->W_tgt[l][m].get(), Config::HEAD_DIM, in_dim);
            std::memset(weights->a[l][m].get(), 0,
                        Config::HEAD_DIM * sizeof(float));
        }
    }
}

// ============================================================
// Workspace
// ============================================================

template<typename Config>
struct GATv2Workspace
{
    static constexpr int EMBED_DIM =
        Config::EMBED_DIM;


    int max_neighbors = 0;
    int embed_dim     = 0;


    std::unique_ptr<float[]>
        neighbor_a;


    std::unique_ptr<float[]>
        neighbor_b;

    std::unique_ptr<float[]> neighbor_input;

    float attention_scores[
        MAX_NEIGHBORS
    ] = {};


    bool init(
        int max_neighbors_
    )
    {
        if (max_neighbors_ <= 0 ||
            max_neighbors_ > MAX_NEIGHBORS)
        {
            return false;
        }


        max_neighbors =
            max_neighbors_;


        embed_dim =
            EMBED_DIM;


        const std::size_t count =
            static_cast<std::size_t>(
                max_neighbors
            ) *
            static_cast<std::size_t>(
                embed_dim
            );


        
try
{
    neighbor_a =
        std::make_unique<float[]>(count);

    neighbor_b =
        std::make_unique<float[]>(count);

    neighbor_input =
        std::make_unique<float[]>(
            static_cast<std::size_t>(max_neighbors) *
            FFT_SIZE
        );
}
catch (...)
{
    neighbor_a.reset();
    neighbor_b.reset();
    neighbor_input.reset();

    max_neighbors = 0;
    embed_dim = 0;

    return false;
}



        std::memset(
            attention_scores,
            0,
            sizeof(attention_scores)
        );


        return true;
    }


    float*
    buffer_a() noexcept
    {
        return neighbor_a.get();
    }


    float*
    buffer_b() noexcept
    {
        return neighbor_b.get();
    }


    const float*
    buffer_a() const noexcept
    {
        return neighbor_a.get();
    }


    const float*
    buffer_b() const noexcept
    {
        return neighbor_b.get();
    }
};


// ============================================================
// GATv2Layer
// ============================================================

template<typename Config>
struct GATv2Layer
{
    GATv2GpuContext* gpu       = nullptr;   // NEW

    using Weights =
        GATv2Weights<Config>;


    static constexpr int NUM_HEADS =
        Config::HEADS;

    static constexpr int HIDDEN_DIM =
        Config::HEAD_DIM;

    static constexpr int EMBED_DIM =
        Config::EMBED_DIM;


    const Weights*
        weights = nullptr;


    int layer_idx = 0;

    int input_dim = 0;


    // ========================================================
    // Dot product
    // ========================================================


static inline float DotFloat(
    const float* __restrict row,
    const float* __restrict input,
    int count
) noexcept
{
    using f32x = sx::native_simd<float>;

    constexpr int width =
        static_cast<int>(f32x::size());

    f32x sum(0.0f);

    int i = 0;

    #pragma GCC unroll 2
    for (; i + width <= count; i += width)
    {
        f32x weights;
        weights.copy_from(
            row + i,
            sx::element_aligned
        );

        f32x values;
        values.copy_from(
            input + i,
            sx::element_aligned
        );

        sum += weights * values;
    }

    float result =
        sx::reduce(sum, std::plus<>{});

    #pragma GCC unroll 4
    for (; i < count; ++i)
    {
        result += row[i] * input[i];
    }

    return result;
}


    // ========================================================
    // LeakyReLU
    // ========================================================

    static inline float LeakyReLU(
        float x
    ) noexcept
    {
        return
            x >= 0.0f
                ? x
                : GAT_LEAKY_SLOPE * x;
    }


    // ========================================================
    // Forward
    // ========================================================

    
bool forward(
    const float* node_input,
    const float* const neighbor_inputs[],
    int num_neighbors,
    float* output,
    float* neighbor_output,
    float* attention_scores,
    float* packed_neighbors
) const noexcept
{
    if (weights == nullptr ||
        node_input == nullptr ||
        output == nullptr ||
        neighbor_output == nullptr ||
        attention_scores == nullptr)
    {
        return false;
    }

    if (num_neighbors < 0 ||
        num_neighbors > MAX_NEIGHBORS)
    {
        return false;
    }

    if (num_neighbors == 0)
    {
        std::memset(
            output,
            0,
            static_cast<std::size_t>(EMBED_DIM) *
                sizeof(float)
        );

        return true;
    }

    if (neighbor_inputs == nullptr)
        return false;

    for (int j = 0; j < num_neighbors; ++j)
    {
        if (neighbor_inputs[j] == nullptr)
            return false;
    }

    std::memset(
        output,
        0,
        static_cast<std::size_t>(EMBED_DIM) *
            sizeof(float)
    );

    // h_src_gpu zawiera wszystkie głowy:
    // [head][dimension].
    alignas(64) float h_src_gpu[EMBED_DIM];

    // --------------------------------------------------------
    // Projekcje GPU wszystkich głów albo CPU fallback.
    // --------------------------------------------------------

    if (gpu != nullptr)
    {
        if (packed_neighbors == nullptr)
            return false;

        // Przygotuj ciągły bufor [neighbor][input_dim].
        // L0: [N][4096]
        // L1/L2: [N][512]
        for (int j = 0; j < num_neighbors; ++j)
        {
            std::memcpy(
                packed_neighbors +
                    static_cast<std::size_t>(j) * input_dim,
                neighbor_inputs[j],
                static_cast<std::size_t>(input_dim) *
                    sizeof(float)
            );
        }

        // Wymaga kontekstu GPU zawierającego wagi
        // wszystkich głów tej warstwy.
        if (!gatv2_gpu_run(
                gpu,
                node_input,
                packed_neighbors,
                num_neighbors,
                h_src_gpu,
                neighbor_output))
        {
            return false;
        }
    }
    else
    {
        for (int m = 0; m < NUM_HEADS; ++m)
        {
            const float* Wsrc =
                weights->W_src[layer_idx][m].get();

            const float* Wtgt =
                weights->W_tgt[layer_idx][m].get();

            if (Wsrc == nullptr || Wtgt == nullptr)
                return false;

            float* h_src =
                h_src_gpu +
                static_cast<std::size_t>(m) * HIDDEN_DIM;

            for (int d = 0; d < HIDDEN_DIM; ++d)
            {
                const float* row =
                    Wsrc +
                    static_cast<std::size_t>(d) *
                        input_dim;

                h_src[d] = DotFloat(
                    row,
                    node_input,
                    input_dim
                );
            }

            for (int j = 0; j < num_neighbors; ++j)
            {
                float* projected =
                    neighbor_output +
                    static_cast<std::size_t>(j) * EMBED_DIM +
                    static_cast<std::size_t>(m) * HIDDEN_DIM;

                const float* nj = neighbor_inputs[j];

                for (int d = 0; d < HIDDEN_DIM; ++d)
                {
                    const float* row =
                        Wtgt +
                        static_cast<std::size_t>(d) *
                            input_dim;

                    projected[d] = DotFloat(
                        row,
                        nj,
                        input_dim
                    );
                }
            }
        }
    }

    // --------------------------------------------------------
    // Attention, softmax i agregacja nadal na CPU.
    // --------------------------------------------------------

    for (int m = 0; m < NUM_HEADS; ++m)
    {
        const float* h_src =
            h_src_gpu +
            static_cast<std::size_t>(m) * HIDDEN_DIM;

        const float* a_m =
            weights->a[layer_idx][m].get();

        if (a_m == nullptr)
            return false;

        // Attention scores.
        for (int j = 0; j < num_neighbors; ++j)
        {
            const float* h_tgt =
                neighbor_output +
                static_cast<std::size_t>(j) * EMBED_DIM +
                static_cast<std::size_t>(m) * HIDDEN_DIM;

            float logit = 0.0f;

            for (int d = 0; d < HIDDEN_DIM; ++d)
            {
                const float preact =
                    h_src[d] + h_tgt[d];

                logit +=
                    a_m[d] * LeakyReLU(preact);
            }

            attention_scores[j] = logit;
        }

        // Stable softmax.
        float max_score = attention_scores[0];

        for (int j = 1; j < num_neighbors; ++j)
        {
            max_score = std::max(
                max_score,
                attention_scores[j]
            );
        }

        float exp_sum = 0.0f;

        for (int j = 0; j < num_neighbors; ++j)
        {
            const float e = std::exp(
                attention_scores[j] - max_score
            );

            attention_scores[j] = e;
            exp_sum += e;
        }

        if (!(exp_sum > 0.0f) || !std::isfinite(exp_sum))
            return false;

        const float inv_exp_sum = 1.0f / exp_sum;

        for (int j = 0; j < num_neighbors; ++j)
        {
            attention_scores[j] *= inv_exp_sum;
        }

        // Weighted aggregation.
        float* head_output =
            output +
            static_cast<std::size_t>(m) * HIDDEN_DIM;

        for (int j = 0; j < num_neighbors; ++j)
        {
            const float alpha = attention_scores[j];

            const float* projected =
                neighbor_output +
                static_cast<std::size_t>(j) * EMBED_DIM +
                static_cast<std::size_t>(m) * HIDDEN_DIM;

            for (int d = 0; d < HIDDEN_DIM; ++d)
            {
                head_output[d] += alpha * projected[d];
            }
        }
    }

    return true;
}

};
// ============================================================
// GATv2Network
// ============================================================

template<typename Config>
struct GATv2Network
{
    using Weights =
        GATv2Weights<Config>;

    using Layer =
        GATv2Layer<Config>;

    using Workspace =
        GATv2Workspace<Config>;


    static constexpr int NUM_HEADS =
        Config::HEADS;

    static constexpr int HIDDEN_DIM =
        Config::HEAD_DIM;

    static constexpr int NUM_LAYERS =
        Config::LAYERS;

    static constexpr int EMBED_DIM =
        Config::EMBED_DIM;


    std::unique_ptr<Weights>
        weights;


    Layer layers[
        NUM_LAYERS
    ];


    // ========================================================
    // Constructor
    // ========================================================

    GATv2Network() = default;


    // ========================================================
    // Initialization
    // ========================================================

       bool init()
    {
        std::unique_ptr<Weights> new_weights;

        try
        {
            new_weights = std::make_unique<Weights>();
        }
        catch (...)
        {
            return false;
        }

        weights = std::move(new_weights);

        // ----------------------------------------------------
        // Configure layers
        // ----------------------------------------------------
        for (int l = 0; l < NUM_LAYERS; ++l)
        {
            layers[l].weights   = weights.get();
            layers[l].layer_idx = l;
            layers[l].input_dim = weights->input_dim[l];
        }

        
        // ====================================================
        // Weights: load from file, or generate LLortho (once)
        // ====================================================
        if (load_weights(KOALA_MODEL_PATH))
        {
            std::cerr << "[GATv2] Weights loaded from "
                      << KOALA_MODEL_PATH << "\n";
        }
        else
        {
            std::cerr << "[GATv2] No weights file. Generating LLortho...\n";

            GenerateLLortho(weights.get());

            if (save_weights(KOALA_MODEL_PATH))
            {
                std::cerr << "[GATv2] LLortho saved to "
                          << KOALA_MODEL_PATH << "\n";
            }
            else
            {
                std::cerr << "[GATv2] ERROR: failed to save "
                          << KOALA_MODEL_PATH << "\n";
            }
        }

        // ====================================================
        // GPU: create contexts with current weights
        // ====================================================
        std::cerr << "[GATv2] gatv2_gpu_available() = "
                  << gatv2_gpu_available() << "\n";
                  

if (gatv2_gpu_available() > 0)

    {
        bool all_ok = true;

        for (int l = 0; l < NUM_LAYERS && all_ok; ++l)
        {
            const float* src_heads[NUM_HEADS];
            const float* tgt_heads[NUM_HEADS];

            for (int m = 0; m < NUM_HEADS; ++m)
            {
                src_heads[m] = weights->W_src[l][m].get();
                tgt_heads[m] = weights->W_tgt[l][m].get();
            }

            GATv2GpuContext* gpu = gatv2_gpu_create(
                src_heads,
                tgt_heads,
                weights->input_dim[l],
                NUM_HEADS,
                HIDDEN_DIM,
                MAX_NEIGHBORS
            );

            if (gpu == nullptr)
            {
                std::cerr << "[GATv2] GPU init failed at layer "
                          << l << "\n";
                all_ok = false;
                break;
            }

            layers[l].gpu = gpu;

            std::cerr << "[GATv2] GPU layer " << l
                      << " ready (K=" << weights->input_dim[l]
                      << ", H=" << NUM_HEADS
                      << ", D=" << HIDDEN_DIM << ")\n";
        }

        if (!all_ok)
        {
            for (int l = 0; l < NUM_LAYERS; ++l)
            {
                if (layers[l].gpu != nullptr)
                {
                    gatv2_gpu_destroy(layers[l].gpu);
                    layers[l].gpu = nullptr;
                }
            }
            std::cerr << "[GATv2] GPU disabled, CPU only\n";
        }
    }
    else
    {
        std::cerr << "[GATv2] no GPU, CPU only\n";
    }

    return true;
}



    bool save_weights(const char* path) const
{
    return SaveWeights(weights.get(), path);
}

bool load_weights(const char* path)
{
    return LoadWeights(weights.get(), path);
}



~GATv2Network()
{
    for (int l = 0; l < NUM_LAYERS; ++l)
    {
        if (layers[l].gpu != nullptr)
        {
            gatv2_gpu_destroy(layers[l].gpu);
            layers[l].gpu = nullptr;
        }
    }
}


    // ========================================================
    // Forward
    //
    // node_fft:
    //
    //   4096 float
    //
    // neighbors_fft:
    //
    //   pointers to 4096-float FFT representations
    //
    // output:
    //
    //   EMBED_DIM float
    // ========================================================

    bool forward(
        const float* node_fft,
        const float* const neighbors_fft[],
        int num_neighbors,
        float* output,
        Workspace& workspace
    ) const noexcept
    {
        if (weights == nullptr)
        {
            return false;
        }


        if (node_fft == nullptr ||
            output == nullptr)
        {
            return false;
        }


        if (num_neighbors < 0 ||
            num_neighbors > MAX_NEIGHBORS)
        {
            return false;
        }


        if (workspace.max_neighbors <
            num_neighbors)
        {
            return false;
        }


        if (workspace.embed_dim <
            EMBED_DIM)
        {
            return false;
        }


        // ----------------------------------------------------
        // Validate initial neighbors
        // ----------------------------------------------------

        if (num_neighbors > 0 &&
            neighbors_fft == nullptr)
        {
            return false;
        }


        for (int j = 0;
             j < num_neighbors;
             ++j)
        {
            if (neighbors_fft[j] == nullptr)
            {
                return false;
            }
        }


        // ====================================================
        // IMPORTANT:
        //
        // L0 does NOT need a copy of node_fft.
        //
        // We simply point layer_input at node_fft.
        // ====================================================

        const float*
            layer_input =
                node_fft;


        // ----------------------------------------------------
        // Only EMBED_DIM is needed for subsequent layers.
        // ----------------------------------------------------

        alignas(64)
        float layer_output[
            EMBED_DIM
        ];


        alignas(64)
        float next_layer_input[
            EMBED_DIM
        ];


        // ====================================================
        // Neighbor ping-pong buffers
        // ====================================================

        float*
            current_neighbor_buffer =
                nullptr;


        float*
            next_neighbor_buffer =
                workspace.buffer_a();


        // ====================================================
        // Neighbor pointer array
        // ====================================================

        const float*
            layer_neighbor_inputs[
                MAX_NEIGHBORS
            ];


        // ====================================================
        // Layers
        // ====================================================

        for (int l = 0;
             l < NUM_LAYERS;
             ++l)
        {
            // ------------------------------------------------
            // Prepare neighbor pointers
            // ------------------------------------------------

            if (l == 0)
            {
                for (int j = 0;
                     j < num_neighbors;
                     ++j)
                {
                    layer_neighbor_inputs[j] =
                        neighbors_fft[j];
                }
            }
            else
            {
                for (int j = 0;
                     j < num_neighbors;
                     ++j)
                {
                    layer_neighbor_inputs[j] =
                        current_neighbor_buffer +
                        static_cast<std::size_t>(j) *
                        static_cast<std::size_t>(
                            EMBED_DIM
                        );
                }
            }


            // ------------------------------------------------
            // Clear output
            // ------------------------------------------------

            std::memset(
                layer_output,
                0,
                sizeof(layer_output)
            );


            // ------------------------------------------------
            // GAT layer
            // ------------------------------------------------

            const bool ok =
                layers[l].forward(
                    layer_input,
                    layer_neighbor_inputs,
                    num_neighbors,
                    layer_output,
                    next_neighbor_buffer,
                    workspace.attention_scores,
                    workspace.neighbor_input.get()
                );


            if (!ok)
            {
                return false;
            }


            // =================================================
            // Residual
            //
            // L0:
            //
            //   4096 -> 512
            //
            // no residual.
            //
            // L1+:
            //
            //   512 -> 512
            //
            // residual enabled.
            // =================================================

            if (weights->
                input_dim[l] ==
                EMBED_DIM)
            {
                constexpr float
                    RESIDUAL_SCALE =
                        0.10f;

                #pragma GCC unroll 8
                for (int i = 0;
                     i < EMBED_DIM;
                     ++i)
                {
                    layer_output[i] +=
                        RESIDUAL_SCALE *
                        layer_input[i];
                }
            }


            // =================================================
            // After L0 the representation is EMBED_DIM.
            //
            // Copy only 512 floats instead of 4096.
            // =================================================

            std::memcpy(
                next_layer_input,
                layer_output,
                sizeof(next_layer_input)
            );


            layer_input =
                next_layer_input;


            // ------------------------------------------------
            // Neighbor representation
            // ------------------------------------------------

            current_neighbor_buffer =
                next_neighbor_buffer;


            // ------------------------------------------------
            // Ping-pong
            // ------------------------------------------------

            if (next_neighbor_buffer ==
                workspace.buffer_a())
            {
                next_neighbor_buffer =
                    workspace.buffer_b();
            }
            else
            {
                next_neighbor_buffer =
                    workspace.buffer_a();
            }
        }


        // ====================================================
        // Final embedding
        // ====================================================

        std::memcpy(
            output,
            layer_input,
            static_cast<std::size_t>(
                EMBED_DIM
            ) *
            sizeof(float)
        );


        return true;
    }
};


// ============================================================
// Default aliases
// ============================================================

using DefaultGATv2Network =
    GATv2Network<
        DefaultGATv2Config
    >;


using DefaultGATv2Workspace =
    GATv2Workspace<
        DefaultGATv2Config
    >;


// ============================================================
// Convenience helpers
// ============================================================

inline bool InitDefaultGATv2(
    DefaultGATv2Network& network
)
{
    return network.init();
}


inline bool InitDefaultGATv2Workspace(
    DefaultGATv2Workspace& workspace
)
{
    return workspace.init(
        MAX_NEIGHBORS
    );
}