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
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

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

inline int ParseUciSquare(std::string_view square)
{
    if (square.size() != 2 || square[0] < 'a' || square[0] > 'h' ||
        square[1] < '1' || square[1] > '8')
        return -1;
    return ('8' - square[1]) * 8 + (square[0] - 'a');
}

inline bool MatchesUciMove(const Move& move, std::string_view uciMove)
{
    if (uciMove.size() != 4 && uciMove.size() != 5)
        return false;
    if (ParseUciSquare(uciMove.substr(0, 2)) != move.From ||
        ParseUciSquare(uciMove.substr(2, 2)) != move.To)
        return false;

    if (!IsPromotion(move.Flags))
        return uciMove.size() == 4;

    if (uciMove.size() != 5)
        return false;
    constexpr std::string_view promotionLetters = "qrbn";
    const std::size_t promotion = (move.Flags - PROMO_QUEEN_QUIET) & 3u;
    return uciMove[4] == promotionLetters[promotion];
}

template<bool White>
inline Move ResolveUciMoveForSide(
    GameState& state,
    std::string_view uciMove,
    const PawnTables& pawnTables
)
{
    if constexpr (White) {
        MoveListWhite moves{};
        int count = 0;
        AllWhiteLegalMoves(moves, count, state, pawnTables);
        for (int i = 0; i < count; ++i)
            if (MatchesUciMove(moves.WhiteMoves[i], uciMove))
                return moves.WhiteMoves[i];
    } else {
        MoveListBlack moves{};
        int count = 0;
        AllBlackLegalMoves(moves, count, state, pawnTables);
        for (int i = 0; i < count; ++i)
            if (MatchesUciMove(moves.BlackMoves[i], uciMove))
                return moves.BlackMoves[i];
    }

    throw std::invalid_argument("Illegal or unrecognized UCI move");
}

inline Move ResolveUciMove(
    GameState& state,
    bool whiteToMove,
    std::string_view uciMove,
    const PawnTables& pawnTables
)
{
    return whiteToMove
        ? ResolveUciMoveForSide<true>(state, uciMove, pawnTables)
        : ResolveUciMoveForSide<false>(state, uciMove, pawnTables);
}

template<bool White>
inline void ApplyGameMoveForSide(
    GameState& state,
    FFTWorking& workingFFT,
    const Move& move,
    bool& whiteToMove,
    unsigned& halfmoveClock,
    unsigned& fullmoveNumber
)
{
    const Board before = state.board;
    const BB from = BB(1) << move.From;
    const BB to = BB(1) << move.To;
    const bool pawnMove = ((White ? WhitePawns(before) : BlackPawns(before)) & from) != 0;
    const bool capture = (state.Occupancy & to) != 0 || move.Flags == EN_PASSANT;

    MakeMove<White>(state, move);
    ApplyFftDelta(workingFFT, MakeFftDelta<4>(before, state.board));

    halfmoveClock = (pawnMove || capture) ? 0 : halfmoveClock + 1;
    if constexpr (!White)
        ++fullmoveNumber;
    whiteToMove = !whiteToMove;
}

inline void ApplyGameMove(
    GameState& state,
    FFTWorking& workingFFT,
    const Move& move,
    bool& whiteToMove,
    unsigned& halfmoveClock,
    unsigned& fullmoveNumber
)
{
    if (whiteToMove)
        ApplyGameMoveForSide<true>(state, workingFFT, move, whiteToMove,
                                   halfmoveClock, fullmoveNumber);
    else
        ApplyGameMoveForSide<false>(state, workingFFT, move, whiteToMove,
                                    halfmoveClock, fullmoveNumber);
}

struct EngineIdentity
{
    bool known = false;
    bool white = true;

    void Reset() noexcept { known = false; }

