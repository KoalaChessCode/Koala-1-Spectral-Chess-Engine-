
#include "GATv2Kernels.hpp"

#include <omp.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <cstring>
#include <memory>

// ============================================================
// GATv2 GPU context
//
// d_W_both: [2*HD][K]
//   [0, HD)    = W_src
//   [HD, 2*HD) = W_tgt
//
// d_X: [max_neighbors+1][K]
//   [0]    = node
//   [1..N] = neighbors
//
// d_Y: [max_neighbors+1][HD]
//   [0]    = h_src
//   [1..N] = neighbor_out
//
// Wagi są wgrywane raz.
// Per wywołanie: 1 transfer H->D, 1 kernel, 1 transfer D->H.
// ============================================================

struct GATv2GpuContext
{
    int device = -1;
    int host_device = -1;

    int K = 0;
    int H = 0;
    int D = 0;
    int HD = 0;
    int max_neighbors = 0;

    std::size_t weight_both_count = 0;
    std::size_t input_capacity = 0;
    std::size_t output_capacity = 0;

    float* d_W_both = nullptr;
    float* d_X = nullptr;
    float* d_Y = nullptr;

    std::unique_ptr<float[]> h_X_staging;
    std::unique_ptr<float[]> h_Y_staging;
};

// ============================================================
// Safe size calculations
// ============================================================

static bool checked_mul(
    std::size_t a,
    std::size_t b,
    std::size_t& result
) noexcept
{
    if (a != 0 &&
        b > std::numeric_limits<std::size_t>::max() / a)
    {
        return false;
    }

    result = a * b;
    return true;
}

static bool checked_float_bytes(
    std::size_t count,
    std::size_t& bytes
) noexcept
{
    return checked_mul(count, sizeof(float), bytes);
}

// ============================================================
// Device allocation helpers
// ============================================================

static float* allocate_device_floats(
    std::size_t count,
    int device
) noexcept
{
    std::size_t bytes = 0;

    if (count == 0 ||
        !checked_float_bytes(count, bytes))
    {
        return nullptr;
    }

    return static_cast<float*>(omp_target_alloc(bytes, device));
}

static void free_device_buffer(
    float*& ptr,
    int device
) noexcept
{
    if (ptr != nullptr)
    {
        omp_target_free(ptr, device);
        ptr = nullptr;
    }
}

static bool copy_host_to_device(
    float* dst,
    const float* src,
    std::size_t count,
    int device,
    int host_device
) noexcept
{
    std::size_t bytes = 0;

    if (count == 0)
        return true;

    if (dst == nullptr ||
        src == nullptr ||
        !checked_float_bytes(count, bytes))
    {
        return false;
    }

    return omp_target_memcpy(
        dst,
        src,
        bytes,
        0,
        0,
        device,
        host_device
    ) == 0;
}

static bool copy_device_to_host(
    float* dst,
    const float* src,
    std::size_t count,
    int device,
    int host_device
) noexcept
{
    std::size_t bytes = 0;

    if (count == 0)
        return true;

    if (dst == nullptr ||
        src == nullptr ||
        !checked_float_bytes(count, bytes))
    {
        return false;
    }

    return omp_target_memcpy(
        dst,
        src,
        bytes,
        0,
        0,
        host_device,
        device
    ) == 0;
}

// ============================================================
// SIMT projection kernel
//
// Każdy zespół GPU oblicza jeden wynik:
//
//   Y[n][hd] = sum_k W[row][k] * X[n][k]
//
// row = hd       dla node
// row = HD + hd  dla neighbor
//
// Różnica względem poprzedniej wersji:
//   - teams distribute przydziela wyniki zespołom GPU;
//   - parallel for rozdziela redukcję po K między wątki zespołu;
//   - reduction(+:sum) scala sumy cząstkowe.
//
// To jest wariant eksperymentalny do benchmarku na gfx803.
// ============================================================


template<int K>
static bool project_both_device(
    const float* d_W,
    const float* d_X,
    float* d_Y,
    int HD,
    int N,
    int device
) noexcept
{
    if (N < 0 ||
        N == std::numeric_limits<int>::max() ||
        HD <= 0 ||
        d_W == nullptr ||
        d_X == nullptr ||
        d_Y == nullptr)
    {
        return false;
    }

    const int total = N + 1;

    #pragma omp target teams distribute collapse(2) \
        device(device) is_device_ptr(d_W, d_X, d_Y) \
        thread_limit(256)
    for (int n = 0; n < total; ++n)
    {
        for (int hd = 0; hd < HD; ++hd)
        {
            const int row_hd = (n == 0) ? hd : (HD + hd);

            const float* __restrict row =
                d_W + static_cast<std::size_t>(row_hd) *
                      static_cast<std::size_t>(K);

            const float* __restrict x =
                d_X + static_cast<std::size_t>(n) *
                      static_cast<std::size_t>(K);

            float sum = 0.0f;

            #pragma omp parallel for reduction(+:sum) num_threads(256)
            for (int k = 0; k < K; ++k)
            {
                sum += row[k] * x[k];
            }

            d_Y[
                static_cast<std::size_t>(n) *
                static_cast<std::size_t>(HD) +
                static_cast<std::size_t>(hd)
            ] = sum;
        }
    }

    return true;
}


