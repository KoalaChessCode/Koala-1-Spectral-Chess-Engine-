#include "Constants.hpp"
#include "MoveGen.hpp"
#include "Legal.hpp"
#include "Tree.hpp"
#include "GATv2Wrapper.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <array>

struct SearchTimeLimits
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point soft_deadline;
    Clock::time_point hard_deadline;
    std::chrono::milliseconds budget{0};
    std::chrono::milliseconds soft{0};
};

struct TimeManagementParameters
{
    static constexpr double eta = 0.8;
    static constexpr double kappa = 1.5;
    static constexpr double q = 0.5;
    static constexpr double T0 = 60000.0;
    static constexpr double alpha = 0.20;
    static constexpr std::int64_t reserve_ms = 100;
    static constexpr std::int64_t default_moves_to_go = 30;
};

inline SearchTimeLimits MakeSearchTimeLimits(
    std::int64_t remaining_ms,
    std::int64_t increment_ms,
    std::int64_t moves_to_go
)
{
    using namespace std::chrono;
    const double T = static_cast<double>(std::max<std::int64_t>(0, remaining_ms));
    const double I = static_cast<double>(std::max<std::int64_t>(0, increment_ms));
    const double H = static_cast<double>(moves_to_go > 0
        ? moves_to_go : TimeManagementParameters::default_moves_to_go);
    // Bayesian PUCT uncertainty falls as 1/sqrt(N); logarithmic relaxation
    // increases the time share sublinearly as more clock time is available.
    const double phi = 1.0 + TimeManagementParameters::alpha *
        std::log(1.0 + T / TimeManagementParameters::T0);
    const double base_relaxed = phi *
        (T / (H + TimeManagementParameters::kappa))
        + TimeManagementParameters::eta * I;
    const double budget = std::max(0.0, std::min({
        base_relaxed,
        TimeManagementParameters::q * T,
        T - static_cast<double>(TimeManagementParameters::reserve_ms)
    }));
    const auto budget_ms = milliseconds(static_cast<std::int64_t>(budget));
    const auto soft = milliseconds(static_cast<std::int64_t>(budget * 0.8));
    const auto now = SearchTimeLimits::Clock::now();
    return {now + soft, now + budget_ms, budget_ms, soft};
}

// ===========================================================================
// SAVE DELTA DURING EXPANSION
// ===========================================================================
//
// IMPORTANT:
//
// parentBoard points to the Board stored in the parent's Node.
//
// MakeMove changes InstanceOne.board,
// but does NOT change parentBoard.
//
// Therefore, after MakeMove we have:
//
// parentBoard = parent state
// InstanceOne.board = child state
//
// and we can calculate the exact delta.
//
// ===========================================================================

template<
    bool White,
    typename GameStateType
>
inline void StoreChildWithFftDelta(
    Tree<Board>& tree,
    std::uint32_t parentIndex,
    std::uint32_t childIndex,
    GameStateType& instance,
    const Move& move
)
{
    const Board& parentBoard =
        tree.At(parentIndex).position;

    StateUndo undo =
        MakeMove<White>(
            instance,
            move
        );

    tree.At(childIndex).position =
        instance.board;

    tree.moves[childIndex] =
        move;

    tree.At(childIndex).fftDelta =
        MakeFftDelta<4>(
            parentBoard,
            instance.board
        );

    UnmakeMove<White>(
        instance,
        move,
        undo
    );
}

template<typename T, typename Y, typename U>
void AllWhiteLegalMoves(
    T& Wmoves,
    int& Wcount,
    Y& Inst,
    U& PawnMem
)
{
    WhitePiecesMovesGen(
        Wmoves,
        Wcount,
        Inst,
        PawnMem
    );

    Wcount = FilterLegalMoves<true>(
        Wmoves.WhiteMoves,
        Wcount,
        Inst
    );

    Wmoves.Count = static_cast<U8>(Wcount);
}

template<typename A, typename B, typename C>
void AllBlackLegalMoves(C& Bmoves,int& Bcount,A& Inst, B& PawnMem) {
    BlackPiecesMovesGen(
    Bmoves,
    Bcount,
    Inst,
    PawnMem
);  


    Bcount = FilterLegalMoves<false>(
        Bmoves.BlackMoves,
        Bcount,
        Inst
    );



    Bmoves.Count = static_cast<U8>(Bcount);
}


