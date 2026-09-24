#include "Constants.hpp"
#include "MoveGen.hpp"
#include "Legal.hpp"

template<typename T, typename Y, typename U>
void AllWhiteLegalMoves(T& Wmoves,int Wcount,Y& Inst,U& PawnMem){
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
void AllBlackLegalMoves(A& Inst,B& PawnMem,C& Bmoves,int Bcount){
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


