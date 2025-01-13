#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <io.h>
#include <fcntl.h>

#include "debug.hh"
#include "board.hh"
#include "search.hh"
#include "basiceval.hh"

using namespace tc;

int main() {
    std::ostringstream oss;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand((tv.tv_sec) * 1000ULL + tv.tv_usec);

    Board b;
    // b.load_fen("startpos");
    b.load_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    // b.load_fen("k7/6b1/8/1qb1K1Br/8/4N3/8/4r3 w - - 0 1");
    
    debug_tostr_board(std::cout, b, { });
    // debug_tostr_bitboard(std::cout, b.attacks_by(WHITE), { });

    // MoveList<BasicScoreMoveOrderer, 1024, false> moveList;
    // gen_all_moves<decltype(moveList), defaultFullyLegalMovegenOptions>(&b, &moveList, b.turn);

    // oss << "\n\nGenerated " << moveList.count << " pseudo-legal moves for " << (b.turn ? "white" : "black");
    // for (int i = moveList.count - 1; i >= 0; i--) {
    //     Move move = moveList.moves[i];
    //     if (move.null()) continue;

    //     oss << "\n[" << i << "] ";
    //     debug_tostr_move(oss, b, move);
    //     oss << "  -  " << moveList.scores[i];
    // }

    std::cout << oss.str() << "\n\n";
    oss.str("");

    if (false) {
        return 0;
    }

    constexpr bool searchMetrics = false;
    constexpr static StaticSearchOptions SearchOptions { .useTranspositionTable = false, .useMoveEvalTable = true, .debugMetrics = searchMetrics };
    TranspositionTable tt;
    tt.alloc(1024 * 1024 * 128);
    MoveEvalTable met;
    met.alloc(64 * 64 * 8);
    BasicStaticEvaluator bse;
    SearchManager sm;
    sm.board = &b;
    sm.leafEval = &bse;
    SearchState<SearchOptions> searchState { .moveEvalTable = &met, .transpositionTable = &tt };
    ThreadSearchState<SearchOptions> tss { };
    SearchEvalResult res;
    constexpr int maxDepth = 8;
    std::cout << "\n";

    gettimeofday(&tv, NULL);
    double t0 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

    for (int depth = 2; depth <= maxDepth; depth++) {
        gettimeofday(&tv, NULL);
        double t1 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

        if (b.turn == WHITE) res = sm.search_fixed_internal_sync<SearchOptions, WHITE, false>(&searchState, &tss, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth, depth);
        else res = sm.search_fixed_internal_sync<SearchOptions, BLACK, false>(&searchState, &tss, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth, depth);
        
        gettimeofday(&tv, NULL);
        double t2 = (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

        std::cout << "Completed depth " << depth << " search for " << (b.turn ? "WHITE" : "BLACK") << " to move, best move [";
        debug_tostr_move(std::cout, b, res.move);
        std::cout << "]  -  ";
        write_eval(std::cout, SIGN_OF_COLOR(b.turn) * res.eval);
        std::cout << "\n";
        std::cout << " Time: " << (t2 - t1) << "ms, total: " << (t2 - t0) << "ms\n";
        if (searchMetrics) {
            debug_tostr_search_metrics(std::cout, &searchState);
            std::cout << "\n";
        }
    }

    // char hl[64];
    // float blockersDensity = 0.2f;

    // if (true) {
    //     // u8 rookIndex = rand() % 64;
    //     u8 rookIndex = 4;
    //     u64 blockers = bitwise_random_64(blockersDensity);
    //     blockers &= ~(1ULL << rookIndex);

    //     std::ostringstream oss;
    //     oss << "Rook index: " << (int)rookIndex << " (x" << (int)FILE(rookIndex) << " y" << (int)RANK(rookIndex) << ")\n\n";
    //     oss << "Blockers with rook highlighted: " << "\n";
    //     memset(&hl, 0, 64);
    //     hl[rookIndex] = '.';
    //     debug_tostr_bitboard(oss, blockers, { .highlightChars = (char*)&hl });

    //     oss << "\n\nAttack bitboard: \n";
    //     u64 bc = blockers;
    //     while (bc) {
    //         hl[_pop_lsb(bc)] = 219;
    //     }
    //     Bitboard bb = tc::lookup::magic::rook_attack_bb(rookIndex, blockers);
    //     debug_tostr_bitboard(oss, bb, { .highlightChars = (char*)&hl });

    //     std::cout << oss.str() << std::endl;
    // }

    // if (false) {
    //     u8 bishopIndex = rand() % 64;
    //     u64 blockers = bitwise_random_64(blockersDensity);
    //     blockers &= ~(1ULL << bishopIndex);

    //     std::ostringstream oss;
    //     oss << "Bishop index: " << (int)bishopIndex << " (x" << (int)FILE(bishopIndex) << " y" << (int)RANK(bishopIndex) << ")\n\n";
    //     oss << "Blockers with bishop highlighted: " << "\n";
    //     memset(&hl, 0, 64);
    //     hl[bishopIndex] = '.';
    //     debug_tostr_bitboard(oss, blockers, { .highlightChars = (char*)&hl });

    //     oss << "\n\nAttack bitboard: \n";
    //     u64 bc = blockers;
    //     while (bc) {
    //         hl[_pop_lsb(bc)] = 219;
    //     }
    //     Bitboard bb = tc::lookup::magic::bishop_attack_bb(bishopIndex, blockers);
    //     debug_tostr_bitboard(oss, bb, { .highlightChars = (char*)&hl });

    //     std::cout << "\n\n" << oss.str() << std::endl;
    // }
}