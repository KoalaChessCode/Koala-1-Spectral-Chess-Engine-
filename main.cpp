#include "Puct.hpp"
#include "Thread.hpp"
#include "Pipeline.hpp"
#include "events.hpp"
#include "Listener.hpp"
#include "Helpers.hpp"
#include "FEN.hpp"

alignas(64) static unsigned char EmbeddedFftBlob[] = {
    #embed "fft_32x8x8.bin"
};


void debug(const TreeStateWrapper&, const FFTWorking&){
    std::cerr << "[SIZE] Node<Board>: "
              << sizeof(Node<Board>)
              << " bytes\n";

    std::cerr << "[SIZE] Move: "
              << sizeof(Move)
              << " bytes\n";

    std::cerr << "[SIZE] Board: "
          << sizeof(Board) << " B\n";

    std::cerr << "[SIZE] FftImpulse: "
          << sizeof(FftImpulse) << " B\n";

    std::cerr << "[SIZE] FftDelta: "
          << sizeof(FftDelta<4>) << " B\n";

    std::cerr << "[SIZE] Node<Board>: "
          << sizeof(Node<Board>) << " B\n";

}
int main(void)
{
    PipelineContext Context;
    constexpr PawnTables PawnMemorandum{};
    TreeStateWrapper Tree;
    alignas(64) FFTWorking WorkingFFT{};
    InitSpectrum(EmbeddedFftBlob, WorkingFFT);

    debug(Tree,WorkingFFT);
   
      

    uci::Listener listener;

    listener.addListener(
        uci::event::UCI,
        [](uci::arguments_t) {
            std::cout << "uciok\n";
        }
    );

    listener.addListener(
        uci::event::ISREADY,
        [](uci::arguments_t) {
            std::cout << "readyok\n";
        }
    );

    listener.addListener(
        uci::event::POSITION,
        [&Tree, &WorkingFFT,&Context,&PawnMemorandum](uci::arguments_t args) {
            auto it = args.find("fen");

            if (it != args.end())
            {
                const std::string_view fenString = it->second;

                try
                {
                    if (FENUtility::IsInitialPosition(fenString))
                    {
                        // ========================================
                        // Initial position detected.
                        // Skip irregular script (already initialized).
                        // ========================================
                        std::cerr << "[FEN] Using initial position\n";

                    }
                    else
                    {
                        // ========================================
                        // Non-standard position detected.
                        // Apply delta-based FFT transformation.
                        // ========================================
                        std::cerr << "[FEN] Loading custom position: "
                                  << fenString << '\n';
                        FENUtility::IrregularScript(
                            Tree,
                            WorkingFFT,
                            fenString
                        );

                        std::cerr << "[FEN] Custom position loaded\n";
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[FEN ERROR] " << e.what() << '\n';
                }
            }
            else
            {
                // ================================================
                // No FEN provided: use default startpos.
                // ================================================
                std::cerr << "[FEN] Using startpos\n";
            }

            std::cerr << "[UCI] BEFORE PIPELINE\n";

            RunPipeline(
                Tree,
                0,
                PawnMemorandum,
                true,
                Context,
                WorkingFFT
            );
            std::cerr << "[UCI] AFTER PIPELINE\n";

            std::cerr << "[TREE] Nodes used: "
          << Tree.tree.size
          << " / "
          << Tree.tree.capacity
          << '\n';

        }
    );

    listener.addListener(
        uci::event::QUIT,
        [&listener](uci::arguments_t) {
            listener.stopListening();
        }
    );

    std::cerr << "Listener works.\n";

    listener.setupListener();

    return 0;
}