// ============================================================
// Internal cleanup
// ============================================================

static void release_context(
    GATv2GpuContext* ctx
) noexcept
{
    if (ctx == nullptr)
        return;

    if (ctx->device >= 0)
    {
        free_device_buffer(ctx->d_W_both, ctx->device);
        free_device_buffer(ctx->d_X, ctx->device);
        free_device_buffer(ctx->d_Y, ctx->device);
    }

    ctx->h_X_staging.reset();
    ctx->h_Y_staging.reset();
}

// ============================================================
// Create context
// ============================================================

extern "C"
GATv2GpuContext* gatv2_gpu_create(
    const float* const W_heads_src[],
    const float* const W_heads_tgt[],
    int K,
    int H,
    int D,
    int max_neighbors
) noexcept
{
    if (W_heads_src == nullptr ||
        W_heads_tgt == nullptr ||
        K <= 0 ||
        H <= 0 ||
        D <= 0 ||
        max_neighbors <= 0 ||
        max_neighbors == std::numeric_limits<int>::max())
    {
        return nullptr;
    }

    for (int m = 0; m < H; ++m)
    {
        if (W_heads_src[m] == nullptr ||
            W_heads_tgt[m] == nullptr)
        {
            return nullptr;
        }
    }

    const int device = omp_get_default_device();
    const int host_device = omp_get_initial_device();

    if (device == host_device ||
        device < 0 ||
        device >= omp_get_num_devices())
    {
        return nullptr;
    }

    std::size_t HD_size = 0;
    std::size_t head_size = 0;
    std::size_t weight_single_count = 0;
    std::size_t weight_both_count = 0;
    std::size_t input_capacity = 0;
    std::size_t output_capacity = 0;

    if (!checked_mul(
            static_cast<std::size_t>(H),
            static_cast<std::size_t>(D),
            HD_size) ||
        HD_size > static_cast<std::size_t>(
            std::numeric_limits<int>::max()) ||
        !checked_mul(
            static_cast<std::size_t>(D),
            static_cast<std::size_t>(K),
            head_size) ||
        !checked_mul(
            static_cast<std::size_t>(H),
            head_size,
            weight_single_count) ||
        !checked_mul(
            weight_single_count,
            2,
            weight_both_count) ||
        !checked_mul(
            static_cast<std::size_t>(max_neighbors) + 1,
            static_cast<std::size_t>(K),
            input_capacity) ||
        !checked_mul(
            static_cast<std::size_t>(max_neighbors) + 1,
            HD_size,
            output_capacity))
    {
        return nullptr;
    }

    std::size_t ignored = 0;

    if (!checked_float_bytes(weight_both_count, ignored) ||
        !checked_float_bytes(input_capacity, ignored) ||
        !checked_float_bytes(output_capacity, ignored))
    {
        return nullptr;
    }

    GATv2GpuContext* ctx =
        new (std::nothrow) GATv2GpuContext{};

    if (ctx == nullptr)
        return nullptr;

    ctx->device = device;
    ctx->host_device = host_device;
    ctx->K = K;
    ctx->H = H;
    ctx->D = D;
    ctx->HD = static_cast<int>(HD_size);
    ctx->max_neighbors = max_neighbors;

    ctx->weight_both_count = weight_both_count;
    ctx->input_capacity = input_capacity;
    ctx->output_capacity = output_capacity;

    // ---- Host staging ----
    try
    {
        ctx->h_X_staging =
            std::make_unique<float[]>(input_capacity);

        ctx->h_Y_staging =
            std::make_unique<float[]>(output_capacity);
    }
    catch (...)
    {
        delete ctx;
        return nullptr;
    }

    // ---- Device buffers ----
    ctx->d_W_both =
        allocate_device_floats(weight_both_count, device);

    ctx->d_X =
        allocate_device_floats(input_capacity, device);

    ctx->d_Y =
        allocate_device_floats(output_capacity, device);

    if (ctx->d_W_both == nullptr ||
        ctx->d_X == nullptr ||
        ctx->d_Y == nullptr)
    {
        release_context(ctx);
        delete ctx;
        return nullptr;
    }

    // ---- Sklej W_src i W_tgt w jeden bufor ----
    std::unique_ptr<float[]> staging;

    try
    {
        staging = std::make_unique<float[]>(weight_both_count);
    }
    catch (...)
    {
        release_context(ctx);
        delete ctx;
        return nullptr;
    }

    // W_src: pierwsze HD wierszy.
    for (int m = 0; m < H; ++m)
    {
        std::memcpy(
            staging.get() +
                static_cast<std::size_t>(m) * head_size,
            W_heads_src[m],
            head_size * sizeof(float)
        );
    }

    // W_tgt: kolejne HD wierszy.
    for (int m = 0; m < H; ++m)
    {
        std::memcpy(
            staging.get() +
                weight_single_count +
                static_cast<std::size_t>(m) * head_size,
            W_heads_tgt[m],
            head_size * sizeof(float)
        );
    }

    if (!copy_host_to_device(
            ctx->d_W_both,
            staging.get(),
            weight_both_count,
            device,
            host_device))
    {
        release_context(ctx);
        delete ctx;
        return nullptr;
    }

    return ctx;
}

