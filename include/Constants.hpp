#pragma once
#include <cstdint>
#include <array>
#include <cstddef>  
#include <bit>

using std::uint64_t;
using std::uint8_t;

typedef uint64_t BB;
typedef uint8_t U8;

constexpr BB notFileA   = 0xFEFEFEFEFEFEFEFE;
constexpr BB notFileAB  = 0xFCFCFCFCFCFCFCFC;
constexpr BB notFileGH  = 0x3F3F3F3F3F3F3F3F;
constexpr BB notFileH   = 0x7F7F7F7F7F7F7F7F;



constexpr U8 QUIET       = 137;
constexpr U8 CAPTURE     = 53;
constexpr U8 DOUBLE_PUSH = 48;
constexpr U8 EN_PASSANT  = 177;

constexpr U8 WHITE_KINGSIDE_RIGHT  = 1u << 0;
constexpr U8 WHITE_QUEENSIDE_RIGHT = 1u << 1;
constexpr U8 BLACK_KINGSIDE_RIGHT  = 1u << 2;
constexpr U8 BLACK_QUEENSIDE_RIGHT = 1u << 3;

constexpr U8 PROMO_QUEEN_QUIET  = 201;
constexpr U8 PROMO_ROOK_QUIET   = 202;
constexpr U8 PROMO_BISHOP_QUIET = 203;
constexpr U8 PROMO_KNIGHT_QUIET = 204;

constexpr U8 PROMO_QUEEN_CAPTURE  = 205;
constexpr U8 PROMO_ROOK_CAPTURE   = 206;
constexpr U8 PROMO_BISHOP_CAPTURE = 207;
constexpr U8 PROMO_KNIGHT_CAPTURE = 208;

constexpr U8 CASTLE_WHITE_KINGSIDE  = 209;
constexpr U8 CASTLE_WHITE_QUEENSIDE = 210;
constexpr U8 CASTLE_BLACK_KINGSIDE  = 211;
constexpr U8 CASTLE_BLACK_QUEENSIDE = 212;


constexpr std::size_t CHILDREN = 64;

constexpr bool WHITE = true;
constexpr bool BLACK = true;

struct Board
{
    union
    {
        struct
        {
            //White Pieces
            BB WRookLeft;
            BB WRookRight;
            BB WBishopLeft;
            BB WBishopRight;
            BB WQueen;
            BB WKing;
            BB WKnightLeft;
            BB WKnightRight;
            
            //Black Pieces
            BB BRookLeft;
            BB BRookRight;
            BB BBishopLeft;
            BB BBishopRight;
            BB BQueen;
            BB BKing;
            BB BKnightLeft;
            BB BKnightRight;

            //Black Pawns
            BB BPawn_a2;
            BB BPawn_b2;
            BB BPawn_c2;
            BB BPawn_d2;
            BB BPawn_e2;
            BB BPawn_f2;
            BB BPawn_g2;
            BB BPawn_h2;

            //White Pawns
            BB WPawn_a7;
            BB WPawn_b7;
            BB WPawn_c7;
            BB WPawn_d7;
            BB WPawn_e7;
            BB WPawn_f7;
            BB WPawn_g7;
            BB WPawn_h7;
        };

        std::array<BB, 32> Pieces;
    };


    Board()
    {
        //Black Pieces
        BRookLeft    = BB(1) << 0;
        BKnightLeft  = BB(1) << 1;
        BBishopLeft  = BB(1) << 2;
        BQueen       = BB(1) << 3;
        BKing        = BB(1) << 4;
        BBishopRight = BB(1) << 5;
        BKnightRight = BB(1) << 6;
        BRookRight   = BB(1) << 7;

        //Black Pawns
        BPawn_a2 = BB(1) << 8;
        BPawn_b2 = BB(1) << 9;
        BPawn_c2 = BB(1) << 10;
        BPawn_d2 = BB(1) << 11;

        BPawn_e2 = BB(1) << 12;
        BPawn_f2 = BB(1) << 13;
        BPawn_g2 = BB(1) << 14;
        BPawn_h2 = BB(1) << 15;

        //White Pawns
        WPawn_a7 = BB(1) << 48;
        WPawn_b7 = BB(1) << 49;
        WPawn_c7 = BB(1) << 50;
        WPawn_d7 = BB(1) << 51;

        WPawn_e7 = BB(1) << 52;
        WPawn_f7 = BB(1) << 53;
        WPawn_g7 = BB(1) << 54;
        WPawn_h7 = BB(1) << 55;

        //White Pieces
        WRookLeft    = BB(1) << 56;
        WKnightLeft  = BB(1) << 57;
        WBishopLeft  = BB(1) << 58;
        WQueen       = BB(1) << 59;
        WKing        = BB(1) << 60;
        WBishopRight  = BB(1) << 61;
        WKnightRight = BB(1) << 62;
        WRookRight   = BB(1) << 63;
    }
};



