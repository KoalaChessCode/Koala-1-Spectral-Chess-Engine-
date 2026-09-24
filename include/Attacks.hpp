#pragma once

#include "Constants.hpp"


template<
    BB Board::* Piece,
    const std::array<BB, 64>& BishopDiagonal,
    const std::array<BB, 64>& BishopAntiDiagonal>
inline BB BishopAttacks(const Board& board, BB occupancy)
{
    BB bishops = board.*Piece;
    BB attacks = 0;

    while (bishops)
    {
        const int square = __builtin_ctzll(bishops);
        const BB bishop = BB(1) << square;

        attacks |= HyperbolaAttacks(
            occupancy,
            bishop,
            BishopDiagonal[square]
        );
        attacks |= HyperbolaAttacks(
            occupancy,
            bishop,
            BishopAntiDiagonal[square]
        );

        bishops &= bishops - 1;
    }

    return attacks;
}

template<
    BB Board::* Piece,
    const std::array<BB, 64>& BishopDiagonal,
    const std::array<BB, 64>& BishopAntiDiagonal,
    const std::array<BB, 64>& RookFile,
    const std::array<BB, 64>& RookRank>
inline BB QueenAttacks(const Board& board, BB occupancy){
    BB queens = board.*Piece;
    BB attacks = 0;

    while (queens)
    {
        const int square = __builtin_ctzll(queens);
        const BB queen = BB(1) << square;

        attacks |= HyperbolaAttacks(
                occupancy,
                queen,
                BishopDiagonal[square]
            )
            |
            HyperbolaAttacks(
                occupancy,
                queen,
                BishopAntiDiagonal[square]
            )
            |
            HyperbolaAttacks(
                occupancy,
                queen,
                RookFile[square]
            )
            |
            HyperbolaAttacks(
                occupancy,
                queen,
                RookRank[square]
            );

        queens &= queens - 1;
    }

    return attacks;
}

template<
    BB Board::* Piece,
    const std::array<BB, 64>& RookFile,
    const std::array<BB, 64>& RookRank>
inline BB RookAttacks(const Board& board, BB occupancy){
    BB rooks = board.*Piece;
    BB attacks = 0;

    while (rooks)
    {
        const int square = __builtin_ctzll(rooks);
        const BB rook = BB(1) << square;

        attacks |= HyperbolaAttacks(
                occupancy,
                rook,
                RookFile[square]
            )
            |
            HyperbolaAttacks(
                occupancy,
                rook,
                RookRank[square]
            );

        rooks &= rooks - 1;
    }

    return attacks;
}

template<
    BB Board::* Piece,
    const std::array<BB, 64>& KnightAttacksTable>
inline BB KnightAttacks(const Board& board)
{
    BB knights = board.*Piece;
    BB attacks = 0;

    while (knights)
    {
        const int square = __builtin_ctzll(knights);
        attacks |= KnightAttacksTable[square];
        knights &= knights - 1;
    }

    return attacks;
}

template<
    BB Board::* Piece,
    const std::array<BB, 64>& KingAttacksTable>
inline BB KingAttacks(const Board& board)
{
    BB kings = board.*Piece;
    BB attacks = 0;

    while (kings)
    {
        const int square = __builtin_ctzll(kings);
        attacks |= KingAttacksTable[square];
        kings &= kings - 1;
    }

    return attacks;
}


