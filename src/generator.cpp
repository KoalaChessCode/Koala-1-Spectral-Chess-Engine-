#include "Constants.hpp"
#include "BoardEncoding.hpp"
#include "FFTBatch.hpp"

int main (int argc, char* argv[]){
    const char* sciezka_zapisu = (argc > 1) ? argv[1] : "fft_32x8x8.bin";
    Board Plank;
    PieceChannelBatch input{};
    FFTBatch output{};

    EncodeBoardToChannels(Plank, input);
    ComputeFftBatch(input, output);
    WriteFftBinary(sciezka_zapisu, output);

    return 0;
}