struct PawnData
{
    BB move_one;
    BB move_two;

    BB capture_left;
    BB capture_right;

    BB promotion_move_one;
    BB promotion_capture_left;
    BB promotion_capture_right;

    BB en_passant_left;
    BB en_passant_right;


    constexpr PawnData(
        BB move_one = 0,
        BB move_two = 0,
        BB capture_left = 0,
        BB capture_right = 0,
        BB promotion_move_one = 0,
        BB promotion_capture_left = 0,
        BB promotion_capture_right = 0,
        BB en_passant_left = 0,
        BB en_passant_right = 0
    )
        :
        move_one(move_one),
        move_two(move_two),
        capture_left(capture_left),
        capture_right(capture_right),
        promotion_move_one(promotion_move_one),
        promotion_capture_left(promotion_capture_left),
        promotion_capture_right(promotion_capture_right),
        en_passant_left(en_passant_left),
        en_passant_right(en_passant_right)
    {}
};

inline BB WhitePawns(const Board& board)
{
    return board.WPawn_a7
        | board.WPawn_b7
        | board.WPawn_c7
        | board.WPawn_d7
        | board.WPawn_e7
        | board.WPawn_f7
        | board.WPawn_g7
        | board.WPawn_h7;
}

inline BB BlackPawns(const Board& board)
{
    return board.BPawn_a2
        | board.BPawn_b2
        | board.BPawn_c2
        | board.BPawn_d2
        | board.BPawn_e2
        | board.BPawn_f2
        | board.BPawn_g2
        | board.BPawn_h2;
}

inline BB WhiteOccupancy(const Board& board)
{
    return WhitePawns(board)
        |board.WRookLeft
        |board.WRookRight
        |board.WKnightLeft
        |board.WKnightRight
        |board.WBishopLeft
        |board.WBishopRight
        |board.WQueen
        |board.WKing;
}

inline BB BlackOccupancy(const Board& board){
    return BlackPawns(board)
        |board.BRookLeft
        |board.BRookRight
        |board.BKnightLeft
        |board.BKnightRight
        |board.BBishopLeft
        |board.BBishopRight
        |board.BQueen
        |board.BKing;
}
inline BB UpdateWhiteOccup(const Board& board)
{
    return WhiteOccupancy(board);
}

inline BB UpdateBlackOccup(const Board& board)
{
    return BlackOccupancy(board);
}


inline BB QuietFilter(BB attacks, BB own, BB enemy)
{
    return attacks & ~(own | enemy);
}

inline BB CaptureFilter(BB attacks, BB enemy)
{
    return attacks & enemy;
}

struct Move
{
    std::uint8_t From;
    std::uint8_t To;
    std::uint8_t Flags;
};

struct MoveListWhite
{
    std::array<Move, 256> WhiteMoves;
    std::uint8_t Count = 0;
};

struct MoveListBlack
{
    std::array<Move, 256> BlackMoves;
    std::uint8_t Count = 0;
};

struct GameState
{
    Board board;

    BB WhiteOccup;
    BB BlackOccup;
    BB Occupancy;

    U8 CastlingRights;
    BB EnPassantTarget;

    MoveListWhite WhiteMoves;
    MoveListBlack BlackMoves;

    int WhiteCount;
    int BlackCount;

    GameState(
        const Board& b,
        U8 castlingRights =
            WHITE_KINGSIDE_RIGHT
          | WHITE_QUEENSIDE_RIGHT
          | BLACK_KINGSIDE_RIGHT
          | BLACK_QUEENSIDE_RIGHT,
        BB enPassantTarget = 0
    )
        :
        board(b),
        WhiteOccup(UpdateWhiteOccup(b)),
        BlackOccup(UpdateBlackOccup(b)),
        Occupancy(WhiteOccup | BlackOccup),
        CastlingRights(castlingRights),
        EnPassantTarget(enPassantTarget),
        WhiteMoves(),
        BlackMoves(),
        WhiteCount(0),
        BlackCount(0)
    {
    }
};