// ============================================================
// Run projections
// ============================================================

extern "C"
int gatv2_gpu_run(
    GATv2GpuContext* ctx,
    const float* node,
    const float* neighbors,
    int N,
    float* h_src,
    float* neighbor_out
) noexcept
{
    if (ctx == nullptr ||
        ctx->device < 0 ||
        ctx->d_W_both == nullptr ||
        ctx->d_X == nullptr ||
        ctx->d_Y == nullptr ||
        ctx->h_X_staging == nullptr ||
        ctx->h_Y_staging == nullptr ||
        node == nullptr ||
        h_src == nullptr ||
        N < 0 ||
        N > ctx->max_neighbors)
    {
        return 0;
    }

    if (N > 0 &&
        (neighbors == nullptr || neighbor_out == nullptr))
    {
        return 0;
    }

    const int K = ctx->K;
    const int HD = ctx->HD;
    const int device = ctx->device;
    const int host_device = ctx->host_device;

    const std::size_t total =
        static_cast<std::size_t>(N) + 1;

    // ---- 1. Zbierz node i neighbors w staging ----
    float* h_X = ctx->h_X_staging.get();

    std::memcpy(
        h_X,
        node,
        static_cast<std::size_t>(K) * sizeof(float)
    );

    if (N > 0)
    {
        std::memcpy(
            h_X + static_cast<std::size_t>(K),
            neighbors,
            static_cast<std::size_t>(N) *
                static_cast<std::size_t>(K) *
                sizeof(float)
        );
    }

    // ---- 2. Jeden transfer host -> device ----
    const std::size_t input_count =
        total * static_cast<std::size_t>(K);

    if (input_count > ctx->input_capacity)
        return 0;

    if (!copy_host_to_device(
            ctx->d_X,
            h_X,
            input_count,
            device,
            host_device))
    {
        return 0;
    }

    // ---- 3. Kooperacyjny kernel SIMT ----
    bool ok = false;

if (ctx->K == 4096)
{
    ok = project_both_device<4096>(
        ctx->d_W_both, ctx->d_X, ctx->d_Y,
        ctx->HD, N, ctx->device
    );
}
else if (ctx->K == 512)
{
    ok = project_both_device<512>(
        ctx->d_W_both, ctx->d_X, ctx->d_Y,
        ctx->HD, N, ctx->device
    );
}

if (!ok)
    return 0;

    // ---- 4. Jeden transfer device -> host ----
    const std::size_t output_count =
        total * static_cast<std::size_t>(HD);

    if (output_count > ctx->output_capacity)
        return 0;

    float* h_Y = ctx->h_Y_staging.get();

    if (!copy_device_to_host(
            h_Y,
            ctx->d_Y,
            output_count,
            device,
            host_device))
    {
        return 0;
    }

    // ---- 5. Rozdziel wyniki ----
    std::memcpy(
        h_src,
        h_Y,
        static_cast<std::size_t>(HD) * sizeof(float)
    );

    if (N > 0)
    {
        std::memcpy(
            neighbor_out,
            h_Y + static_cast<std::size_t>(HD),
            static_cast<std::size_t>(N) *
                static_cast<std::size_t>(HD) *
                sizeof(float)
        );
    }

    return 1;
}

// ============================================================
// Destroy context
// ============================================================

extern "C"
void gatv2_gpu_destroy(
    GATv2GpuContext* ctx
) noexcept
{
    if (ctx == nullptr)
        return;

    release_context(ctx);
    delete ctx;
}

// ============================================================
// Availability
// ============================================================

extern "C"
int gatv2_gpu_available() noexcept
{
    const int n = omp_get_num_devices();

    if (n <= 0)
        return 0;

    const int device = omp_get_default_device();
    const int host = omp_get_initial_device();

    if (device == host || device < 0)
        return 0;

    return n;
}
