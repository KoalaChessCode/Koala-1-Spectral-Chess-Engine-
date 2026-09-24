#pragma once
#include "Constants.hpp"
#include "Attacks.hpp"


inline void PushMoves(
    std::array<Move,256>& roll,
    int& count_move,
    uint8_t from_sq,
    BB targets,
    uint8_t flags)
{
    while (targets)
    {
        uint8_t to = static_cast<uint8_t>(__builtin_ctzll(targets));

        roll[count_move++] = {
            from_sq,
            to,
            flags
        };

        targets &= targets - 1;
    }
}

template<std::size_t N>
inline void PushRookMovesForPieces(
    std::array<Move, N>& roll, int& count_move, BB pieces,
    BB ownOcc, BB enemyOcc, BB occupancy)
{
    while (pieces)
    {
        const U8 from = static_cast<U8>(__builtin_ctzll(pieces));
        const BB piece = BB(1) << from;
        const BB attacks = HyperbolaAttacks(occupancy, piece, ROOK_FILES[from])
            | HyperbolaAttacks(occupancy, piece, ROOK_RANKS[from]);
        PushMoves(roll, count_move, from, QuietFilter(attacks, ownOcc, enemyOcc), QUIET);
        PushMoves(roll, count_move, from, CaptureFilter(attacks, enemyOcc), CAPTURE);
        pieces &= pieces - 1;
    }
}

template<std::size_t N>
inline void PushBishopMovesForPieces(
    std::array<Move, N>& roll, int& count_move, BB pieces,
    BB ownOcc, BB enemyOcc, BB occupancy)
{
    while (pieces)
    {
        const U8 from = static_cast<U8>(__builtin_ctzll(pieces));
        const BB piece = BB(1) << from;
        const BB attacks = HyperbolaAttacks(occupancy, piece, BISHOP_DIAGONALS[from])
            | HyperbolaAttacks(occupancy, piece, BISHOP_ANTIDIAGONALS[from]);
        PushMoves(roll, count_move, from, QuietFilter(attacks, ownOcc, enemyOcc), QUIET);
        PushMoves(roll, count_move, from, CaptureFilter(attacks, enemyOcc), CAPTURE);
        pieces &= pieces - 1;
    }
}

template<std::size_t N>
inline void PushKnightMovesForPieces(
    std::array<Move, N>& roll, int& count_move, BB pieces,
    BB ownOcc, BB enemyOcc)
{
    while (pieces)
    {
        const U8 from = static_cast<U8>(__builtin_ctzll(pieces));
        const BB attacks = KNIGHT_ATTACKS[from];
        PushMoves(roll, count_move, from, QuietFilter(attacks, ownOcc, enemyOcc), QUIET);
        PushMoves(roll, count_move, from, CaptureFilter(attacks, enemyOcc), CAPTURE);
        pieces &= pieces - 1;
    }
}

template<std::size_t N>
inline void PushQueenMovesForPieces(
    std::array<Move, N>& roll, int& count_move, BB pieces,
    BB ownOcc, BB enemyOcc, BB occupancy)
{
    while (pieces)
    {
        const U8 from = static_cast<U8>(__builtin_ctzll(pieces));
        const BB piece = BB(1) << from;
        const BB attacks = HyperbolaAttacks(occupancy, piece, BISHOP_DIAGONALS[from])
            | HyperbolaAttacks(occupancy, piece, BISHOP_ANTIDIAGONALS[from])
            | HyperbolaAttacks(occupancy, piece, ROOK_FILES[from])
            | HyperbolaAttacks(occupancy, piece, ROOK_RANKS[from]);
        PushMoves(roll, count_move, from, QuietFilter(attacks, ownOcc, enemyOcc), QUIET);
        PushMoves(roll, count_move, from, CaptureFilter(attacks, enemyOcc), CAPTURE);
        pieces &= pieces - 1;
    }
}

inline void PushPromotionMoves(
    std::array<Move,256>& roll,
    int& count_move,
    uint8_t from_sq,
    BB targets,
    bool capture)
{
    while (targets)
    {
        const uint8_t to = static_cast<uint8_t>(__builtin_ctzll(targets));

        roll[count_move++] = {
            from_sq,
            to,
            capture ? PROMO_QUEEN_CAPTURE : PROMO_QUEEN_QUIET
        };
        roll[count_move++] = {
            from_sq,
            to,
            capture ? PROMO_ROOK_CAPTURE : PROMO_ROOK_QUIET
        };
        roll[count_move++] = {
            from_sq,
            to,
            capture ? PROMO_BISHOP_CAPTURE : PROMO_BISHOP_QUIET
        };
        roll[count_move++] = {
            from_sq,
            to,
            capture ? PROMO_KNIGHT_CAPTURE : PROMO_KNIGHT_QUIET
        };

        targets &= targets - 1;
    }
}

