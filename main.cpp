#include "Tree.hpp"
#include "Thread.hpp"
#include "Pipeline.hpp"
#include "events.hpp"
#include "Listener.hpp"
#include "Helpers.hpp"
#include "FEN.hpp"
#include "GATv2Wrapper.hpp"
#include "GATv2Kernels.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

GameState CURRENT_POSITION{Board{}};

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
    bool whiteToMove = true;

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
        [&Tree, &WorkingFFT, &rootFFT, &whiteToMove](uci::arguments_t args) {
            auto it = args.find("fen");
            if (it != args.end())
            {
                const std::string_view fenString = it->second;

                try
                {
                    const auto parsedPosition =
                        FENUtility::ParseFEN(fenString);

                    whiteToMove = parsedPosition.whiteToMove;

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

                    U8 castlingRights = 0;
                    if (parsedPosition.whiteKingSideCastle)
                        castlingRights |= WHITE_KINGSIDE_RIGHT;
                    if (parsedPosition.whiteQueenSideCastle)
                        castlingRights |= WHITE_QUEENSIDE_RIGHT;
                    if (parsedPosition.blackKingSideCastle)
                        castlingRights |= BLACK_KINGSIDE_RIGHT;
                    if (parsedPosition.blackQueenSideCastle)
                        castlingRights |= BLACK_QUEENSIDE_RIGHT;

                    const BB enPassantTarget =
                        parsedPosition.enPassantSquare >= 0
                        ? BB(1) << parsedPosition.enPassantSquare
                        : 0;

                    CURRENT_POSITION = GameState(
                        parsedPosition.board,
                        castlingRights,
                        enPassantTarget
                    );
                     
                }
               
                catch (const std::exception& e)
                {
                    std::cerr << "[FEN ERROR] " << e.what() << '\n';
                }
            }
            else if (args.contains("startpos")) {
                try {
                    const auto parsedPosition = FENUtility::ParseFEN(INITIAL_FEN);
                    whiteToMove = parsedPosition.whiteToMove;
                    CURRENT_POSITION = GameState(parsedPosition.board);
                    std::memcpy(&WorkingFFT, &rootFFT, sizeof(FFTWorking));
                } catch (const std::exception& e) {
                    std::cerr << "[FEN ERROR] " << e.what() << '\n';
                }
            }
            Tree.tree.nodes[0].position = CURRENT_POSITION.board;
        }
    );

    listener.addListener(
        uci::event::GO,
        [&Tree, &WorkingFFT, &Context, &PawnMemorandum, &whiteToMove]
        (uci::arguments_t args) {
            const auto readTime = [&args](const char* key) -> std::int64_t {
                const auto it = args.find(key);
                if (it == args.end()) return 0;
                try {
                    return std::max<std::int64_t>(0, std::stoll(it->second));
                } catch (...) {
                    return 0;
                }
            };
            const std::int64_t white_time = readTime("wtime");
            const std::int64_t black_time = readTime("btime");
            const std::int64_t white_inc = readTime("winc");
            const std::int64_t black_inc = readTime("binc");
            const std::int64_t moves_to_go = readTime("movestogo");
            const std::int64_t remaining = whiteToMove ? white_time : black_time;
            const std::int64_t increment = whiteToMove ? white_inc : black_inc;
            const SearchTimeLimits limits = MakeSearchTimeLimits(
                remaining, increment, moves_to_go);

            std::cerr << "[UCI go] wtime=" << white_time
                      << " btime=" << black_time
                      << " winc=" << white_inc
                      << " binc=" << black_inc
                      << " movestogo=" << moves_to_go
                      << " side=" << (whiteToMove ? "white" : "black") << '\n';
            Tree.tree.nodes[0].position = CURRENT_POSITION.board;
            RunPipeline(Tree, 0, PawnMemorandum, whiteToMove,
                        Context, WorkingFFT, limits);
            ReportGATv2Stats(Context, true);
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
