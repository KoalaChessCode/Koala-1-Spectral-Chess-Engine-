#include "Puct.hpp"
#include "Thread.hpp"
#include "Pipeline.hpp"
#include "events.hpp"
#include "Listener.hpp"

alignas(64) static unsigned char EmbeddedFftBlob[] = {
    #embed "fft_32x8x8.bin"
};



int main (void){
    PipelineContext Context;
    constexpr PawnTables PawnMemorandum{};
    TreeStateWrapper Tree;
    RunPipeline(Tree,0,PawnMemorandum,true,Context,EmbeddedFftBlob);

    return 0;
}

/*
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



*/