    bool ObserveFirstTurn(bool whiteToMove) noexcept
    {
        if (known)
            return false;
        known = true;
        white = whiteToMove;
        return true;
    }
};

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
   



    
    Arena gatv2Arena(8 * 1024 * 1024);//Alloated 8MB ON EMBEDDINGS 

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
    unsigned halfmoveClock = 0;
    unsigned fullmoveNumber = 1;
    EngineIdentity engineIdentity;

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
        uci::event::UCINEWGAME,
        [&engineIdentity](uci::arguments_t) {
            engineIdentity.Reset();
        }
    );

    listener.addListener(
        uci::event::POSITION,
        [&Tree, &WorkingFFT, &rootFFT, &whiteToMove, &halfmoveClock,
         &fullmoveNumber, &PawnMemorandum](uci::arguments_t args) {
            try {
                GameState nextPosition{Board{}};
                alignas(64) FFTWorking nextFFT{};
                std::memcpy(&nextFFT, &rootFFT, sizeof(FFTWorking));
                bool nextWhiteToMove = true;
                unsigned nextHalfmoveClock = 0;
                unsigned nextFullmoveNumber = 1;

                const auto fenIt = args.find("fen");
                if (fenIt != args.end()) {
                    const std::string_view fenString = fenIt->second;
                    const auto parsedPosition = FENUtility::ParseFEN(fenString);
                    nextWhiteToMove = parsedPosition.whiteToMove;
                    nextHalfmoveClock = parsedPosition.halfmoveClock;
                    nextFullmoveNumber = parsedPosition.fullmoveNumber;

                    U8 castlingRights = 0;
                    if (parsedPosition.whiteKingSideCastle)
                        castlingRights |= WHITE_KINGSIDE_RIGHT;
                    if (parsedPosition.whiteQueenSideCastle)
                        castlingRights |= WHITE_QUEENSIDE_RIGHT;
                    if (parsedPosition.blackKingSideCastle)
                        castlingRights |= BLACK_KINGSIDE_RIGHT;
                    if (parsedPosition.blackQueenSideCastle)
                        castlingRights |= BLACK_QUEENSIDE_RIGHT;
                    const BB enPassantTarget = parsedPosition.enPassantSquare >= 0
                        ? BB(1) << parsedPosition.enPassantSquare : 0;
                    nextPosition = GameState(parsedPosition.board, castlingRights,
                                             enPassantTarget);

                    if (FENUtility::IsInitialPosition(fenString)) {
                        std::cerr << "[FEN] Using initial position\n";
                    } else {
                        std::cerr << "[FEN] Loading custom position: "
                                  << fenString << '\n';
                        FENUtility::IrregularScript(Tree, nextFFT, fenString);
                        std::cerr << "[FEN] Custom position loaded\n";
                    }
                } else if (args.contains("startpos")) {
                    const auto parsedPosition = FENUtility::ParseFEN(INITIAL_FEN);
                    nextPosition = GameState(parsedPosition.board);
                    nextWhiteToMove = parsedPosition.whiteToMove;
                } else {
                    return;
                }

                const auto movesIt = args.find("moves");
                if (movesIt != args.end()) {
                    std::istringstream moves(movesIt->second);
                    std::string uciMove;
                    while (moves >> uciMove) {
                        const Move move = ResolveUciMove(
                            nextPosition, nextWhiteToMove, uciMove, PawnMemorandum);
                        ApplyGameMove(nextPosition, nextFFT, move,
                                      nextWhiteToMove, nextHalfmoveClock,
                                      nextFullmoveNumber);
                    }
                }

                CURRENT_POSITION = nextPosition;
                WorkingFFT = nextFFT;
                whiteToMove = nextWhiteToMove;
                halfmoveClock = nextHalfmoveClock;
                fullmoveNumber = nextFullmoveNumber;
                Tree.tree.nodes[0].position = CURRENT_POSITION.board;
            } catch (const std::exception& e) {
                std::cerr << "[POSITION ERROR] " << e.what() << '\n';
            }
        }
    );

    listener.addListener(
        uci::event::GO,
        [&Tree, &WorkingFFT, &Context, &PawnMemorandum, &whiteToMove,
         &halfmoveClock, &fullmoveNumber, &engineIdentity]
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
            if (engineIdentity.ObserveFirstTurn(whiteToMove))
                std::cerr << "[ENGINE] playing as "
                          << (engineIdentity.white ? "white" : "black")
                          << '\n';
            Tree.tree.nodes[0].position = CURRENT_POSITION.board;
            const PipelineResult result = RunPipeline(
                Tree, 0, PawnMemorandum, whiteToMove,
                Context, WorkingFFT, limits);
            const float valueForEngine =
                engineIdentity.white == whiteToMove
                ? result.valueForRootSide : -result.valueForRootSide;
            std::cerr << "[ENGINE VALUE] " << valueForEngine << '\n';
            if (result.hasMove)
                ApplyGameMove(CURRENT_POSITION, WorkingFFT, result.bestMove,
                              whiteToMove, halfmoveClock, fullmoveNumber);
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
