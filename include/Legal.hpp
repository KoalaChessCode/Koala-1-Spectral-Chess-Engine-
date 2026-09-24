#pragma once

#include "Constants.hpp"
#include "Attacks.hpp"

inline BB WhitePawnAttackMask(const Board& board)
{
    const BB pawns = WhitePawns(board);
    return ((pawns >> 9) & notFileH)
        | ((pawns >> 7) & notFileA);
}

inline BB BlackPawnAttackMask(const Board& board)
{
    const BB pawns = BlackPawns(board);
    return ((pawns << 7) & notFileH)
        | ((pawns << 9) & notFileA);
}

inline BB WhiteKnightAttackMask(const Board& board)
{
    return KnightAttacks<&Board::WKnightLeft, KNIGHT_ATTACKS>(board)
        | KnightAttacks<&Board::WKnightRight, KNIGHT_ATTACKS>(board);
}

inline BB BlackKnightAttackMask(const Board& board)
{
    return KnightAttacks<&Board::BKnightLeft, KNIGHT_ATTACKS>(board)
        | KnightAttacks<&Board::BKnightRight, KNIGHT_ATTACKS>(board);
}

inline BB WhiteBishopAttackMask(const Board& board, BB occupancy)
{
    return BishopAttacks<
        &Board::WBishopLeft,
        BISHOP_DIAGONALS,
        BISHOP_ANTIDIAGONALS
    >(board, occupancy)
        | BishopAttacks<
            &Board::WBishopRight,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS
        >(board, occupancy);
}

inline BB BlackBishopAttackMask(const Board& board, BB occupancy)
{
    return BishopAttacks<
        &Board::BBishopLeft,
        BISHOP_DIAGONALS,
        BISHOP_ANTIDIAGONALS
    >(board, occupancy)
        | BishopAttacks<
            &Board::BBishopRight,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS
        >(board, occupancy);
}

inline BB WhiteRookAttackMask(const Board& board, BB occupancy)
{
    return RookAttacks<
        &Board::WRookLeft,
        ROOK_FILES,
        ROOK_RANKS
    >(board, occupancy)
        | RookAttacks<
            &Board::WRookRight,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);
}

inline BB BlackRookAttackMask(const Board& board, BB occupancy)
{
    return RookAttacks<
        &Board::BRookLeft,
        ROOK_FILES,
        ROOK_RANKS
    >(board, occupancy)
        | RookAttacks<
            &Board::BRookRight,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);
}

inline BB WhiteQueenAttackMask(const Board& board, BB occupancy)
{
    return QueenAttacks<
        &Board::WQueen,
        BISHOP_DIAGONALS,
        BISHOP_ANTIDIAGONALS,
        ROOK_FILES,
        ROOK_RANKS
    >(board, occupancy);
}

inline BB BlackQueenAttackMask(const Board& board, BB occupancy)
{
    return QueenAttacks<
        &Board::BQueen,
        BISHOP_DIAGONALS,
        BISHOP_ANTIDIAGONALS,
        ROOK_FILES,
        ROOK_RANKS
    >(board, occupancy);
}

inline BB WhiteKingAttackMask(const Board& board)
{
    return KingAttacks<&Board::WKing, KING_ATTACKS>(board);
}

inline BB BlackKingAttackMask(const Board& board)
{
    return KingAttacks<&Board::BKing, KING_ATTACKS>(board);
}

inline BB WhiteAttackMask(const Board& board)
{
    const BB occupancy = WhiteOccupancy(board) | BlackOccupancy(board);
    return WhitePawnAttackMask(board)
        | WhiteKnightAttackMask(board)
        | WhiteBishopAttackMask(board, occupancy)
        | WhiteRookAttackMask(board, occupancy)
        | WhiteQueenAttackMask(board, occupancy)
        | WhiteKingAttackMask(board);
}

inline BB BlackAttackMask(const Board& board)
{
    const BB occupancy = WhiteOccupancy(board) | BlackOccupancy(board);
    return BlackPawnAttackMask(board)
        | BlackKnightAttackMask(board)
        | BlackBishopAttackMask(board, occupancy)
        | BlackRookAttackMask(board, occupancy)
        | BlackQueenAttackMask(board, occupancy)
        | BlackKingAttackMask(board);
}

inline U8 WhiteKingSquare(const Board& board)
{
    return static_cast<U8>(__builtin_ctzll(board.WKing));
}

inline U8 BlackKingSquare(const Board& board)
{
    return static_cast<U8>(__builtin_ctzll(board.BKing));
}

template<bool White>
inline bool IsKingInCheck(const Board& board)
{
    const BB kingMask = White
        ? board.WKing
        : board.BKing;

    const BB enemyAttacks = White
        ? BlackAttackMask(board)
        : WhiteAttackMask(board);

    return (enemyAttacks & kingMask) != 0;
}

