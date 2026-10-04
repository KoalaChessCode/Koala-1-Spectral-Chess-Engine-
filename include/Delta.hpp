#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <cmath>

struct FftImpulse
{
    std::uint8_t channel = 0;
    std::uint8_t square  = 0;
    float value = 0.0f;
};

struct FftDelta
{
// Maximum:
//
// normal move:
// from -, to +
//
// capture:
// move from -, move to +, capture to -
//
// promotion with capture:
// pawn from -, promoted piece to +, captured piece to -
//
// en passant:
// pawn from -, pawn to +, captured pawn -
//
// castling:
// king from -, king to +,
// rook from -, rook to +
//
// => maximum 4 pulses.
    std::uint8_t count = 0;
    std::array<FftImpulse, 4> impulses{};

    inline void Add(
        std::uint8_t channel,
        std::uint8_t square,
        float value
    ) noexcept
    {
        // Legalny ruch nigdy nie powinien przekroczyć 4.
        if (count >= impulses.size())
            return;

        impulses[count++] = {
            channel,
            square,
            value
        };
    }
};

// ============================================================
// 32-CHANNEL FFT CODES
// ============================================================

constexpr std::array<float, 32> FFTChannelCodes =
{
    // White pieces
     5.1f,   // WRookLeft
     5.2f,   // WRookRight
     3.3f,   // WBishopLeft
     3.4f,   // WBishopRight
     9.0f,   // WQueen
    10.0f,   // WKing
     3.1f,   // WKnightLeft
     3.2f,   // WKnightRight

    // Black pieces
    -5.1f,   // BRookLeft
    -5.2f,   // BRookRight
    -3.3f,   // BBishopLeft
    -3.4f,   // BBishopRight
    -9.0f,   // BQueen
   -10.0f,   // BKing
    -3.1f,   // BKnightLeft
    -3.2f,   // BKnightRight

    // Black pawns
    -1.1f,   // BPawn_a2
    -1.2f,   // BPawn_b2
    -1.3f,   // BPawn_c2
    -1.4f,   // BPawn_d2
    -1.5f,   // BPawn_e2
    -1.6f,   // BPawn_f2
    -1.7f,   // BPawn_g2
    -1.8f,   // BPawn_h2

    // White pawns
     1.1f,   // WPawn_a7
     1.2f,   // WPawn_b7
     1.3f,   // WPawn_c7
     1.4f,   // WPawn_d7
     1.5f,   // WPawn_e7
     1.6f,   // WPawn_f7
     1.7f,   // WPawn_g7
     1.8f    // WPawn_h7
};


// ============================================================
// FFT DELTA: BOARD -> BOARD
// ============================================================
//
// This is the most important function.
//
// We are not concerned with Move::Flags here.
//
// We compare:
//     parent.Pieces[channel]
//     child.Pieces[channel]
//
// This allows us to automatically handle:
//
// QUIET
// DOUBLE_PUSH
// CAPTURE
// PROMOTION
// EN_PASSANT
// CASTLING
//
// ============================================================

inline FftDelta MakeFftDelta(
    const Board& parent,
    const Board& child
) noexcept
{
    FftDelta delta;

    for (std::uint8_t channel = 0; channel < 32; ++channel)
    {
        const BB oldPieces = parent.Pieces[channel];
        const BB newPieces = child.Pieces[channel];

        if (oldPieces == newPieces)
            continue;

        const float code = FFTChannelCodes[channel];

// ----------------------------------------------------
// The piece disappears from the old square. 
// ----------------------------------------------------


        if (oldPieces != 0)
        {
            const std::uint8_t oldSquare =
                static_cast<std::uint8_t>(
                    __builtin_ctzll(oldPieces)
                );

            delta.Add(
                channel,
                oldSquare,
                -code
            );
        }

    // ----------------------------------------------------
// The piece appears on a new square. 
// ----------------------------------------------------

        if (newPieces != 0)
        {
            const std::uint8_t newSquare =
                static_cast<std::uint8_t>(
                    __builtin_ctzll(newPieces)
                );

            delta.Add(
                channel,
                newSquare,
                code
            );
        }
    }

    return delta;
}


// ============================================================
// FFT BUFFER
// ============================================================

constexpr std::size_t PieceChannelCount = 32;
constexpr std::size_t PieceBoardRows = 8;
constexpr std::size_t PieceBoardCols = 8;

struct alignas(64) FFTWorking
{
    float real[
        PieceChannelCount
    ][PieceBoardRows]
     [PieceBoardCols]{};

    float imag[
        PieceChannelCount
    ][PieceBoardRows]
     [PieceBoardCols]{};
};


// ============================================================
// MAPPING SQUARE -> FFT ROW/COL
// ============================================================

constexpr std::size_t FFTRowFromSquare(
    std::uint8_t square
)
{
    return 7u -
        static_cast<std::size_t>(square / 8);
}

constexpr std::size_t FFTColFromSquare(
    std::uint8_t square
)
{
    return static_cast<std::size_t>(square % 8);
}

// ============================================================
// SINGLE IMPULSE -> FFT
// ============================================================
//
// For the point:
//
//     value * delta(row,col)
//
// The 2D FFT yields:
//
//     value * exp(
//         -2*pi*i *
//
//         (frequencyRow * row +
//          frequencyCol * col) / 8
//     )
//
// ============================================================

inline void ApplyImpulseFFT(
    FFTWorking& fft,
    const FftImpulse& impulse
) noexcept
{
    const std::size_t inputRow =
        FFTRowFromSquare(impulse.square);

    const std::size_t inputCol =
        FFTColFromSquare(impulse.square);

    constexpr float Pi =
        3.14159265358979323846f;

    constexpr float TwoPi =
        2.0f * Pi;

    for (std::size_t frequencyRow = 0;
         frequencyRow < 8;
         ++frequencyRow)
    {
        for (std::size_t frequencyCol = 0;
             frequencyCol < 8;
             ++frequencyCol)
        {
            const float phase =
                -TwoPi *
                (
                    static_cast<float>(
                        frequencyRow * inputRow
                    )
                    +
                    static_cast<float>(
                        frequencyCol * inputCol
                    )
                )
                / 8.0f;

            const float c =
                std::cos(phase);

            const float s =
                std::sin(phase);

            fft.real
                [impulse.channel]
                [frequencyRow]
                [frequencyCol]
                += impulse.value * c;

            fft.imag
                [impulse.channel]
                [frequencyRow]
                [frequencyCol]
                += impulse.value * s;
        }
    }
}


// ============================================================
// APPLY DELTA
// ============================================================

inline void ApplyFftDelta(
    FFTWorking& fft,
    const FftDelta& delta
) noexcept
{
    for (std::uint8_t i = 0;
         i < delta.count;
         ++i)
    {
        ApplyImpulseFFT(
            fft,
            delta.impulses[i]
        );
    }
}


// ============================================================
// REMOVE DELTA
// ============================================================
//
// DFS:
//
//     Node0
//       ↓ +delta1
//     Node1
//       ↓ +delta2
//     Node2
//
// back:
//
//     Node2
//       ↓ -delta2
//     Node1
//       ↓ -delta1
//     Node0
//
// ============================================================

inline void RemoveFftDelta(
    FFTWorking& fft,
    const FftDelta& delta
) noexcept
{
    for (std::uint8_t i = 0;
         i < delta.count;
         ++i)
    {
        FftImpulse inverse =
            delta.impulses[i];

        inverse.value =
            -inverse.value;

        ApplyImpulseFFT(
            fft,
            inverse
        );
    }
}

