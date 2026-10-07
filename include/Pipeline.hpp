#include "Constants.hpp"
#include "MoveGen.hpp"
#include "Legal.hpp"
#include "Puct.hpp"

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

template<
    typename TrState,
    typename PawnTables
>
void RunPipeline(
    TrState& St,
    std::uint32_t nodeIndex,
    const PawnTables& pawnTable,
    bool whiteToMove,
    PipelineContext& ctx,
    FFTWorking& workingFFT
)
{
    Node<Board>& node =
        St.tree.At(nodeIndex);

    Board Plank =
        node.position;

    GameState InstanceOne(
        Plank
    );


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

    while (ctx.IsRunning())
    {
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
            current.firstChild =
                St.tree.CreateChildren(
                    current.nodeIndex,
                    static_cast<
                        std::uint16_t
                    >(
                        InstanceOne.WhiteCount
                    )
                );


            if (
                current.firstChild ==
                UINT32_MAX
            )
            {
                return;
            }


            current.childCount =
                static_cast<
                    std::uint16_t
                >(
                    InstanceOne.WhiteCount
                );


            // =========================================
            // CREATE CHILDREN
            // =========================================

            for (
                std::uint16_t i = 0;
                i < current.childCount;
                ++i
            )
            {
                const std::uint32_t childIndex =
                    current.firstChild + i;


                const Move move =
                    InstanceOne
                        .WhiteMoves
                        .WhiteMoves[i];


                StoreChildWithFftDelta<true>(
                    St.tree,
                    current.nodeIndex,
                    childIndex,
                    InstanceOne,
                    move
                );
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
            current.firstChild =
                St.tree.CreateChildren(
                    current.nodeIndex,
                    static_cast<
                        std::uint16_t
                    >(
                        InstanceOne.BlackCount
                    )
                );


            if (
                current.firstChild ==
                UINT32_MAX
            )
            {
                return;
            }


            current.childCount =
                static_cast<
                    std::uint16_t
                >(
                    InstanceOne.BlackCount
                );


            // =========================================
            // CREATE CHILDREN
            // =========================================

            for (
                std::uint16_t i = 0;
                i < current.childCount;
                ++i
            )
            {
                const std::uint32_t childIndex =
                    current.firstChild + i;


                const Move move =
                    InstanceOne
                        .BlackMoves
                        .BlackMoves[i];


                StoreChildWithFftDelta<false>(
                    St.tree,
                    current.nodeIndex,
                    childIndex,
                    InstanceOne,
                    move
                );
            }
        }
    }
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
                    St.tree
                        .At(childIndex)
                        .fftDelta
                );


                // Cofnij board.
                if (current.whiteToMove)
                {
                    UnmakeMove<true>(
                        InstanceOne,
                        move,
                        undo
                    );
                }
                else
                {
                    UnmakeMove<false>(
                        InstanceOne,
                        move,
                        undo
                    );
                }


                --depth;

                continue;
            }


            // =================================================
            // INIT CHILD CURSOR
            // =================================================

            stack[depth].nodeIndex =
                childIndex;

            stack[depth].firstChild =
                0;

            stack[depth].childCount =
                0;

            stack[depth].nextChild =
                0;

            stack[depth].whiteToMove =
                !current.whiteToMove;

            stack[depth].expanded =
                false;

            stack[depth].hasUndo =
                true;

            stack[depth].undo =
                undo;


            continue;
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
}