inline void PushCastleMove(
    std::array<Move,256>& roll,
    int& count_move,
    uint8_t from_sq,
    const CastleData& castle)
{
    roll[count_move++] = {
        from_sq,
        static_cast<uint8_t>(__builtin_ctzll(castle.kingTo)),
        castle.moveFlag
    };
}


inline void PushPawnMove(
    std::array<Move,256>& roll,
    int& count_move,
    const PawnData& pawn,
    BB occupancy,
    BB enemyOcc,
    BB en_passant_target,
    uint8_t from_sq)
{
    // ======================
    // QUIET MOVE
    // ======================

    BB quiet = pawn.move_one & ~occupancy;
    BB promo_quiet = pawn.promotion_move_one & ~occupancy;

    if (promo_quiet)
    {
        PushPromotionMoves(
            roll,
            count_move,
            from_sq,
            promo_quiet,
            false
        );
    }
    else if (quiet)
    {
        roll[count_move++] = {
            from_sq,
            static_cast<uint8_t>(__builtin_ctzll(quiet)),
            QUIET
        };
    }

    // ======================
    // DOUBLE PUSH
    // ======================

    if (quiet)
    {
        BB dbl = pawn.move_two & ~occupancy;

        if (dbl)
        {
            roll[count_move++] = {
                from_sq,
                static_cast<uint8_t>(__builtin_ctzll(dbl)),
                DOUBLE_PUSH
            };
        }
    }

    // ======================
    // CAPTURE LEFT
    // ======================

    BB left = pawn.capture_left & enemyOcc & ~pawn.promotion_capture_left;
    BB promo_left = pawn.promotion_capture_left & enemyOcc;

    if (promo_left)
    {
        PushPromotionMoves(
            roll,
            count_move,
            from_sq,
            promo_left,
            true
        );
    }
    if (left)
    {
        roll[count_move++] = {
            from_sq,
            static_cast<uint8_t>(__builtin_ctzll(left)),
            CAPTURE
        };
    }

    // ======================
    // CAPTURE RIGHT
    // ======================

    BB right = pawn.capture_right & enemyOcc & ~pawn.promotion_capture_right;
    BB promo_right = pawn.promotion_capture_right & enemyOcc;

    if (promo_right)
    {
        PushPromotionMoves(
            roll,
            count_move,
            from_sq,
            promo_right,
            true
        );
    }
    if (right)
    {
        roll[count_move++] = {
            from_sq,
            static_cast<uint8_t>(__builtin_ctzll(right)),
            CAPTURE
        };
    }

    BB ep_left = pawn.en_passant_left & en_passant_target;
    if (ep_left)
    {
        roll[count_move++] = {
            from_sq,
            static_cast<uint8_t>(__builtin_ctzll(ep_left)),
            EN_PASSANT
        };
    }

    BB ep_right = pawn.en_passant_right & en_passant_target;
    if (ep_right)
    {
        roll[count_move++] = {
            from_sq,
            static_cast<uint8_t>(__builtin_ctzll(ep_right)),
            EN_PASSANT
        };
    }
}

inline void WhitePawnMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB occupancy,
    BB en_passant_target,
    const Board& board,
    const std::array<PawnData,48>& PawnTable)
{
    BB pawns = WhitePawns(board);

    while (pawns)
    {
        int square = __builtin_ctzll(pawns);

        const PawnData& pawn = PawnTable[square - 8];

        PushPawnMove(
            roll.WhiteMoves,
            count_move,
            pawn,
            occupancy,
            blackOcc,
            en_passant_target,
            static_cast<std::uint8_t>(square));

        pawns &= pawns - 1;
    }
}


inline void BlackPawnMoves(
    MoveListBlack& roll,
    int& count_move,
    BB whiteOcc,
    BB occupancy,
    BB en_passant_target,
    const Board& board,
    const std::array<PawnData,48>& PawnTable)
{
    BB pawns = BlackPawns(board);

    while (pawns)
    {
        int square = __builtin_ctzll(pawns);

        const PawnData& pawn = PawnTable[square - 8];

        PushPawnMove(
            roll.BlackMoves,
            count_move,
            pawn,
            occupancy,
            whiteOcc,
            en_passant_target,
            static_cast<std::uint8_t>(square));

        pawns &= pawns - 1;
    }
}