inline BB ReverseBits(BB x)
{
    x = ((x & 0x5555555555555555ULL) << 1)  | ((x >> 1)  & 0x5555555555555555ULL);
    x = ((x & 0x3333333333333333ULL) << 2)  | ((x >> 2)  & 0x3333333333333333ULL);
    x = ((x & 0x0F0F0F0F0F0F0F0FULL) << 4)  | ((x >> 4)  & 0x0F0F0F0F0F0F0F0FULL);
    x = ((x & 0x00FF00FF00FF00FFULL) << 8)  | ((x >> 8)  & 0x00FF00FF00FF00FFULL);
    x = ((x & 0x0000FFFF0000FFFFULL) << 16) | ((x >> 16) & 0x0000FFFF0000FFFFULL);
    x = (x << 32) | (x >> 32);
    return x;
}


constexpr BB SetBit(int square)
{
    return BB(1) << square;
}


constexpr int Square(int rank, int file)
{
    return rank * 8 + file;
}

constexpr PawnData generate_pawn_data(int square, int direction, int double_move_rank, int en_passant_rank, int promotion_rank) {
    int rank = square / 8, file = square % 8;
    BB move_one = 0, move_two = 0, capture_left = 0, capture_right = 0;
    BB promotion_move_one = 0, promotion_capture_left = 0, promotion_capture_right = 0, ep_left = 0, ep_right = 0;


    int target_rank = rank + direction;
    if (target_rank >= 0 && target_rank <= 7) {
        move_one = SetBit(Square(target_rank, file));
        if (target_rank == promotion_rank) promotion_move_one = move_one;
    }


    if (rank == double_move_rank) {
        int double_target = rank + direction * 2;
        move_two = SetBit(Square(double_target, file));
    }

    if (target_rank >= 0 && target_rank <= 7 && file > 0) {
        capture_left = SetBit(Square(target_rank, file - 1));
        if (target_rank == promotion_rank) promotion_capture_left = capture_left;
    }

    if (target_rank >= 0 && target_rank <= 7 && file < 7) {
        capture_right = SetBit(Square(target_rank, file + 1));
        if (target_rank == promotion_rank) promotion_capture_right = capture_right;
    }

    if (rank == en_passant_rank) {
        if (file > 0) ep_left = SetBit(Square(target_rank, file - 1));
        if (file < 7) ep_right = SetBit(Square(target_rank, file + 1));
    }

    return PawnData(
        move_one, move_two, capture_left, capture_right,
        promotion_move_one, promotion_capture_left, promotion_capture_right,
        ep_left, ep_right
    );
}




constexpr PawnData generate_white_pawn_data(int square)
{
    return generate_pawn_data(
        square,
        -1, //  direction of the white pieces
        6,  // starting rank
        3,  //  en passant rank
        0   //  promotion rank
    );
}


constexpr PawnData generate_black_pawn_data(int square)
{
    return generate_pawn_data(
        square,
        1,  //Black's direction
        1,  //  starting rank
        4,  // en passant rank
        7   // promotion rank
    );
}


constexpr auto make_white_pawn_table()
{
    std::array<PawnData, 48> table{};

    for (int i = 0; i < 48; ++i)
    {
        table[i] = generate_white_pawn_data(i + 8);
    }

    return table;
}

constexpr auto make_black_pawn_table()
{
    std::array<PawnData, 48> table{};

    for (int i = 0; i < 48; ++i)
    {
        table[i] = generate_black_pawn_data(i + 8);
    }

    return table;
}

struct PawnTables
{
    std::array<PawnData,48> White;
    std::array<PawnData,48> Black;

    constexpr PawnTables()
        : White(make_white_pawn_table()),
          Black(make_black_pawn_table())
    {
    }
};

struct CastleData
{
    BB emptyMask;
    BB kingSafetyMask;
    BB kingFrom;
    BB kingTo;
    BB rookFrom;
    BB rookTo;
    U8 rightMask;
    U8 moveFlag;

