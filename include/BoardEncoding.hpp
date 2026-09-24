#pragma once

#include "Constants.hpp"
#include <iomanip>
#include <iostream>

using EncodedBoard = float[8][8];

constexpr float WhiteCode(float value)
{
    return value;
}

constexpr float BlackCode(float value)
{
    return -value;
}

constexpr std::size_t MatrixRowFromSquare(int square)
{
   
    return 7u - static_cast<std::size_t>(square / 8);
}

constexpr std::size_t MatrixColFromSquare(int square)
{
    return static_cast<std::size_t>(square % 8);
}

inline void PutPiece(EncodedBoard matrix, BB piece, float code)
{
    if (piece == 0)
    {
        return;
    }

    const int square = __builtin_ctzll(piece);
    matrix[MatrixRowFromSquare(square)][MatrixColFromSquare(square)] = code;
}

constexpr float EncodePawn(int file)
{

    return 1.0f + (static_cast<float>(file) + 1.0f) * 0.1f;
}

constexpr float EncodeKnightLeft()
{
    return 3.1f;
}

constexpr float EncodeKnightRight()
{
    return 3.2f;
}

constexpr float EncodeBishopLeft()
{
    return 3.3f;
}

constexpr float EncodeBishopRight()
{
    return 3.4f;
}

constexpr float EncodeRookLeft()
{
    return 5.1f;
}

constexpr float EncodeRookRight()
{
    return 5.2f;
}

constexpr float EncodeQueen()
{
    return 9.0f;
}

constexpr float EncodeKing()
{
    return 10.0f;
}

inline void EncodeBoardToMatrix(const Board& board, EncodedBoard matrix)
{
  
    PutPiece(matrix, board.BRookLeft, BlackCode(EncodeRookLeft()));
    PutPiece(matrix, board.BRookRight, BlackCode(EncodeRookRight()));
    PutPiece(matrix, board.BBishopLeft, BlackCode(EncodeBishopLeft()));
    PutPiece(matrix, board.BBishopRight, BlackCode(EncodeBishopRight()));
    PutPiece(matrix, board.BQueen, BlackCode(EncodeQueen()));
    PutPiece(matrix, board.BKing, BlackCode(EncodeKing()));
    PutPiece(matrix, board.BKnightLeft, BlackCode(EncodeKnightLeft()));
    PutPiece(matrix, board.BKnightRight, BlackCode(EncodeKnightRight()));

    PutPiece(matrix, board.BPawn_a2, BlackCode(EncodePawn(0)));
    PutPiece(matrix, board.BPawn_b2, BlackCode(EncodePawn(1)));
    PutPiece(matrix, board.BPawn_c2, BlackCode(EncodePawn(2)));
    PutPiece(matrix, board.BPawn_d2, BlackCode(EncodePawn(3)));
    PutPiece(matrix, board.BPawn_e2, BlackCode(EncodePawn(4)));
    PutPiece(matrix, board.BPawn_f2, BlackCode(EncodePawn(5)));
    PutPiece(matrix, board.BPawn_g2, BlackCode(EncodePawn(6)));
    PutPiece(matrix, board.BPawn_h2, BlackCode(EncodePawn(7)));


    PutPiece(matrix, board.WRookLeft, WhiteCode(EncodeRookLeft()));
    PutPiece(matrix, board.WRookRight, WhiteCode(EncodeRookRight()));
    PutPiece(matrix, board.WBishopLeft, WhiteCode(EncodeBishopLeft()));
    PutPiece(matrix, board.WBishopRight, WhiteCode(EncodeBishopRight()));
    PutPiece(matrix, board.WQueen, WhiteCode(EncodeQueen()));
    PutPiece(matrix, board.WKing, WhiteCode(EncodeKing()));
    PutPiece(matrix, board.WKnightLeft, WhiteCode(EncodeKnightLeft()));
    PutPiece(matrix, board.WKnightRight, WhiteCode(EncodeKnightRight()));

    PutPiece(matrix, board.WPawn_a7, WhiteCode(EncodePawn(0)));
    PutPiece(matrix, board.WPawn_b7, WhiteCode(EncodePawn(1)));
    PutPiece(matrix, board.WPawn_c7, WhiteCode(EncodePawn(2)));
    PutPiece(matrix, board.WPawn_d7, WhiteCode(EncodePawn(3)));
    PutPiece(matrix, board.WPawn_e7, WhiteCode(EncodePawn(4)));
    PutPiece(matrix, board.WPawn_f7, WhiteCode(EncodePawn(5)));
    PutPiece(matrix, board.WPawn_g7, WhiteCode(EncodePawn(6)));
    PutPiece(matrix, board.WPawn_h7, WhiteCode(EncodePawn(7)));

}