inline void WhiteRookLeftMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB rook = board.WRookLeft;

    if (rook == 0)
        return;

    if (rook & (rook - 1))
    {
        PushRookMovesForPieces(roll.WhiteMoves, count_move, rook, whiteOcc, blackOcc, occupancy);
        return;
    }

    int square = __builtin_ctzll(rook);

    BB attacks =
        RookAttacks<
            &Board::WRookLeft,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);

    BB quiet = QuietFilter(attacks, whiteOcc, blackOcc);
    BB caps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(square),
        quiet,
        QUIET);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(square),
        caps,
        CAPTURE);
}

inline void WhiteRookRightMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB rook = board.WRookRight;

    if (rook == 0)
        return;

    if (rook & (rook - 1))
    {
        PushRookMovesForPieces(roll.WhiteMoves, count_move, rook, whiteOcc, blackOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(rook);

   BB attacks =
        RookAttacks<
            &Board::WRookRight,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);

    BB quiet = QuietFilter(attacks, whiteOcc, blackOcc);
    BB caps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        quiet,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        caps,
        CAPTURE
    );
}


inline void BlackRookLeftMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB rook = board.BRookLeft;

    if (rook == 0)
        return;

    if (rook & (rook - 1))
    {
        PushRookMovesForPieces(roll.BlackMoves, count_move, rook, blackOcc, whiteOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(rook);

   BB attacks =
        RookAttacks<
            &Board::BRookLeft,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);

    BB quiet = QuietFilter(attacks, blackOcc, whiteOcc);
    BB caps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        quiet,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        caps,
        CAPTURE
    );
}

inline void BlackRookRightMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB rook = board.BRookRight;

    if (rook == 0)
        return;

    int index = __builtin_ctzll(rook);

   BB attacks =
        RookAttacks<
            &Board::BRookRight,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);

    BB quiet = QuietFilter(attacks, blackOcc, whiteOcc);
    BB caps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        quiet,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        caps,
        CAPTURE
    );
}


inline void WhiteBishopLeftMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB bishop = board.WBishopLeft;

    if (bishop == 0)
        return;

    if (bishop & (bishop - 1))
    {
        PushBishopMovesForPieces(roll.WhiteMoves, count_move, bishop, whiteOcc, blackOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(bishop);

    BB attacks =
        BishopAttacks<
            &Board::WBishopLeft,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS
        >(board, occupancy);

    BB WBLeftMoves = QuietFilter(attacks, whiteOcc, blackOcc);
    BB WBLeftCaps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WBLeftMoves,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WBLeftCaps,
        CAPTURE
    );
}


inline void WhiteBishopRightMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB bishop = board.WBishopRight;

    if (bishop == 0)
        return;

    if (bishop & (bishop - 1))
    {
        PushBishopMovesForPieces(roll.WhiteMoves, count_move, bishop, whiteOcc, blackOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(bishop);

    BB attacks =
        BishopAttacks<
            &Board::WBishopRight,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS
        >(board, occupancy);

    BB WBRightMoves = QuietFilter(attacks, whiteOcc, blackOcc);
    BB WBRightCaps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WBRightMoves,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WBRightCaps,
        CAPTURE
    );
}

inline void BlackBishopLeftMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB bishop = board.BBishopLeft;

    if (bishop == 0)
        return;

    if (bishop & (bishop - 1))
    {
        PushBishopMovesForPieces(roll.BlackMoves, count_move, bishop, blackOcc, whiteOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(bishop);

    BB attacks =
        BishopAttacks<
            &Board::BBishopLeft,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS
        >(board, occupancy);

    BB BBLeftMoves = QuietFilter(attacks, blackOcc, whiteOcc);
    BB BBLeftCaps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BBLeftMoves,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BBLeftCaps,
        CAPTURE
    );
}

inline void BlackBishopRightMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB bishop = board.BBishopRight;

    if (bishop == 0)
        return;

    int index = __builtin_ctzll(bishop);

    BB attacks =
        BishopAttacks<
            &Board::BBishopRight,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS
        >(board, occupancy);

    BB BBRightMoves = QuietFilter(attacks, blackOcc, whiteOcc);
    BB BBRightCaps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BBRightMoves,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BBRightCaps,
        CAPTURE
    );
}