    constexpr CastleData(
        BB emptyMask = 0,
        BB kingSafetyMask = 0,
        BB kingFrom = 0,
        BB kingTo = 0,
        BB rookFrom = 0,
        BB rookTo = 0,
        U8 rightMask = 0,
        U8 moveFlag = 0)
        :
        emptyMask(emptyMask),
        kingSafetyMask(kingSafetyMask),
        kingFrom(kingFrom),
        kingTo(kingTo),
        rookFrom(rookFrom),
        rookTo(rookTo),
        rightMask(rightMask),
        moveFlag(moveFlag)
    {}
};

constexpr auto make_white_castle_table()
{
    std::array<CastleData, 2> table{};

    table[0] = CastleData(
        SetBit(61) | SetBit(62),
        SetBit(60) | SetBit(61) | SetBit(62),
        SetBit(60),
        SetBit(62),
        SetBit(63),
        SetBit(61),
        WHITE_KINGSIDE_RIGHT,
        CASTLE_WHITE_KINGSIDE
    );

    table[1] = CastleData(
        SetBit(57) | SetBit(58) | SetBit(59),
        SetBit(60) | SetBit(59) | SetBit(58),
        SetBit(60),
        SetBit(58),
        SetBit(56),
        SetBit(59),
        WHITE_QUEENSIDE_RIGHT,
        CASTLE_WHITE_QUEENSIDE
    );

    return table;
}

constexpr auto make_black_castle_table()
{
    std::array<CastleData, 2> table{};

    table[0] = CastleData(
        SetBit(5) | SetBit(6),
        SetBit(4) | SetBit(5) | SetBit(6),
        SetBit(4),
        SetBit(6),
        SetBit(7),
        SetBit(5),
        BLACK_KINGSIDE_RIGHT,
        CASTLE_BLACK_KINGSIDE
    );

    table[1] = CastleData(
        SetBit(1) | SetBit(2) | SetBit(3),
        SetBit(4) | SetBit(3) | SetBit(2),
        SetBit(4),
        SetBit(2),
        SetBit(0),
        SetBit(3),
        BLACK_QUEENSIDE_RIGHT,
        CASTLE_BLACK_QUEENSIDE
    );

    return table;
}

struct CastleTables
{
    std::array<CastleData, 2> White;
    std::array<CastleData, 2> Black;

    constexpr CastleTables()
        : White(make_white_castle_table()),
          Black(make_black_castle_table())
    {
    }
};

constexpr CastleTables CASTLE_TABLES{};

template<bool White>
inline void RemovePawnAt(Board& board, U8 square)
{
    const BB mask = BB(1) << square;
    const std::size_t file = static_cast<std::size_t>(square & 7);

    if constexpr (White)
    {
        static constexpr std::array<BB Board::*, 8> pawnFiles{
            &Board::WPawn_a7,
            &Board::WPawn_b7,
            &Board::WPawn_c7,
            &Board::WPawn_d7,
            &Board::WPawn_e7,
            &Board::WPawn_f7,
            &Board::WPawn_g7,
            &Board::WPawn_h7
        };

        board.*pawnFiles[file] &= ~mask;
    }
    else
    {
        static constexpr std::array<BB Board::*, 8> pawnFiles{
            &Board::BPawn_a2,
            &Board::BPawn_b2,
            &Board::BPawn_c2,
            &Board::BPawn_d2,
            &Board::BPawn_e2,
            &Board::BPawn_f2,
            &Board::BPawn_g2,
            &Board::BPawn_h2
        };

        board.*pawnFiles[file] &= ~mask;
    }
}

inline void ApplyCastle(Board& board, U8 castleFlag)
{
    switch (castleFlag)
    {
        case CASTLE_WHITE_KINGSIDE:
            board.WKing = (board.WKing & ~(BB(1) << 60)) | (BB(1) << 62);
            board.WRookRight = (board.WRookRight & ~(BB(1) << 63)) | (BB(1) << 61);
            break;

        case CASTLE_WHITE_QUEENSIDE:
            board.WKing = (board.WKing & ~(BB(1) << 60)) | (BB(1) << 58);
            board.WRookLeft = (board.WRookLeft & ~(BB(1) << 56)) | (BB(1) << 59);
            break;

        case CASTLE_BLACK_KINGSIDE:
            board.BKing = (board.BKing & ~(BB(1) << 4)) | (BB(1) << 6);
            board.BRookRight = (board.BRookRight & ~(BB(1) << 7)) | (BB(1) << 5);
            break;

        case CASTLE_BLACK_QUEENSIDE:
            board.BKing = (board.BKing & ~(BB(1) << 4)) | (BB(1) << 2);
            board.BRookLeft = (board.BRookLeft & ~(BB(1) << 0)) | (BB(1) << 3);
            break;

        default:
            break;
    }
}

