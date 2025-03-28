#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <io.h>
#include <fcntl.h>

#include "uci.hh"
#include "../vendor/popl/include/popl.hpp"

#include "logging.hh"
#include "debug.hh"
#include "board.hh"
#include "search.hh"
#include "basiceval.hh"

using namespace popl;
using namespace tc;

int main(int argc, char** argv) {
    std::ostringstream oss;
    log<DEBUG>(P, "Entered main function, seeding random and parsing options");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand((tv.tv_sec) * 1000ULL + tv.tv_usec);

    /*  ========== OPTION PARSING ========== */
    OptionParser opt("Tension dogshit chess bot - by orbyfied 2025");

    auto doUci = opt.add<Switch>("u", "uci", "listen on UCI");

    opt.parse(argc, argv);

    /* ========== UCI and other interfaces ========== */

    if (true || doUci->value()) {
        UCIState state;
        uci_listen(&state, &opt);
        return 0;
    }

    /* =========================== */

    Board b;
    std::cout << "Enter FEN position: ";
    std::string fen;
    std::getline(std::cin, fen); // get input until enter key is pressed
    if (!fen.empty()) {
        b.load_fen(fen.c_str());
    } else {
        b.load_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQkq - ");
    }

    debug_tostr_board(std::cout, b);
    debug_tostr_bitboard(std::cout, b.pieces(PAWN), { });

    if (false) {
        constexpr Color turn = BLACK;
        constexpr bool iterLegal = true;
        constexpr bool sortMoves = false;
        // constexpr int iters = 5'000'000;
        constexpr int iters = 1;

        int totalPseudoLegal = 0;
        int totalLegal = 0;
        Time t01 = log<DEBUG>(P, "Starting movegen benchmark iters(%i)", iters);
        for (int i = iters; i >= 0; i--) {
            MoveList<BasicScoreMoveOrderer, MAX_MOVES> ml;
            gen_all_moves<decltype(ml), movegenAllPL, turn>(&b, &ml);
            if constexpr (sortMoves)
                ml.sort_moves<turn>(&b);
            totalPseudoLegal += ml.count;

            if constexpr (iterLegal) {
                for (int j = ml.count - 1; j >= 0; j--) {
                    Move move = ml.get_move(j);

                    ExtMove<true> xMove(move);
                    b.make_move_unchecked<turn, true>(&xMove);

                    if (b.is_in_check<turn>()) {
                        b.unmake_move_unchecked<turn, true>(&xMove);
                        continue;
                    }

                    totalLegal++;
                    b.unmake_move_unchecked<turn, true>(&xMove);
                }
            }
        }

        Time t = get_microseconds_since_start();
        log<DEBUG>(P, "Finished movegen benchmark in %f sec (%llu us) pseudo legal generated: %i, legal: %i", to_seconds(t - t01), t - t01, totalPseudoLegal, totalLegal);
        
        debug_tostr_board(std::cout, b);
    }
    

    // if (true) { return 0; }

    MoveList<BasicScoreMoveOrderer, 1024> moveList;
    if (b.turn) { gen_all_moves<decltype(moveList), movegenCapturesPL, WHITE>(&b, &moveList); moveList.sort_moves<WHITE>(&b); }
    else { gen_all_moves<decltype(moveList), movegenCapturesPL, BLACK>(&b, &moveList); moveList.sort_moves<BLACK>(&b); }

    oss << "\n\nGenerated " << moveList.count << " pseudo-legal moves for " << (b.turn ? "white" : "black");
    for (int i = moveList.count - 1; i >= 0; i--) {
        Move move = moveList.get_move(i);
        if (move.null()) continue;

        oss << "\n[" << i << "] ";
        debug_tostr_move(oss, b, move);
        oss << "  -  " << moveList.get_score(i);
    }

    std::cout << oss.str() << "\n\n";
    oss.str("");

    if (true) {
        constexpr bool searchMetrics = true;
        constexpr static StaticSearchOptions SearchOptions { .useTranspositionTable = true, .debugMetrics = searchMetrics };
        TranspositionTable tt;
        tt.alloc(18);
        BasicStaticEvaluator bse;
        SearchManager sm;
        SearchState<SearchOptions, BasicStaticEvaluator> searchState { .board = &b, .leafEval = &bse, .transpositionTable = &tt };
        ThreadSearchState<SearchOptions> tss { };
        constexpr int maxPrimaryDepth = 8;
        std::cout << "\n";

        gettimeofday(&tv, NULL);
        double t0 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

        for (int depth = 1; depth <= maxPrimaryDepth; depth++) {
            gettimeofday(&tv, NULL);
            double t1 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

            searchState.metrics = SearchMetrics { };
            searchState.maxPrimaryDepth = depth;

            i32 eval;
            if (b.turn == WHITE) eval = search_sync<SearchOptions, BasicStaticEvaluator, WHITE>(&searchState, &tss, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth);
            else eval = search_sync<SearchOptions, BasicStaticEvaluator, BLACK>(&searchState, &tss, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth);
            Move move = searchState.stack.first()->move;

            gettimeofday(&tv, NULL);
            double t2 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

            // debug_tostr_board(std::cout, b);
            // debug_tostr_bitboard(std::cout, b.allPiecesPerColor[BLACK], { });

            std::cout << "Completed depth " << depth << " search for " << (b.turn ? "WHITE" : "BLACK") << " to move, best move [";
            debug_tostr_move(std::cout, b, move);
            std::cout << "]  -  ";
            write_eval(std::cout, SIGN_OF_COLOR(b.turn) * eval);
            std::cout << "\n";
            std::cout << " Time: " << (t2 - t1) << "ms, total: " << (t2 - t0) << "ms\n";
            if (SearchOptions.maintainPV) {
                std::cout << " PV: { ";
                std::cout << " }\n";
            }

            if (searchMetrics) {
                debug_tostr_search_metrics(std::cout, &searchState);
                std::cout << "\n";
            }
        }
    }

    char hl[64];
    float blockersDensity = 0.2f;

    if (false) {
        // u8 rookIndex = rand() % 64;
        u8 rookIndex = 4;
        u64 blockers = bitwise_random_64(blockersDensity);
        blockers &= ~(1ULL << rookIndex);

        std::ostringstream oss;
        oss << "Rook index: " << (int)rookIndex << " (x" << (int)FILE(rookIndex) << " y" << (int)RANK(rookIndex) << ")\n\n";
        oss << "Blockers with rook highlighted: " << "\n";
        memset(&hl, 0, 64);
        hl[rookIndex] = '.';
        debug_tostr_bitboard(oss, blockers, { .highlightChars = (char*)&hl });

        oss << "\n\nAttack bitboard: \n";
        u64 bc = blockers;
        while (bc) {
            hl[_pop_lsb(bc)] = 219;
        }
        Bitboard bb = tc::lookup::magic::rook_attack_bb(rookIndex, blockers);
        debug_tostr_bitboard(oss, bb, { .highlightChars = (char*)&hl });

        std::cout << oss.str() << std::endl;
    }

    if (false) {
        u8 bishopIndex = rand() % 64;
        u64 blockers = bitwise_random_64(blockersDensity);
        blockers &= ~(1ULL << bishopIndex);

        std::ostringstream oss;
        oss << "Bishop index: " << (int)bishopIndex << " (x" << (int)FILE(bishopIndex) << " y" << (int)RANK(bishopIndex) << ")\n\n";
        oss << "Blockers with bishop highlighted: " << "\n";
        memset(&hl, 0, 64);
        hl[bishopIndex] = '.';
        debug_tostr_bitboard(oss, blockers, { .highlightChars = (char*)&hl });

        oss << "\n\nAttack bitboard: \n";
        u64 bc = blockers;
        while (bc) {
            hl[_pop_lsb(bc)] = 219;
        }
        Bitboard bb = tc::lookup::magic::bishop_attack_bb(bishopIndex, blockers);
        debug_tostr_bitboard(oss, bb, { .highlightChars = (char*)&hl });

        std::cout << "\n\n" << oss.str() << std::endl;
    }
}