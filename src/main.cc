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
    // b.load_fen("8/8/8/8/3p4/8/4P3/8 w - - 0 1");

    debug_tostr_board(std::cout, b, { });

    // MoveList<BasicScoreMoveOrderer, 1024, false> moveList;
    // gen_all_moves<decltype(moveList), defaultFullyLegalMovegenOptions>(&b, &moveList, b.turn);

    // oss << "\n\nGenerated " << moveList.count << " pseudo-legal moves for " << (b.turn ? "white" : "black");
    // for (int i = moveList.count - 1; i >= 0; i--) {
    //     Move move = moveList.moves[i];
    //     if (move.null()) continue;

    //     oss << "\n[" << i << "]";
    //     debug_tostr_move(oss, b, move);
    //     oss << "  -  " << moveList.scores[i];
    // }

    // std::cout << oss.str() << "\n\n";
    // oss.str("");

    constexpr static StaticSearchOptions SearchOptions { .useMoveEvalTable = true, .debugMetrics = true };
    MoveEvalTable met;
    met.alloc(4096);
    BasicStaticEvaluator bse;
    SearchManager sm;
    sm.board = &b;
    sm.leafEval = &bse;
    SearchState<SearchOptions> searchState { .moveEvalTable = &met };
    ThreadSearchState<SearchOptions> tss { };
    int depth = 20;
    SearchEvalResult res = sm.search_fixed_internal_sync<SearchOptions, WHITE, false>(&searchState, &tss, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth, depth);

    oss << "\n[*] Best move: ";
    debug_tostr_move(oss, b, res.move);
    oss << "  -  ";
    write_eval(oss, res.eval);
    ExtMove<true> extMove(res.move);
    b.make_move_unchecked<WHITE, true>(&extMove);
    std::cout << oss.str() << "\n";
    debug_tostr_search_metrics(std::cout, &searchState);

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