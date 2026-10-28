#include "Tree.hpp"
#include "Thread.hpp"
#include "Pipeline.hpp"
#include "events.hpp"
#include "Listener.hpp"
#include "Helpers.hpp"
#include "FEN.hpp"
#include "GATv2WrapperDFS.hpp"
#include "GATv2Kernels.hpp"

alignas(64)  unsigned char EmbeddedFftBlob[] = {
    #embed "weights/fft_32x8x8.bin"
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
    alignas(64) FFTWorking rootFFT;
    std::memcpy(&rootFFT, &WorkingFFT, sizeof(FFTWorking));


    debug(Tree,WorkingFFT);
   



    
    Arena gatv2Arena(2 * 1024 * 1024);    

    if (!Context.allocate(gatv2Arena))
    {
        std::cerr << "[INIT] PipelineContext allocation failed\n";
        return 1;
    }

    // GATv2 network + workspace
    auto gatv2Net = std::make_unique<DefaultGATv2Network>();
    if (!InitDefaultGATv2(*gatv2Net))
    {
        std::cerr << "[INIT] GATv2 init failed\n";
        return 1;
    }

    auto gatv2Ws = std::make_unique<DefaultGATv2Workspace>();
    if (!InitDefaultGATv2Workspace(*gatv2Ws))
    {
        std::cerr << "[INIT] GATv2 workspace init failed\n";
        return 1;
    }

    Context.gatv2 = gatv2Net.get();
    Context.gatv2_workspace = gatv2Ws.get();


    std::cerr << "[INIT] fft_working_history: "
          << Context.fft_working_history << '\n';
std::cerr << "[INIT] fft_interleaved: "
          << Context.fft_interleaved << '\n';



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
        [&Tree, &WorkingFFT,&rootFFT,&Context,&PawnMemorandum](uci::arguments_t args) {
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

        std::cerr << "[UCI] BEFORE GATv2\n";

        auto t0 = std::chrono::high_resolution_clock::now();

        RunGATv2OverTree(
    Tree,
    Context,
    rootFFT
);

        auto t1 = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    t1 - t0
).count();

std::cerr << "[UCI] AFTER GATv2: " << ms << " ms\n";

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