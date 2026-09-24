#pragma once

#include "BoardEncoding.hpp"
#include <cstdint>
#include <fstream>

constexpr std::size_t PieceChannelCount = 32;
constexpr std::size_t PieceBoardRows = 8;
constexpr std::size_t PieceBoardCols = 8;

struct alignas(64) PieceChannelBatch
{
    float planes[PieceChannelCount][PieceBoardRows][PieceBoardCols]{};
};

struct alignas(64) FFTBatch
{
    float real[PieceChannelCount][PieceBoardRows][PieceBoardCols]{};
    float imag[PieceChannelCount][PieceBoardRows][PieceBoardCols]{};
};

constexpr std::size_t FFTScalarCount =
    PieceChannelCount * PieceBoardRows * PieceBoardCols;

struct alignas(64) FftBinaryHeader
{
    char magic[8];
    std::uint32_t channels;
    std::uint32_t rows;
    std::uint32_t cols;
    std::uint32_t reserved;
};

extern "C" void fft_batch_8x8(
    const float* input,
    float* output_real,
    float* output_imag);

inline void PutPieceChannel(
    float matrix[PieceBoardRows][PieceBoardCols],
    BB piece,
    float code)
{
    if (piece == 0)
    {
        return;
    }

    const int square = __builtin_ctzll(piece);
    matrix[MatrixRowFromSquare(square)][MatrixColFromSquare(square)] = code;
}

inline void EncodeBoardToChannels(const Board& board, PieceChannelBatch& batch)
{
    PutPieceChannel(batch.planes[0], board.WRookLeft, WhiteCode(EncodeRookLeft()));
    PutPieceChannel(batch.planes[1], board.WRookRight, WhiteCode(EncodeRookRight()));
    PutPieceChannel(batch.planes[2], board.WBishopLeft, WhiteCode(EncodeBishopLeft()));
    PutPieceChannel(batch.planes[3], board.WBishopRight, WhiteCode(EncodeBishopRight()));
    PutPieceChannel(batch.planes[4], board.WQueen, WhiteCode(EncodeQueen()));
    PutPieceChannel(batch.planes[5], board.WKing, WhiteCode(EncodeKing()));
    PutPieceChannel(batch.planes[6], board.WKnightLeft, WhiteCode(EncodeKnightLeft()));
    PutPieceChannel(batch.planes[7], board.WKnightRight, WhiteCode(EncodeKnightRight()));

    PutPieceChannel(batch.planes[8], board.BRookLeft, BlackCode(EncodeRookLeft()));
    PutPieceChannel(batch.planes[9], board.BRookRight, BlackCode(EncodeRookRight()));
    PutPieceChannel(batch.planes[10], board.BBishopLeft, BlackCode(EncodeBishopLeft()));
    PutPieceChannel(batch.planes[11], board.BBishopRight, BlackCode(EncodeBishopRight()));
    PutPieceChannel(batch.planes[12], board.BQueen, BlackCode(EncodeQueen()));
    PutPieceChannel(batch.planes[13], board.BKing, BlackCode(EncodeKing()));
    PutPieceChannel(batch.planes[14], board.BKnightLeft, BlackCode(EncodeKnightLeft()));
    PutPieceChannel(batch.planes[15], board.BKnightRight, BlackCode(EncodeKnightRight()));

    PutPieceChannel(batch.planes[16], board.BPawn_a2, BlackCode(EncodePawn(0)));
    PutPieceChannel(batch.planes[17], board.BPawn_b2, BlackCode(EncodePawn(1)));
    PutPieceChannel(batch.planes[18], board.BPawn_c2, BlackCode(EncodePawn(2)));
    PutPieceChannel(batch.planes[19], board.BPawn_d2, BlackCode(EncodePawn(3)));
    PutPieceChannel(batch.planes[20], board.BPawn_e2, BlackCode(EncodePawn(4)));
    PutPieceChannel(batch.planes[21], board.BPawn_f2, BlackCode(EncodePawn(5)));
    PutPieceChannel(batch.planes[22], board.BPawn_g2, BlackCode(EncodePawn(6)));
    PutPieceChannel(batch.planes[23], board.BPawn_h2, BlackCode(EncodePawn(7)));

    PutPieceChannel(batch.planes[24], board.WPawn_a7, WhiteCode(EncodePawn(0)));
    PutPieceChannel(batch.planes[25], board.WPawn_b7, WhiteCode(EncodePawn(1)));
    PutPieceChannel(batch.planes[26], board.WPawn_c7, WhiteCode(EncodePawn(2)));
    PutPieceChannel(batch.planes[27], board.WPawn_d7, WhiteCode(EncodePawn(3)));
    PutPieceChannel(batch.planes[28], board.WPawn_e7, WhiteCode(EncodePawn(4)));
    PutPieceChannel(batch.planes[29], board.WPawn_f7, WhiteCode(EncodePawn(5)));
    PutPieceChannel(batch.planes[30], board.WPawn_g7, WhiteCode(EncodePawn(6)));
    PutPieceChannel(batch.planes[31], board.WPawn_h7, WhiteCode(EncodePawn(7)));
}

inline void ComputeFftBatch(
    const PieceChannelBatch& input,
    FFTBatch& output)
{
    fft_batch_8x8(
        reinterpret_cast<const float*>(input.planes),
        reinterpret_cast<float*>(output.real),
        reinterpret_cast<float*>(output.imag));
}

inline bool WriteFftBinary(const char* path, const FFTBatch& fft)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        return false;
    }

    const FftBinaryHeader header{
        {'C', 'H', 'F', 'F', 'T', '8', 'B', '0'},
        static_cast<std::uint32_t>(PieceChannelCount),
        static_cast<std::uint32_t>(PieceBoardRows),
        static_cast<std::uint32_t>(PieceBoardCols),
        0
    };

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (std::size_t channel = 0; channel < PieceChannelCount; ++channel)
    {
        for (std::size_t row = 0; row < PieceBoardRows; ++row)
        {
            for (std::size_t col = 0; col < PieceBoardCols; ++col)
            {
                const float real = fft.real[channel][row][col];
                const float imag = fft.imag[channel][row][col];
                out.write(reinterpret_cast<const char*>(&real), sizeof(real));
                out.write(reinterpret_cast<const char*>(&imag), sizeof(imag));
            }
        }
    }

    return static_cast<bool>(out);
}
