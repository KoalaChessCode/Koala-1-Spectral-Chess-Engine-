#include "Constants.hpp"
#include "Delta.hpp"
#include "Puct.hpp"
#include <string_view>

namespace FENUtility
{

struct ParsedFEN
{
    Board board{};

    bool whiteToMove = true;

    bool whiteKingSideCastle  = false;
    bool whiteQueenSideCastle = false;
    bool blackKingSideCastle  = false;
    bool blackQueenSideCastle = false;

    int enPassantSquare = -1;

    unsigned halfmoveClock = 0;
    unsigned fullmoveNumber = 1;
};


inline Board ParseBoardField(std::string_view fen)
{
    Board board{};

    // Board starts with the normal position.
    // FEN parsing must therefore clear every channel first.
    for (BB& pieces : board.Pieces)
        pieces = 0;

    std::size_t pos = 0;

    int rank = 7;
    int file = 0;

    std::uint8_t whiteRooks   = 0;
    std::uint8_t blackRooks   = 0;

    std::uint8_t whiteKnights = 0;
    std::uint8_t blackKnights = 0;

    std::uint8_t whiteBishops = 0;
    std::uint8_t blackBishops = 0;

    std::uint8_t whiteQueens = 0;
    std::uint8_t blackQueens = 0;

    std::uint8_t whiteKings = 0;
    std::uint8_t blackKings = 0;

    std::uint8_t whitePawns = 0;
    std::uint8_t blackPawns = 0;

    while (pos < fen.size() && fen[pos] != ' ')
    {
        const char c = fen[pos];

        // -----------------------------------------------------
        // Rank separator
        // -----------------------------------------------------

        if (c == '/')
        {
            if (file != 8)
                throw std::invalid_argument("Invalid FEN rank");

            if (rank == 0)
                throw std::invalid_argument("Too many FEN ranks");

            file = 0;
            --rank;
            ++pos;
            continue;
        }

        // -----------------------------------------------------
        // Empty squares
        // -----------------------------------------------------

        if (c >= '1' && c <= '8')
        {
            file += c - '0';

            if (file > 8)
                throw std::invalid_argument("Too many squares in FEN rank");

            ++pos;
            continue;
        }

        if (file >= 8)
            throw std::invalid_argument("Too many squares in FEN rank");

        const std::uint8_t square =
            static_cast<std::uint8_t>(rank * 8 + file);

        const BB bit = BB(1) << square;

        // -----------------------------------------------------
        // White pawn
        // -----------------------------------------------------

        if (c == 'P')
        {
            // White pawns cannot legally stand on rank 1.
            if (rank == 0)
                throw std::invalid_argument(
                    "White pawn on first rank"
                );

            if (whitePawns >= 8)
                throw std::invalid_argument(
                    "Too many white pawns"
                );

            switch (file)
            {
                case 0: board.WPawn_a7 |= bit; break;
                case 1: board.WPawn_b7 |= bit; break;
                case 2: board.WPawn_c7 |= bit; break;
                case 3: board.WPawn_d7 |= bit; break;
                case 4: board.WPawn_e7 |= bit; break;
                case 5: board.WPawn_f7 |= bit; break;
                case 6: board.WPawn_g7 |= bit; break;
                case 7: board.WPawn_h7 |= bit; break;
            }

            ++whitePawns;
            ++file;
            ++pos;
            continue;
        }

        // -----------------------------------------------------
        // Black pawn
        // -----------------------------------------------------

        if (c == 'p')
        {
            // Black pawns cannot legally stand on rank 8.
            if (rank == 7)
                throw std::invalid_argument(
                    "Black pawn on eighth rank"
                );

            if (blackPawns >= 8)
                throw std::invalid_argument(
                    "Too many black pawns"
                );

            switch (file)
            {
                case 0: board.BPawn_a2 |= bit; break;
                case 1: board.BPawn_b2 |= bit; break;
                case 2: board.BPawn_c2 |= bit; break;
                case 3: board.BPawn_d2 |= bit; break;
                case 4: board.BPawn_e2 |= bit; break;
                case 5: board.BPawn_f2 |= bit; break;
                case 6: board.BPawn_g2 |= bit; break;
                case 7: board.BPawn_h2 |= bit; break;
            }

            ++blackPawns;
            ++file;
            ++pos;
            continue;
        }

        // -----------------------------------------------------
        // White rook
        // -----------------------------------------------------

        if (c == 'R')
        {
            if (whiteRooks == 0)
                board.WRookLeft = bit;
            else if (whiteRooks == 1)
                board.WRookRight = bit;
            else
                throw std::invalid_argument(
                    "Board cannot represent more than two white rooks"
                );

            ++whiteRooks;
        }

        // -----------------------------------------------------
        // Black rook
        // -----------------------------------------------------

        else if (c == 'r')
        {
            if (blackRooks == 0)
                board.BRookLeft = bit;
            else if (blackRooks == 1)
                board.BRookRight = bit;
            else
                throw std::invalid_argument(
                    "Board cannot represent more than two black rooks"
                );

            ++blackRooks;
        }

        // -----------------------------------------------------
        // White knight
        // -----------------------------------------------------

        else if (c == 'N')
        {
            if (whiteKnights == 0)
                board.WKnightLeft = bit;
            else if (whiteKnights == 1)
                board.WKnightRight = bit;
            else
                throw std::invalid_argument(
                    "Board cannot represent more than two white knights"
                );

            ++whiteKnights;
        }

        // -----------------------------------------------------
        // Black knight
        // -----------------------------------------------------

        else if (c == 'n')
        {
            if (blackKnights == 0)
                board.BKnightLeft = bit;
            else if (blackKnights == 1)
                board.BKnightRight = bit;
            else
                throw std::invalid_argument(
                    "Board cannot represent more than two black knights"
                );

            ++blackKnights;
        }

        // -----------------------------------------------------
        // White bishop
        // -----------------------------------------------------

        else if (c == 'B')
        {
            if (whiteBishops == 0)
                board.WBishopLeft = bit;
            else if (whiteBishops == 1)
                board.WBishopRight = bit;
            else
                throw std::invalid_argument(
                    "Board cannot represent more than two white bishops"
                );

            ++whiteBishops;
        }

        // -----------------------------------------------------
        // Black bishop
        // -----------------------------------------------------

        else if (c == 'b')
        {
            if (blackBishops == 0)
                board.BBishopLeft = bit;
            else if (blackBishops == 1)
                board.BBishopRight = bit;
            else
                throw std::invalid_argument(
                    "Board cannot represent more than two black bishops"
                );

            ++blackBishops;
        }

        // -----------------------------------------------------
        // White queen
        // -----------------------------------------------------

        else if (c == 'Q')
        {
            if (whiteQueens != 0)
                throw std::invalid_argument(
                    "Board cannot represent multiple white queens"
                );

            board.WQueen = bit;
            ++whiteQueens;
        }

        // -----------------------------------------------------
        // Black queen
        // -----------------------------------------------------

        else if (c == 'q')
        {
            if (blackQueens != 0)
                throw std::invalid_argument(
                    "Board cannot represent multiple black queens"
                );

            board.BQueen = bit;
            ++blackQueens;
        }

        // -----------------------------------------------------
        // White king
        // -----------------------------------------------------

        else if (c == 'K')
        {
            if (whiteKings != 0)
                throw std::invalid_argument(
                    "Multiple white kings"
                );

            board.WKing = bit;
            ++whiteKings;
        }

        // -----------------------------------------------------
        // Black king
        // -----------------------------------------------------

        else if (c == 'k')
        {
            if (blackKings != 0)
                throw std::invalid_argument(
                    "Multiple black kings"
                );

            board.BKing = bit;
            ++blackKings;
        }

        // -----------------------------------------------------
        // Invalid character
        // -----------------------------------------------------

        else
        {
            throw std::invalid_argument(
                "Invalid FEN piece"
            );
        }

        ++file;
        ++pos;
    }

    // ---------------------------------------------------------
    // Final board validation
    // ---------------------------------------------------------

    if (file != 8)
        throw std::invalid_argument(
            "Incomplete FEN rank"
        );

    if (rank != 0)
        throw std::invalid_argument(
            "Incomplete FEN board"
        );

    if (whiteKings != 1)
        throw std::invalid_argument(
            "FEN must contain exactly one white king"
        );

    if (blackKings != 1)
        throw std::invalid_argument(
            "FEN must contain exactly one black king"
        );

    return board;
}


inline ParsedFEN ParseFEN(std::string_view fen)
{
    ParsedFEN result{};

    // =========================================================
    // 1. BOARD
    // =========================================================

    result.board = ParseBoardField(fen);

    std::size_t pos = 0;

    while (pos < fen.size() && fen[pos] != ' ')
        ++pos;

    if (pos == fen.size())
        throw std::invalid_argument(
            "Incomplete FEN"
        );

    // =========================================================
    // 2. SIDE TO MOVE
    // =========================================================

    ++pos;

    if (pos >= fen.size())
        throw std::invalid_argument(
            "Missing side to move"
        );

    if (fen[pos] == 'w')
        result.whiteToMove = true;
    else if (fen[pos] == 'b')
        result.whiteToMove = false;
    else
        throw std::invalid_argument(
            "Invalid side to move"
        );

    while (pos < fen.size() && fen[pos] != ' ')
        ++pos;

    // =========================================================
    // 3. CASTLING
    // =========================================================

    if (pos == fen.size())
        throw std::invalid_argument(
            "Missing castling field"
        );

    ++pos;

    if (pos >= fen.size())
        throw std::invalid_argument(
            "Missing castling field"
        );

    if (fen[pos] == '-')
    {
        ++pos;
    }
    else
    {
        bool seenK = false;
        bool seenQ = false;
        bool seenk = false;
        bool seenq = false;

        while (pos < fen.size() && fen[pos] != ' ')
        {
            switch (fen[pos])
            {
                case 'K':
                    if (seenK)
                        throw std::invalid_argument(
                            "Duplicate K castling flag"
                        );
                    seenK = true;
                    result.whiteKingSideCastle = true;
                    break;

                case 'Q':
                    if (seenQ)
                        throw std::invalid_argument(
                            "Duplicate Q castling flag"
                        );
                    seenQ = true;
                    result.whiteQueenSideCastle = true;
                    break;

                case 'k':
                    if (seenk)
                        throw std::invalid_argument(
                            "Duplicate k castling flag"
                        );
                    seenk = true;
                    result.blackKingSideCastle = true;
                    break;

                case 'q':
                    if (seenq)
                        throw std::invalid_argument(
                            "Duplicate q castling flag"
                        );
                    seenq = true;
                    result.blackQueenSideCastle = true;
                    break;

                default:
                    throw std::invalid_argument(
                        "Invalid castling rights"
                    );
            }

            ++pos;
        }
    }

    // =========================================================
    // 4. EN PASSANT
    // =========================================================

    if (pos == fen.size())
        throw std::invalid_argument(
            "Missing en passant field"
        );

    ++pos;

    if (pos >= fen.size())
        throw std::invalid_argument(
            "Missing en passant field"
        );

    if (fen[pos] == '-')
    {
        result.enPassantSquare = -1;
        ++pos;
    }
    else
    {
        if (pos + 1 >= fen.size())
            throw std::invalid_argument(
                "Invalid en passant square"
            );

        const char fileChar = fen[pos];
        const char rankChar = fen[pos + 1];

        if (fileChar < 'a' || fileChar > 'h' ||
            rankChar < '1' || rankChar > '8')
        {
            throw std::invalid_argument(
                "Invalid en passant square"
            );
        }

        const int epFile = fileChar - 'a';
        const int epRank = rankChar - '1';

        // FEN en-passant target can only be on rank 3 or 6.
        if (epRank != 2 && epRank != 5)
            throw std::invalid_argument(
                "Invalid en passant rank"
            );

        result.enPassantSquare =
            epRank * 8 + epFile;

        pos += 2;
    }

    // =========================================================
    // 5. HALFMOVE CLOCK
    // =========================================================

    if (pos == fen.size())
        throw std::invalid_argument(
            "Missing halfmove clock"
        );

    ++pos;

    unsigned value = 0;

    if (pos >= fen.size())
        throw std::invalid_argument(
            "Missing halfmove clock"
        );

    while (pos < fen.size() && fen[pos] != ' ')
    {
        if (fen[pos] < '0' || fen[pos] > '9')
            throw std::invalid_argument(
                "Invalid halfmove clock"
            );

        value =
            value * 10u +
            static_cast<unsigned>(fen[pos] - '0');

        ++pos;
    }

    result.halfmoveClock = value;

    // =========================================================
    // 6. FULLMOVE NUMBER
    // =========================================================

    if (pos == fen.size())
        throw std::invalid_argument(
            "Missing fullmove number"
        );

    ++pos;

    if (pos >= fen.size())
        throw std::invalid_argument(
            "Missing fullmove number"
        );

    value = 0;

    while (pos < fen.size())
    {
        if (fen[pos] < '0' || fen[pos] > '9')
            throw std::invalid_argument(
                "Invalid fullmove number"
            );

        value =
            value * 10u +
            static_cast<unsigned>(fen[pos] - '0');

        ++pos;
    }

    if (value == 0)
        throw std::invalid_argument(
            "Invalid fullmove number"
        );

    result.fullmoveNumber = value;

    return result;
}


inline bool IsInitialPosition(std::string_view fen) noexcept
{
    const std::size_t space = fen.find(' ');

    const std::string_view board =
        fen.substr(0, space);

    const std::size_t initialSpace =
        INITIAL_FEN.find(' ');

    const std::string_view initialBoard =
        INITIAL_FEN.substr(0, initialSpace);

    return board == initialBoard;
}


inline void IrregularScript(
    TreeStateWrapper& Tree,
    FFTWorking& WorkingFFT,
    std::string_view customFEN
)
{
    const Board& defaultStartBoard =
        Tree.tree.At(0).position;

    const auto parsed =
        FENUtility::ParseFEN(customFEN);

    const auto initialDelta =
        MakeFftDelta<64>(
            defaultStartBoard,
            parsed.board
        );

    Tree.tree.At(0).position =
        parsed.board;

    Tree.tree.At(0).fftDelta = {};

    ApplyFftDelta(
        WorkingFFT,
        initialDelta
    );
}


}