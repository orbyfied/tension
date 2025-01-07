#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <io.h>
#include <fcntl.h>

#include "board.hh"
#include "debug.hh"
#include "search.hh"

using namespace tc;

int perft(Board* board, int depth) {
    if (depth == 0) {
        return 0;
    }

    int count = 0;
    MoveList<NoOrderMoveOrderer, 128, false> moveList;
    std::cout << "a" << std::endl;
    // gen_pseudo_legal_moves(board, board->whiteTurn, &moveList);
    count += moveList.count;
    
    // for each node, perft more moves
    for (int i = 0; i < moveList.count; i++) {
        ExtMove<true> extMove(moveList.moves[i]);
        board->make_move_unchecked<true>(&extMove);
        count += perft(board, depth - 1);
        board->unmake_move_unchecked<true>(&extMove);
    }

    return depth;
}

int main() {
    std::setlocale(LC_CTYPE, ".UTF-8");

    struct timeval timev;
    gettimeofday(&timev, NULL);
    srand(timev.tv_sec ^ timev.tv_usec);

    Board board;

    /* =============================================================== */

    board.set_piece<true>(fC | r3, BLACK_PIECE | PAWN);   // add pawn before bishop, info on this pawn
    board.set_piece<true>(fF | r6, WHITE_PIECE | BISHOP); // add bishop
    board.set_piece<true>(fF | r5, BLACK_PIECE | PAWN);   // add second pawn, bishop attack must be blocked
    board.set_piece<true>(fC | r5, WHITE_PIECE | ROOK);   // add rook, has info on both pawns
    board.set_piece<true>(fC | r7, BLACK_PIECE | PAWN);   // add third pawn, bishop and rook attacks must be blocked

    // debug bitboards
    std::ostringstream oss;
    oss << "\n";
    BitboardToStrOptions strOptions;
    strOptions.highlightChars[fC | r7] = '3';
    strOptions.highlightChars[fF | r5] = '2';
    strOptions.highlightChars[fC | r3] = '1';
    debug_tostr_bitboard(oss, BITBOARD_FILE_RANGE_MASK(1, 5), strOptions);
    oss << "\n\n";
    debug_tostr_board(oss, board, { });
    std::cout << oss.str();

    // Handled In Movegen:
    // x castling
    // x checks
    // x rook movegen    } queen movegen
    // x bishop movegen  } 
    
    // Handled During Search:
    // - pins
    // - check blocking
    // - check for attacks on castling squares (maybe attacked squares bb usable for checks asw?)
}