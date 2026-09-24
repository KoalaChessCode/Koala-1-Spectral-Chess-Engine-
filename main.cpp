#include "Helpers.hpp"
#include "Puct.hpp"
#include "Pipeline.hpp"
#include "events.hpp"
#include "Listener.hpp"

alignas(64) static unsigned char EmbeddedFftBlob[] = {
    #embed "fft_32x8x8.bin"
};

int main (void){
    auto& ptr = InitSpectrum(EmbeddedFftBlob);
    constexpr PawnTables PawnMemorandum{};
    Board Plank;
    GameState InstanceOne(Plank);
    TreeStateWrapper treeState(1000);
    
    uci::Listener listener;

    listener.addListener(uci::event::UCI, [](uci::arguments_t) {
        std::cout << "uciok\n";
    });

    listener.addListener(uci::event::ISREADY, [](uci::arguments_t) {
        std::cout << "readyok\n";
    });

    listener.addListener(
        uci::event::POSITION,
        [](uci::arguments_t args) {
            for (const auto& [key, value] : args) {
                std::cout << key << " = " << value << '\n';
            }
        }
    );

    listener.addListener(uci::event::QUIT, [&listener](uci::arguments_t) {
        listener.stopListening();
    });

    std::cerr << "Listener works.\n";

    listener.setupListener();

    AllWhiteLegalMoves(
        InstanceOne.WhiteMoves,
        InstanceOne.WhiteCount,
        InstanceOne,
        PawnMemorandum
    );


     AllBlackLegalMoves(
        InstanceOne,
        PawnMemorandum,
        InstanceOne.BlackMoves,
        InstanceOne.BlackCount
    );

    return 0;
}