inline void RunDelayedPUCTWorker(
    TreeStateWrapper& St,
    PipelineContext& ctx,
    SearchTimeLimits::Clock::time_point hard_deadline,
    std::atomic<std::uint64_t>& puct_elapsed_ns,
    std::atomic<bool>& puct_did_run
)
{
    {
        std::unique_lock<std::mutex> lock(ctx.puct_mutex);
        ctx.puct_cv.wait(lock, [&] { return ctx.puct_done; });
    }

    // DFS/GAT/FFT use shared mutable state. Wait for the complete DFS pass
    // before reading the tree or updating PUCT statistics. This full-DFS
    // barrier avoids races on ancestors and shared FFT/GAT work buffers.
    if (ctx.puct_ready[0] && St.tree.childCount[0] != 0 &&
        SearchTimeLimits::Clock::now() < hard_deadline) {
        const auto started = SearchTimeLimits::Clock::now();
        const uint32_t best = RunPUCT(
            St, ctx.puct_states, ctx.puct_iterations, 1.0f, 1.0f, 0,
            ctx.puct_ready, hard_deadline);
        puct_did_run.store(best != UINT32_MAX, std::memory_order_relaxed);
        puct_elapsed_ns.fetch_add(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                SearchTimeLimits::Clock::now() - started).count()),
            std::memory_order_relaxed);
    }
}



struct PipelineResult
{
    Move bestMove{};
    bool hasMove = false;
    float valueForRootSide = 0.0f;
};

template<
    typename TrState,
    typename PawnTables
