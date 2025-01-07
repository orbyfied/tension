#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <io.h>
#include <fcntl.h>

#include "debug.hh"
#include "board.hh"
#include "search.hh"

using namespace tc;

int main() {
    Board b;
    // b.set_piece<true>(fC | r4, WHITE_PIECE | ROOK);
    // b.set_piece<true>(fH | r4, WHITE_PIECE | ROOK);
    // b.set_piece<true>(fD | r3, WHITE_PIECE | PAWN);
    // b.set_piece<true>(fA | r7, WHITE_PIECE | KNIGHT);
    // b.set_piece<true>(fB | r5, BLACK_PIECE | KNIGHT);
    // b.set_piece<true>(fB | r4, BLACK_PIECE | QUEEN);
    // b.set_piece<true>(fE | r1, BLACK_PIECE | PAWN);
    b.set_piece<true>(fA | r2, WHITE_PIECE | PAWN);
    b.set_piece<true>(fC | r3, WHITE_PIECE | PAWN);
    b.set_piece<true>(fB | r2, WHITE_PIECE | PAWN);
    b.set_piece<true>(fB | r3, BLACK_PIECE | PAWN);
    b.set_piece<true>(fH | r7, WHITE_PIECE | PAWN);
    b.set_piece<true>(fD | r8, BLACK_PIECE | PAWN);
    b.set_piece<true>(fE | r7, WHITE_PIECE | PAWN);

    std::ostringstream oss;
    debug_tostr_board(oss, b, { });

    MoveList<BasicScoreMoveOrderer, 1024, false> moveList;
    gen_all_moves<decltype(moveList), defaultFullyLegalMovegenOptions, WHITE>(&b, &moveList);

    oss << "\n\nGenerated " << moveList.count << " moves for white:";
    for (int i = moveList.count - 1; i >= 0; i--) {
        Move move = moveList.moves[i];
        if (move.null()) continue;

        oss << "\n[" << i << "]";
        debug_tostr_move(oss, b, move);
        oss << "  -  " << moveList.scores[i];
    }

    std::cout << oss.str() << std::endl;

    // struct timeval tv;
    // gettimeofday(&tv, NULL);
    // srand((tv.tv_sec) * 1000ULL + tv.tv_usec);

    // char hl[64];
    // float blockersDensity = 0.6f;

    // if (true) {
    //     u8 rookIndex = rand() % 64;
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

    // if (true) {
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