#include "Constants.hpp"
#include "MoveGen.hpp"
#include "Legal.hpp"
#include "Puct.hpp"
#include "Helpers.hpp"



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

template<typename TrState, typename PawnTables, std::size_t N>
void RunPipeline(
    TrState& St,
    uint32_t nodeIndex,
    const PawnTables& pawnTable,
    bool whiteToMove,
    PipelineContext& ctx,
    unsigned char (&blob)[N]
)
{
    auto& ptr = InitSpectrum(blob);

 
    Node<Board>& node = St.tree.At(nodeIndex);
    Board Plank = node.position;
    GameState InstanceOne(Plank);


    constexpr std::size_t MaxDepth = 128;

    struct Cursor
    {
        std::uint32_t nodeIndex;
        std::uint32_t firstChild;
        std::uint16_t childCount;
        std::uint16_t nextChild;

        bool whiteToMove;
        bool expanded;
        bool hasUndo;

        StateUndo undo;
    };

    std::array<Cursor, MaxDepth> stack{};

    std::size_t depth = 0;

    stack[0].nodeIndex = nodeIndex;
    stack[0].firstChild = 0;
    stack[0].childCount = 0;
    stack[0].nextChild = 0;
    stack[0].whiteToMove = whiteToMove;
    stack[0].expanded = false;
    stack[0].hasUndo = false;


    while (ctx.IsRunning())
    {
        
        Cursor& current = stack[depth];

        if (!current.expanded)
        {
            current.expanded = true;

            if (current.whiteToMove)
            {
                InstanceOne.WhiteMoves.WhiteMoves.fill({});
                InstanceOne.WhiteMoves.Count = 0;
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
                            static_cast<std::uint16_t>(
                                InstanceOne.WhiteCount
                            )
                        );

                    if (current.firstChild == UINT32_MAX)
                        return;

                    current.childCount =
                        static_cast<std::uint16_t>(
                            InstanceOne.WhiteCount
                        );

                    for (std::uint16_t i = 0;
                         i < current.childCount;
                         ++i)
                    {
                        StateUndo undo =
                            MakeMove<true>(
                                InstanceOne,
                                InstanceOne.WhiteMoves.WhiteMoves[i]
                            );

                        St.tree.At(
                            current.firstChild + i
                        ).position = InstanceOne.board;

                        St.tree.moves[
                            current.firstChild + i
                        ] = InstanceOne.WhiteMoves.WhiteMoves[i];

                        UnmakeMove<true>(
                            InstanceOne,
                            InstanceOne.WhiteMoves.WhiteMoves[i],
                            undo
                        );
                    }
                }
            }
            else
            {
                InstanceOne.BlackMoves.BlackMoves.fill({});
                InstanceOne.BlackMoves.Count = 0;
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
                            static_cast<std::uint16_t>(
                                InstanceOne.BlackCount
                            )
                        );

                    if (current.firstChild == UINT32_MAX)
                        return;

                    current.childCount =
                        static_cast<std::uint16_t>(
                            InstanceOne.BlackCount
                        );

                    for (std::uint16_t i = 0;
                         i < current.childCount;
                         ++i)
                    {
                        StateUndo undo =
                            MakeMove<false>(
                                InstanceOne,
                                InstanceOne.BlackMoves.BlackMoves[i]
                            );

                        St.tree.At(
                            current.firstChild + i
                        ).position = InstanceOne.board;

                        St.tree.moves[
                            current.firstChild + i
                        ] = InstanceOne.BlackMoves.BlackMoves[i];

                        UnmakeMove<false>(
                            InstanceOne,
                            InstanceOne.BlackMoves.BlackMoves[i],
                            undo
                        );
                    }
                }
            }
        }



        if (current.nextChild < current.childCount)
        {
            const std::uint32_t childIndex =
                current.firstChild + current.nextChild;

            ++current.nextChild;

            const Move move =
                St.tree.moves[childIndex];

            StateUndo undo;

            if (current.whiteToMove)
            {
                undo = MakeMove<true>(
                    InstanceOne,
                    move
                );
            }
            else
            {
                undo = MakeMove<false>(
                    InstanceOne,
                    move
                );
            }

         
            ++depth;

            if (depth >= MaxDepth)
            {
             
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

            stack[depth].nodeIndex = childIndex;
            stack[depth].firstChild = 0;
            stack[depth].childCount = 0;
            stack[depth].nextChild = 0;
            stack[depth].whiteToMove = !current.whiteToMove;
            stack[depth].expanded = false;
            stack[depth].hasUndo = true;
            stack[depth].undo = undo;

            continue;
        }


       

        if (depth == 0)
            break;

        Cursor& child = stack[depth];

        const Move move =
            St.tree.moves[child.nodeIndex];

        // child.whiteToMove mówi, kto ma ruch W DZIECKU.
        // Zatem ruch prowadzący do dziecka wykonał przeciwny kolor.
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