inline void WhiteQueenMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB queen = board.WQueen;

    if (queen == 0)
        return;

    if (queen & (queen - 1))
    {
        PushQueenMovesForPieces(roll.WhiteMoves, count_move, queen, whiteOcc, blackOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(queen);

    BB attacks =
        QueenAttacks<
            &Board::WQueen,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);

    BB WQMoves = QuietFilter(attacks, whiteOcc, blackOcc);
    BB WQCaps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WQMoves,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WQCaps,
        CAPTURE
    );
}

inline void BlackQueenMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    BB occupancy,
    const Board& board)
{
    BB queen = board.BQueen;

    if (queen == 0)
        return;

    if (queen & (queen - 1))
    {
        PushQueenMovesForPieces(roll.BlackMoves, count_move, queen, blackOcc, whiteOcc, occupancy);
        return;
    }

    int index = __builtin_ctzll(queen);

    BB attacks =
        QueenAttacks<
            &Board::BQueen,
            BISHOP_DIAGONALS,
            BISHOP_ANTIDIAGONALS,
            ROOK_FILES,
            ROOK_RANKS
        >(board, occupancy);

    BB BQMoves = QuietFilter(attacks, blackOcc, whiteOcc);
    BB BQCaps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BQMoves,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BQCaps,
        CAPTURE
    );
}


inline void WhiteKnightLeftMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    const Board& board)
{
    BB knight = board.WKnightLeft;

    if (knight == 0)
        return;

    if (knight & (knight - 1))
    {
        PushKnightMovesForPieces(roll.WhiteMoves, count_move, knight, whiteOcc, blackOcc);
        return;
    }

    int index = __builtin_ctzll(knight);

    BB attacks =
        KnightAttacks<
            &Board::WKnightLeft,
            KNIGHT_ATTACKS
        >(board);

    BB WLKnightMoves = QuietFilter(attacks, whiteOcc, blackOcc);
    BB WLKnightCaps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WLKnightMoves,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WLKnightCaps,
        CAPTURE
    );
}


inline void WhiteKnightRightMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    const Board& board)
{
    BB knight = board.WKnightRight;

    if (knight == 0)
        return;

    if (knight & (knight - 1))
    {
        PushKnightMovesForPieces(roll.WhiteMoves, count_move, knight, whiteOcc, blackOcc);
        return;
    }

    int index = __builtin_ctzll(knight);

    BB attacks =
        KnightAttacks<
            &Board::WKnightRight,
            KNIGHT_ATTACKS
        >(board);

    BB WRKnightMoves = QuietFilter(attacks, whiteOcc, blackOcc);
    BB WRKnightCaps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WRKnightMoves,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WRKnightCaps,
        CAPTURE
    );
}


inline void BlackKnightLeftMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    const Board& board)
{
    BB knight = board.BKnightLeft;

    if (knight == 0)
        return;

    if (knight & (knight - 1))
    {
        PushKnightMovesForPieces(roll.BlackMoves, count_move, knight, blackOcc, whiteOcc);
        return;
    }

    int index = __builtin_ctzll(knight);

    BB attacks =
        KnightAttacks<
            &Board::BKnightLeft,
            KNIGHT_ATTACKS
        >(board);

    BB BLKnightMoves = QuietFilter(attacks, blackOcc, whiteOcc);
    BB BLKnightCaps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BLKnightMoves,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BLKnightCaps,
        CAPTURE
    );
}


inline void BlackKnightRightMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    const Board& board)
{
    BB knight = board.BKnightRight;

    if (knight == 0)
        return;

    int index = __builtin_ctzll(knight);

    BB attacks =
        KnightAttacks<
            &Board::BKnightRight,
            KNIGHT_ATTACKS
        >(board);

    BB BRKnightMoves = QuietFilter(attacks, blackOcc, whiteOcc);
    BB BRKnightCaps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BRKnightMoves,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BRKnightCaps,
        CAPTURE
    );
}


inline void WhiteKingMoves(
    MoveListWhite& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    const Board& board)
{
    BB king = board.WKing;

    if (king == 0)
        return;

    int index = __builtin_ctzll(king);

    BB attacks =
        KingAttacks<
            &Board::WKing,
            KING_ATTACKS
        >(board);

    BB WKingMoves = QuietFilter(attacks, whiteOcc, blackOcc);
    BB WKingCaps  = CaptureFilter(attacks, blackOcc);

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WKingMoves,
        QUIET
    );

    PushMoves(
        roll.WhiteMoves,
        count_move,
        static_cast<uint8_t>(index),
        WKingCaps,
        CAPTURE
    );
}