>
PipelineResult RunPipeline(
    TrState& St,
    std::uint32_t nodeIndex,
    const PawnTables& pawnTable,
    bool whiteToMove,
    PipelineContext& ctx,
    FFTWorking& workingFFT,
    const SearchTimeLimits& time_limits
)
{
    // A previous search has fully joined its PUCT worker before returning.
    // Reuse the allocated arrays while invalidating all per-search state.
    St.tree.size = 1;
    std::fill_n(St.tree.childBegin, St.tree.capacity, 0);
    std::fill_n(St.tree.childCount, St.tree.capacity, 0);
    std::fill_n(St.tree.parentIndex, St.tree.capacity, 0);
    for (std::size_t i = 0; i < St.tree.capacity; ++i)
        St.tree.At(i).fftDelta = {};

    std::fill_n(ctx.puct_states, PipelineContext::TREE_SIZE, PuctState{});
    std::fill_n(ctx.puct_ready, PipelineContext::TREE_SIZE, false);
    std::fill_n(ctx.gatv2_embeddings,
        PipelineContext::TREE_SIZE * DefaultGATv2Config::EMBED_DIM, 0.0f);
    std::fill_n(ctx.fft_history_nodes, PipelineContext::HISTORY_K, UINT32_MAX);
    std::fill_n(ctx.fft_interleaved,
        static_cast<std::size_t>(PipelineContext::HISTORY_K) * FFT_FLOATS, 0.0f);
    ctx.gatv2_calls.store(0, std::memory_order_relaxed);
    ctx.gatv2_ok.store(0, std::memory_order_relaxed);
    ctx.max_depth_seen.store(0, std::memory_order_relaxed);
    ctx.total_ns.store(0, std::memory_order_relaxed);
    ctx.gatv2_last_report_ns.store(0, std::memory_order_relaxed);
    ctx.Start();

std::memcpy(
    &ctx.fft_working_history[0],
    &workingFFT,
    sizeof(FFTWorking)
);

InterleaveFFT(
    &workingFFT.real[0][0][0],
    &workingFFT.imag[0][0][0],
    ctx.fft_interleaved
);

    {
        std::lock_guard<std::mutex> lock(ctx.puct_mutex);
        ctx.puct_done = false;
    }
    EvaluateSingleNodeGAT(St, ctx, nodeIndex, 0, workingFFT, whiteToMove);

    std::atomic<std::uint64_t> puct_elapsed_ns{0};
    std::atomic<bool> puct_did_run{false};
    std::thread puctWorker([&] {
        RunDelayedPUCTWorker(St, ctx, time_limits.hard_deadline,
                             puct_elapsed_ns, puct_did_run);
    });

    Node<Board>& node =
        St.tree.At(nodeIndex);

    Board Plank =
        node.position;

    GameState InstanceOne = CURRENT_POSITION;
    InstanceOne.board = Plank;
    const Board root_board_snapshot = InstanceOne.board;
    const FFTWorking root_fft_snapshot = workingFFT;


    // ========================================================
    // DFS STACK
    // ========================================================

    constexpr std::size_t MaxDepth = 128;

    struct Cursor
    {
        std::uint32_t nodeIndex = 0;

        std::uint32_t firstChild = 0;

        std::uint16_t childCount = 0;

        std::uint16_t nextChild = 0;

        bool whiteToMove = false;

        bool expanded = false;

        bool hasUndo = false;

        StateUndo undo{};
    };


    std::array<
        Cursor,
        MaxDepth
    > stack{};


    std::size_t depth = 0;


    stack[0].nodeIndex =
        nodeIndex;

    stack[0].whiteToMove =
        whiteToMove;

    // ========================================================
    // DFS
    // ========================================================

    const auto dfs_started = SearchTimeLimits::Clock::now();
    while (ctx.IsRunning())
    {
        if (SearchTimeLimits::Clock::now() >= time_limits.soft_deadline &&
            (depth != 0 || stack[depth].expanded))
            break;
        Cursor& current =
            stack[depth];


        // ====================================================
        // EXPAND
        // ====================================================

        // ====================================================
// EXPAND
// ====================================================

if (!current.expanded)
{
    current.expanded = true;


    // =================================================
    // WHITE
    // =================================================

    if (current.whiteToMove)
    {
        InstanceOne
            .WhiteMoves
            .WhiteMoves
            .fill({});

        InstanceOne
            .WhiteMoves
            .Count = 0;

        InstanceOne.WhiteCount = 0;


        AllWhiteLegalMoves(
            InstanceOne.WhiteMoves,
            InstanceOne.WhiteCount,
            InstanceOne,
            pawnTable
        );


        if (InstanceOne.WhiteCount == 0)
        {
            current.childCount = 0;
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(ctx.puct_mutex);
                current.firstChild = St.tree.CreateChildren(
                    current.nodeIndex,
                    static_cast<
                        std::uint16_t
                    >(
                        InstanceOne.WhiteCount
                    )
                );
            }


            if (
                current.firstChild ==
                UINT32_MAX
            )
            {
                current.childCount = 0;
            }
            else {
                current.childCount = static_cast<std::uint16_t>(InstanceOne.WhiteCount);

                for (std::uint16_t i = 0; i < current.childCount; ++i) {
                    const std::uint32_t childIndex = current.firstChild + i;
                    const Move move = InstanceOne.WhiteMoves.WhiteMoves[i];
                    StoreChildWithFftDelta<true>(
                        St.tree, current.nodeIndex, childIndex, InstanceOne, move);
                }
            }

        }
    }


    // =================================================
    // BLACK
    // =================================================

    else
    {
        InstanceOne
            .BlackMoves
            .BlackMoves
            .fill({});

        InstanceOne
            .BlackMoves
            .Count = 0;

        InstanceOne.BlackCount = 0;


        AllBlackLegalMoves(
            InstanceOne.BlackMoves,
            InstanceOne.BlackCount,
            InstanceOne,
            pawnTable
        );


        if (InstanceOne.BlackCount == 0)
        {
            current.childCount = 0;
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(ctx.puct_mutex);
                current.firstChild = St.tree.CreateChildren(
                    current.nodeIndex,
                    static_cast<
                        std::uint16_t
                    >(
                        InstanceOne.BlackCount
                    )
                );
            }


            if (
                current.firstChild ==
                UINT32_MAX
            )
            {
                current.childCount = 0;
            }
            else {
                current.childCount = static_cast<std::uint16_t>(InstanceOne.BlackCount);

                for (std::uint16_t i = 0; i < current.childCount; ++i) {
                    const std::uint32_t childIndex = current.firstChild + i;
                    const Move move = InstanceOne.BlackMoves.BlackMoves[i];
                    StoreChildWithFftDelta<false>(
                        St.tree, current.nodeIndex, childIndex, InstanceOne, move);
                }
            }
        }

    }

    if (depth + 1 < MaxDepth)
        EvaluateSiblingBatchGAT(
            St,
            ctx,
            current.nodeIndex,
            static_cast<int>(depth),
            workingFFT,
            current.whiteToMove);
}

        // ====================================================
        // NEXT CHILD
        // ====================================================

        if (
            current.nextChild <
            current.childCount
        )
        {
            const std::uint32_t childIndex =
                current.firstChild +
                current.nextChild;


            ++current.nextChild;


            const Move move =
                St.tree.moves[
                    childIndex
                ];


            StateUndo undo;


            // =================================================
            // MAKE
            // =================================================

            if (current.whiteToMove)
            {
                undo =
                    MakeMove<true>(
                        InstanceOne,
                        move
                    );
            }
            else
            {
                undo =
                    MakeMove<false>(
                        InstanceOne,
                        move
                    );
            }


            // =================================================
            // FFT:
            //
            // parent FFT
            //       +
            // child delta
            //
            // =================================================

            ApplyFftDelta<4>(
            workingFFT,
            St.tree
            .At(childIndex)
            .fftDelta
);


            // =================================================
            // DEPTH
            // =================================================

            ++depth;


                if (depth >= MaxDepth)
            {
                // Cofnij FFT.
                RemoveFftDelta(
                    workingFFT,
                    St.tree.At(childIndex).fftDelta
                );

                // Cofnij ruch na planszy.
                if (current.whiteToMove)
                {
                    UnmakeMove<true>(InstanceOne, move, undo);
                }
                else
                {
                    UnmakeMove<false>(InstanceOne, move, undo);
                }

                --depth;
                continue;
            }

            // =================================================
            // OCENA GATv2 (dla tego konkretnego dziecka)
            // =================================================
            if (ctx.puct_ready[childIndex])
                UpdateFFTHistoryOnly(
                    St, ctx, childIndex, depth, workingFFT);
            else
                EvaluateSingleNodeGAT(St, ctx, childIndex, depth, workingFFT,
                                      !current.whiteToMove);

            // =================================================
            // INIT CHILD CURSOR (dla DFS)
            // =================================================
            stack[depth].nodeIndex = childIndex;
            stack[depth].firstChild = 0;
            stack[depth].childCount = 0;
            stack[depth].nextChild = 0;
            stack[depth].whiteToMove = !current.whiteToMove;
            stack[depth].expanded = false;
            stack[depth].hasUndo = true;
            stack[depth].undo = undo;

            continue; // <-- To continue zamyka blok if (current.nextChild < current.childCount)
        }

    
        // ====================================================
        // ROOT
        // ====================================================

        if (depth == 0)
            break;


        // ====================================================
        // RETURN TO PARENT
        // ====================================================

        Cursor& child =
            stack[depth];


        const std::uint32_t childIndex =
            child.nodeIndex;


        const Move move =
            St.tree.moves[
                childIndex
            ];


        // ====================================================
        // FFT UNDO
        // ====================================================

        RemoveFftDelta(
            workingFFT,
            St.tree
                .At(childIndex)
                .fftDelta
        );


        // ====================================================
        // BOARD UNDO
        // ====================================================

        // child.whiteToMove = strona,
        // która ma ruch JUŻ W DZIECKU.
        //
        // Czyli ruch wykonany przed wejściem
        // do dziecka wykonał przeciwny kolor.

        if (child.whiteToMove)
        {
            UnmakeMove<false>(
                InstanceOne,
                move,
                child.undo
            );
        }
        else
        {
            UnmakeMove<true>(
                InstanceOne,
                move,
                child.undo
            );
        }


        --depth;
    }

    // A soft timeout can occur below the root. Restore every active move and
    // FFT delta before the pipeline returns.
    while (depth > 0) {
        Cursor& child = stack[depth];
        const std::uint32_t childIndex = child.nodeIndex;
        const Move move = St.tree.moves[childIndex];
        RemoveFftDelta(workingFFT, St.tree.At(childIndex).fftDelta);
        if (child.whiteToMove)
            UnmakeMove<false>(InstanceOne, move, child.undo);
        else
            UnmakeMove<true>(InstanceOne, move, child.undo);
        --depth;
    }



    float fft_restore_max_abs = 0.0f;
    for (std::size_t channel = 0; channel < PieceChannelCount; ++channel)
        for (std::size_t row = 0; row < PieceBoardRows; ++row)
            for (std::size_t col = 0; col < PieceBoardCols; ++col) {
                fft_restore_max_abs = std::max(fft_restore_max_abs,
                    std::abs(workingFFT.real[channel][row][col] -
                             root_fft_snapshot.real[channel][row][col]));
                fft_restore_max_abs = std::max(fft_restore_max_abs,
                    std::abs(workingFFT.imag[channel][row][col] -
                             root_fft_snapshot.imag[channel][row][col]));
            }
            
    const bool board_restored = std::memcmp(
        &InstanceOne.board, &root_board_snapshot, sizeof(Board)) == 0;
    const auto dfs_finished = SearchTimeLimits::Clock::now();

    {
        std::lock_guard<std::mutex> lock(ctx.puct_mutex);
        ctx.puct_done = true;
    }
    ctx.puct_cv.notify_all();
    puctWorker.join();

    // Spend a small, bounded PUCT batch on the already-built tree when the
    // primary search finishes early. The clock reserve is already excluded
    // from hard_deadline by MakeSearchTimeLimits.
    constexpr int extra_puct_iterations = 40;
    constexpr auto puct_deadline_margin = std::chrono::milliseconds(1);
    if (puct_did_run.load(std::memory_order_relaxed) &&
        St.tree.childCount[0] != 0 &&
        SearchTimeLimits::Clock::now() + puct_deadline_margin <
            time_limits.hard_deadline) {
        const auto started = SearchTimeLimits::Clock::now();
        RunPUCT(St, ctx.puct_states, extra_puct_iterations, 1.0f, 1.0f, 0,
                ctx.puct_ready, time_limits.hard_deadline, false);
        puct_elapsed_ns.fetch_add(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                SearchTimeLimits::Clock::now() - started).count()),
            std::memory_order_relaxed);
    }

    std::size_t ready_count = 0;
    {
        std::lock_guard<std::mutex> lock(ctx.puct_mutex);
        for (std::size_t i = 0; i < St.tree.size; ++i)
            ready_count += ctx.puct_ready[i] ? 1u : 0u;
    }
    const std::uint32_t selected = puct_did_run.load(std::memory_order_relaxed)
        ? select_best_root_child(St.tree, ctx.puct_states, ctx.puct_ready)
        : UINT32_MAX;
    const std::uint32_t first_child = St.tree.childCount[0] != 0
        ? St.tree.childBegin[0] : UINT32_MAX;
    const std::uint32_t best_child = selected != UINT32_MAX
        ? selected : first_child;
    Move bestMove{};
    std::string best_uci = "(none)";
    if (best_child != UINT32_MAX && best_child < St.tree.size) {
        bestMove = St.tree.moves[best_child];
        const auto square = [](std::uint8_t sq) {
            std::string out;
            out += static_cast<char>('a' + sq % 8);
            out += static_cast<char>('8' - sq / 8);
            return out;
        };
        best_uci = square(bestMove.From) + square(bestMove.To);
        if (IsPromotion(bestMove.Flags))
            best_uci += "qrbn"[(bestMove.Flags - PROMO_QUEEN_QUIET) & 3u];
    }
    const auto dfs_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        dfs_finished - dfs_started).count();
    std::cerr << "[SEARCH] budget_ms=" << time_limits.budget.count()
              << " soft_ms=" << time_limits.soft.count()
              << " hard_ms=" << time_limits.budget.count()
              << " dfs_ms=" << dfs_ms
              << " puct_us=" << puct_elapsed_ns.load(std::memory_order_relaxed) / 1000
              << " nodes=" << St.tree.size
              << " puct_ready=" << ready_count
              << " bestMove=" << best_uci << '\n';
    std::cerr << "[SEARCH restore] board=" << (board_restored ? "root" : "MISMATCH")
              << " fft_max_abs=" << fft_restore_max_abs << '\n';
    std::cout << "bestmove " << (best_child == UINT32_MAX ? "0000" : best_uci)
              << '\n' << std::flush;

    const float valueForRootSide = best_child != UINT32_MAX
        ? -ctx.puct_states[best_child].mu_N : 0.0f;
    return {bestMove, best_child != UINT32_MAX, valueForRootSide};
}