using PieceSlot = BB Board::*;

inline constexpr std::array<PieceSlot, 32> PIECE_SLOTS{
    &Board::WRookLeft, &Board::WRookRight, &Board::WBishopLeft, &Board::WBishopRight,
    &Board::WQueen, &Board::WKing, &Board::WKnightLeft, &Board::WKnightRight,
    &Board::BRookLeft, &Board::BRookRight, &Board::BBishopLeft, &Board::BBishopRight,
    &Board::BQueen, &Board::BKing, &Board::BKnightLeft, &Board::BKnightRight,
    &Board::BPawn_a2, &Board::BPawn_b2, &Board::BPawn_c2, &Board::BPawn_d2,
    &Board::BPawn_e2, &Board::BPawn_f2, &Board::BPawn_g2, &Board::BPawn_h2,
    &Board::WPawn_a7, &Board::WPawn_b7, &Board::WPawn_c7, &Board::WPawn_d7,
    &Board::WPawn_e7, &Board::WPawn_f7, &Board::WPawn_g7, &Board::WPawn_h7
};

inline PieceSlot FindPieceAt(const Board& board, BB square)
{
    for (PieceSlot slot : PIECE_SLOTS)
        if (board.*slot & square)
            return slot;
    return nullptr;
}

inline bool IsPromotion(U8 flag)
{
    return flag >= PROMO_QUEEN_QUIET && flag <= PROMO_KNIGHT_CAPTURE;
}

template<bool White>
inline PieceSlot PromotionSlot(U8 flag)
{
    constexpr std::array<PieceSlot, 4> white{
        &Board::WQueen, &Board::WRookLeft, &Board::WBishopLeft, &Board::WKnightLeft};
    constexpr std::array<PieceSlot, 4> black{
        &Board::BQueen, &Board::BRookLeft, &Board::BBishopLeft, &Board::BKnightLeft};
    const unsigned type = (flag - PROMO_QUEEN_QUIET) & 3u;
    return White ? white[type] : black[type];
}

struct LegalUndo
{
    PieceSlot moved;
    PieceSlot captured;
    BB capturedSquare;
    U8 flags;
};

struct StateUndo
{
    LegalUndo board;
    U8 castlingRights;
    BB enPassantTarget;
};

template<bool White>
inline LegalUndo MakeLegalMove(Board& board, const Move& move)
{
    const BB from = BB(1) << move.From;
    const BB to = BB(1) << move.To;
    LegalUndo undo{nullptr, nullptr, 0, move.Flags};

    if (move.Flags >= CASTLE_WHITE_KINGSIDE && move.Flags <= CASTLE_BLACK_QUEENSIDE)
    {
        ApplyCastle(board, move.Flags);
        return undo;
    }

    undo.moved = FindPieceAt(board, from);
    undo.capturedSquare = move.Flags == EN_PASSANT
        ? (White ? to << 8 : to >> 8)
        : to;
    undo.captured = FindPieceAt(board, undo.capturedSquare);

    if (undo.captured)
        board.*undo.captured &= ~undo.capturedSquare;

    board.*undo.moved &= ~from;
    if (IsPromotion(move.Flags))
        board.*PromotionSlot<White>(move.Flags) |= to;
    else
        board.*undo.moved |= to;

    return undo;
}

inline void UnmakeCastle(Board& board, U8 flag)
{
    switch (flag)
    {
        case CASTLE_WHITE_KINGSIDE:
            board.WKing = (board.WKing & ~(BB(1) << 62)) | (BB(1) << 60);
            board.WRookRight = (board.WRookRight & ~(BB(1) << 61)) | (BB(1) << 63);
            break;
        case CASTLE_WHITE_QUEENSIDE:
            board.WKing = (board.WKing & ~(BB(1) << 58)) | (BB(1) << 60);
            board.WRookLeft = (board.WRookLeft & ~(BB(1) << 59)) | (BB(1) << 56);
            break;
        case CASTLE_BLACK_KINGSIDE:
            board.BKing = (board.BKing & ~(BB(1) << 6)) | (BB(1) << 4);
            board.BRookRight = (board.BRookRight & ~(BB(1) << 5)) | (BB(1) << 7);
            break;
        case CASTLE_BLACK_QUEENSIDE:
            board.BKing = (board.BKing & ~(BB(1) << 2)) | (BB(1) << 4);
            board.BRookLeft = (board.BRookLeft & ~(BB(1) << 3)) | (BB(1) << 0);
            break;
        default:
            break;
    }
}

