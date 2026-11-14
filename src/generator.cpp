#include "Constants.hpp"
#include "BoardEncoding.hpp"
#include "FFTBatch.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{
void Fft1D8(double real[8], double imag[8])
{
    constexpr int bit_reversed[8] = {0, 4, 2, 6, 1, 5, 3, 7};
    double temporary_real[8];
    double temporary_imag[8];

    for (int i = 0; i < 8; ++i)
    {
        temporary_real[i] = real[bit_reversed[i]];
        temporary_imag[i] = imag[bit_reversed[i]];
    }
    for (int i = 0; i < 8; ++i)
    {
        real[i] = temporary_real[i];
        imag[i] = temporary_imag[i];
    }

    for (int step = 2; step <= 8; step *= 2)
    {
        const int half = step / 2;
        for (int start = 0; start < 8; start += step)
        {
            for (int j = 0; j < half; ++j)
            {
                const double angle =
                    -2.0 * std::acos(-1.0) * static_cast<double>(j) /
                    static_cast<double>(step);
                const double wr = std::cos(angle);
                const double wi = std::sin(angle);

                const double ur = real[start + j];
                const double ui = imag[start + j];
                const double vr = real[start + j + half];
                const double vi = imag[start + j + half];
                const double tr = wr * vr - wi * vi;
                const double ti = wr * vi + wi * vr;

                real[start + j] = ur + tr;
                imag[start + j] = ui + ti;
                real[start + j + half] = ur - tr;
                imag[start + j + half] = ui - ti;
            }
        }
    }
}

bool WriteByteInclude(const char* path, const char* binary_path)
{
    std::ifstream binary(binary_path, std::ios::binary);
    if (!binary)
    {
        return false;
    }

    constexpr std::size_t binary_size =
        sizeof(FftBinaryHeader) + FFTScalarCount * 2 * sizeof(float);
    std::array<unsigned char, binary_size> bytes{};
    binary.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (binary.gcount() != static_cast<std::streamsize>(bytes.size()) ||
        binary.peek() != std::char_traits<char>::eof())
    {
        return false;
    }

    std::ofstream output(path);
    if (!output)
    {
        return false;
    }
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
        {
            output << ", ";
        }
        if (i % 12 == 0)
        {
            output << '\n';
        }
        output << "0x" << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    output << '\n';
    return static_cast<bool>(output);
}
}

extern "C" void fft_batch_8x8(
    const float* input,
    float* output_real,
    float* output_imag)
{
    for (int channel = 0; channel < 32; ++channel)
    {
        const int base = channel * 64;
        double plane_real[8][8];
        double plane_imag[8][8]{};
        double row_real[8];
        double row_imag[8];
        double column_real[8];
        double column_imag[8];

        for (int row = 0; row < 8; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                const int index = base + row * 8 + col;
                plane_real[row][col] = static_cast<double>(input[index]);
            }
        }

        for (int row = 0; row < 8; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                row_real[col] = plane_real[row][col];
                row_imag[col] = plane_imag[row][col];
            }
            Fft1D8(row_real, row_imag);
            for (int col = 0; col < 8; ++col)
            {
                plane_real[row][col] = row_real[col];
                plane_imag[row][col] = row_imag[col];
            }
        }

        for (int col = 0; col < 8; ++col)
        {
            for (int row = 0; row < 8; ++row)
            {
                column_real[row] = plane_real[row][col];
                column_imag[row] = plane_imag[row][col];
            }
            Fft1D8(column_real, column_imag);
            for (int row = 0; row < 8; ++row)
            {
                const int index = base + row * 8 + col;
                output_real[index] = static_cast<float>(column_real[row]);
                output_imag[index] = static_cast<float>(column_imag[row]);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " [binary-output] [include-output]\n";
        return 2;
    }

    const char* binary_path = argc > 1 ? argv[1] : "fft_32x8x8.bin";
    const std::string include_path =
        argc > 2 ? argv[2] : std::string(binary_path) + ".inc";
    Board Plank;
    PieceChannelBatch input{};
    FFTBatch output{};

    EncodeBoardToChannels(Plank, input);
    ComputeFftBatch(input, output);
    if (!WriteFftBinary(binary_path, output))
    {
        std::cerr << "Could not write FFT binary: " << binary_path << '\n';
        return 1;
    }
    if (!WriteByteInclude(include_path.c_str(), binary_path))
    {
        std::cerr << "Could not write FFT include: " << include_path << '\n';
        return 1;
    }

    return 0;
}