constexpr BB _generate_diagonal_mask(int x)
{
    BB mask = 0;

    int r = x / 8;
    int f = x % 8;

    for (int i = 0; i < 64; i++)
    {
        if ((i / 8 - i % 8) == (r - f))
        {
            mask |= (BB(1) << i);
        }
    }

    return mask;
}

constexpr BB _generate_antidiag_mask(int x){
    BB mask = 0;

    int r = x / 8;
    int f = x % 8;

    for(int i = 0; i < 64; i++){
        if ((i / 8 + i % 8) == (r + f))
            mask |= (BB(1) << i);      
        }
    return mask;
}

constexpr BB _generate_rank_mask(int x)
{
    BB mask = 0;

    int rank = x / 8;

    for (int i = 0; i < 64; i++)
    {
        if ((i / 8) == rank)
        {
            mask |= (BB(1) << i);
        }
    }

    return mask;
}

constexpr BB _generate_file_mask(int index)
{
    BB mask = 0;

    int file = index % 8;

    for (int i = 0; i < 64; i++)
    {
        if ((i % 8) == file)
        {
            mask |= (BB(1) << i);
        }
    }

    return mask;
}

constexpr BB _generate_king_mask(int square)
{
    BB bb = BB(1) << square;
    BB attacks = 0;

    // pion
    attacks |= bb << 8;
    attacks |= bb >> 8;

    // poziom
    attacks |= (bb << 1) & notFileA;
    attacks |= (bb >> 1) & notFileH;

    // przekątne
    attacks |= (bb << 9) & notFileA;
    attacks |= (bb << 7) & notFileH;
    attacks |= (bb >> 7) & notFileA;
    attacks |= (bb >> 9) & notFileH;

    return attacks;
}

constexpr BB _generate_knight_mask(int square)
{
    BB knight = BB(1) << square;
    BB attacks = 0;

    // góra
    attacks |= (knight << 17) & notFileA;
    attacks |= (knight << 15) & notFileH;
    attacks |= (knight << 10) & notFileAB;
    attacks |= (knight << 6)  & notFileGH;

    // dół
    attacks |= (knight >> 17) & notFileH;
    attacks |= (knight >> 15) & notFileA;
    attacks |= (knight >> 10) & notFileGH;
    attacks |= (knight >> 6)  & notFileAB;

    return attacks;
}



constexpr auto BISHOP_DIAGONALS = []()
{
    std::array<BB, 64> table{};

    for (int i = 0; i < 64; i++)
    {
        table[i] = _generate_diagonal_mask(i);
    }

    return table;

}();


constexpr auto BISHOP_ANTIDIAGONALS = []()
{
    std::array<BB, 64> table{};

    for (int i = 0; i < 64; i++)
    {
        table[i] = _generate_antidiag_mask(i);
    }

    return table;

}();


constexpr auto ROOK_RANKS = []()
{
    std::array<BB, 64> table{};

    for (int i = 0; i < 64; i++)
    {
        table[i] = _generate_rank_mask(i);
    }

    return table;

}();

constexpr auto ROOK_FILES = []()
{
    std::array<BB, 64> table{};

    for (int i = 0; i < 64; i++)
    {
        table[i] = _generate_file_mask(i);
    }

    return table;

}();

constexpr auto KNIGHT_ATTACKS = []()
{
    std::array<BB, 64> table{};

    for (int i = 0; i < 64; i++)
    {
        table[i] = _generate_knight_mask(i);
    }

    return table;

}();

constexpr auto KING_ATTACKS = []()
{
    std::array<BB, 64> table{};

    for (int i = 0; i < 64; i++)
    {
        table[i] = _generate_king_mask(i);
    }

    return table;

}();




inline BB HyperbolaAttacks(BB occupancy,
                           BB pieceBit,
                           BB lineMask)
{
    BB mask = lineMask & ~pieceBit;

    BB forward = occupancy & mask;
    BB reverse = ReverseBits(forward);

    forward -= pieceBit;
    reverse -= ReverseBits(pieceBit);

    forward ^= ReverseBits(reverse);

    return forward & mask;
}