template<bool White>
inline void UnmakeLegalMove(Board& board, const Move& move, const LegalUndo& undo)
{
    if (move.Flags >= CASTLE_WHITE_KINGSIDE && move.Flags <= CASTLE_BLACK_QUEENSIDE)
    {
        UnmakeCastle(board, move.Flags);
        return;
    }

    const BB from = BB(1) << move.From;
    const BB to = BB(1) << move.To;
    if (IsPromotion(move.Flags))
        board.*PromotionSlot<White>(move.Flags) &= ~to;
    else
        board.*undo.moved &= ~to;
    board.*undo.moved |= from;

    if (undo.captured)
        board.*undo.captured |= undo.capturedSquare;
}

inline void RefreshOccupancy(GameState& state)
{
    state.WhiteOccup = WhiteOccupancy(state.board);
    state.BlackOccup = BlackOccupancy(state.board);
    state.Occupancy = state.WhiteOccup | state.BlackOccup;
}

inline void RemoveCastlingRightAt(GameState& state, BB square)
{
    if (square & (BB(1) << 60))
        state.CastlingRights &= ~(WHITE_KINGSIDE_RIGHT | WHITE_QUEENSIDE_RIGHT);
    else if (square & (BB(1) << 63))
        state.CastlingRights &= ~WHITE_KINGSIDE_RIGHT;
    else if (square & (BB(1) << 56))
        state.CastlingRights &= ~WHITE_QUEENSIDE_RIGHT;
    else if (square & (BB(1) << 4))
        state.CastlingRights &= ~(BLACK_KINGSIDE_RIGHT | BLACK_QUEENSIDE_RIGHT);
    else if (square & (BB(1) << 7))
        state.CastlingRights &= ~BLACK_KINGSIDE_RIGHT;
    else if (square & (BB(1) << 0))
        state.CastlingRights &= ~BLACK_QUEENSIDE_RIGHT;
}

template<bool White>
inline StateUndo MakeMove(GameState& state, const Move& move)
{
    StateUndo undo{
        MakeLegalMove<White>(state.board, move),
        state.CastlingRights,
        state.EnPassantTarget
    };

    const BB from = BB(1) << move.From;
    RemoveCastlingRightAt(state, from);
    RemoveCastlingRightAt(state, undo.board.capturedSquare);

    state.EnPassantTarget = move.Flags == DOUBLE_PUSH
        ? (White ? from >> 8 : from << 8)
        : 0;
    RefreshOccupancy(state);
    return undo;
}

template<bool White>
inline void UnmakeMove(GameState& state, const Move& move, const StateUndo& undo)
{
    UnmakeLegalMove<White>(state.board, move, undo.board);
    state.CastlingRights = undo.castlingRights;
    state.EnPassantTarget = undo.enPassantTarget;
    RefreshOccupancy(state);
}

template<bool White>
inline bool IsCastleLegal(const GameState& state, const Move& move)
{
    const auto& castles = White ? CASTLE_TABLES.White : CASTLE_TABLES.Black;

    for (const CastleData& castle : castles)
    {
        if (castle.moveFlag != move.Flags)
        {
            continue;
        }

        if ((state.CastlingRights & castle.rightMask) == 0)
        {
            return false;
        }

        const BB kingPiece = White ? state.board.WKing : state.board.BKing;
        const BB rookPiece = (move.Flags == CASTLE_WHITE_KINGSIDE || move.Flags == CASTLE_BLACK_KINGSIDE)
            ? (White ? state.board.WRookRight : state.board.BRookRight)
            : (White ? state.board.WRookLeft : state.board.BRookLeft);

        if ((kingPiece & castle.kingFrom) == 0 || (rookPiece & castle.rookFrom) == 0)
        {
            return false;
        }

        if ((state.Occupancy & castle.emptyMask) != 0)
        {
            return false;
        }

        const BB enemyAttacks = White ? BlackAttackMask(state.board) : WhiteAttackMask(state.board);
        if ((enemyAttacks & castle.kingSafetyMask) != 0)
        {
            return false;
        }

        return true;
    }

    return false;
}




template<bool White>
inline bool IsLegalMove(GameState& state, const Move& move)
{
    if (move.Flags == CASTLE_WHITE_KINGSIDE
        || move.Flags == CASTLE_WHITE_QUEENSIDE
        || move.Flags == CASTLE_BLACK_KINGSIDE
        || move.Flags == CASTLE_BLACK_QUEENSIDE)
    {
        return IsCastleLegal<White>(state, move);
    }

    const LegalUndo undo = MakeLegalMove<White>(state.board, move);
    const bool legal = !IsKingInCheck<White>(state.board);
    UnmakeLegalMove<White>(state.board, move, undo);
    return legal;
}

template<bool White>
inline int FilterLegalMoves(
    std::array<Move,256>& roll,
    int count_move,
    GameState& state)
{
    int out = 0;

    for (int i = 0; i < count_move; ++i)
    {
        if (IsLegalMove<White>(state, roll[i]))
        {
            roll[out++] = roll[i];
        }
    }

    return out;
}