inline void BlackKingMoves(
    MoveListBlack& roll,
    int& count_move,
    BB blackOcc,
    BB whiteOcc,
    const Board& board)
{
    BB king = board.BKing;

    if (king == 0)
        return;

    int index = __builtin_ctzll(king);

    BB attacks =
        KingAttacks<
            &Board::BKing,
            KING_ATTACKS
        >(board);

    BB BKingMoves = QuietFilter(attacks, blackOcc, whiteOcc);
    BB BKingCaps  = CaptureFilter(attacks, whiteOcc);

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BKingMoves,
        QUIET
    );

    PushMoves(
        roll.BlackMoves,
        count_move,
        static_cast<uint8_t>(index),
        BKingCaps,
        CAPTURE
    );
}

inline void WhiteCastleMoves(
    MoveListWhite& roll,
    int& count_move,
    const GameState& state)
{
    const auto& table = CASTLE_TABLES.White;

    for (const CastleData& castle : table)
    {
        if ((state.CastlingRights & castle.rightMask) == 0)
        {
            continue;
        }

        if (state.board.WKing != castle.kingFrom)
        {
            continue;
        }

        BB rook = (castle.moveFlag == CASTLE_WHITE_KINGSIDE)
            ? state.board.WRookRight
            : state.board.WRookLeft;

        if (rook != castle.rookFrom)
        {
            continue;
        }

        if (state.Occupancy & castle.emptyMask)
        {
            continue;
        }

        PushCastleMove(
            roll.WhiteMoves,
            count_move,
            static_cast<uint8_t>(__builtin_ctzll(castle.kingFrom)),
            castle
        );
    }
}

inline void BlackCastleMoves(
    MoveListBlack& roll,
    int& count_move,
    const GameState& state)
{
    const auto& table = CASTLE_TABLES.Black;

    for (const CastleData& castle : table)
    {
        if ((state.CastlingRights & castle.rightMask) == 0)
        {
            continue;
        }

        if (state.board.BKing != castle.kingFrom)
        {
            continue;
        }

        BB rook = (castle.moveFlag == CASTLE_BLACK_KINGSIDE)
            ? state.board.BRookRight
            : state.board.BRookLeft;

        if (rook != castle.rookFrom)
        {
            continue;
        }

        if (state.Occupancy & castle.emptyMask)
        {
            continue;
        }

        PushCastleMove(
            roll.BlackMoves,
            count_move,
            static_cast<uint8_t>(__builtin_ctzll(castle.kingFrom)),
            castle
        );
    }
}

inline void WhitePiecesMovesGen(MoveListWhite& roll,int& count_move,GameState& state,const PawnTables& pawnTables){
    WhiteRookLeftMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    WhiteRookRightMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    WhiteBishopLeftMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    WhiteBishopRightMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    WhiteQueenMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    WhiteKnightLeftMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.board);
    WhiteKnightRightMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.board);
    WhiteKingMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.board);
    WhiteCastleMoves(roll,count_move,state);
    WhitePawnMoves(roll,count_move,state.BlackOccup,state.Occupancy,state.EnPassantTarget,state.board,pawnTables.White);

    // synchronizacja pola Count w strukturze MoveListWhite
    state.WhiteMoves.Count = static_cast<std::uint8_t>(count_move);
}

inline void BlackPiecesMovesGen(MoveListBlack& roll,int& count_move,GameState& state,const PawnTables& pawnTables){
    BlackRookLeftMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    BlackRookRightMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    BlackBishopLeftMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    BlackBishopRightMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    BlackQueenMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.Occupancy,state.board);
    BlackKnightLeftMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.board);
    BlackKnightRightMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.board);
    BlackKingMoves(roll,count_move,state.BlackOccup,state.WhiteOccup,state.board);
    BlackCastleMoves(roll,count_move,state);
    BlackPawnMoves(roll,count_move,state.WhiteOccup,state.Occupancy,state.EnPassantTarget,state.board,pawnTables.Black);  

    // synchronizacja pola Count w strukturze MoveListBlack
    state.BlackMoves.Count = static_cast<std::uint8_t>(count_move);
}
