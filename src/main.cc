#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <io.h>
#include <fcntl.h>

#include "board.hh"
#include "debug.hh"
#include "engine.hh"

using namespace tc;

constexpr extern StaticBoardOptions options { true };

template<StaticBoardOptions const& _BoardOptions>
int perft(Board<_BoardOptions>* board, int depth) {
    if (depth == 0) {
        return 0;
    }

    int count = 0;
    MoveList<NoOrderMoveOrderer, 128> moveList;
    std::cout << "a" << std::endl;
    // gen_pseudo_legal_moves(board, board->whiteTurn, &moveList);
    count += moveList.count;
    
    // for each node, perft more moves
    for (int i = 0; i < moveList.count; i++) {
        ExtMove<true> extMove(moveList.moves[i]);
        board->make_move_unchecked(&extMove);
        count += perft(board, depth - 1);
        board->unmake_move_unchecked(&extMove);
    }

    return depth;
}

int main() {
    std::setlocale(LC_CTYPE, ".UTF-8");

    Board<options> board;

    board.set_piece<true>(fC | r3, BLACK | PAWN);   // add pawn before bishop, info on this pawn
    board.set_piece<true>(fF | r6, WHITE | BISHOP); // add bishop
    board.set_piece<true>(fF | r5, BLACK | PAWN);   // add second pawn, bishop attack must be blocked
    board.set_piece<true>(fC | r5, WHITE | ROOK);   // add rook, has info on both pawns
    board.set_piece<true>(fC | r7, BLACK | PAWN);   // add third pawn, bishop and rook attacks must be blocked

    // debug bitboards
    std::ostringstream oss;
    oss << "\n";
    BitboardToStrOptions strOptions;
    strOptions.highlightChars[fC | r7] = '-';
    strOptions.highlightChars[fF | r5] = '+';
    strOptions.highlightChars[fC | r3] = '_';
    debug_tostr_bitboard(oss, board.attackBBsPerColor[1], strOptions);
    oss << "\n\n";
    debug_tostr_board(oss, board, { });
    std::cout << oss.str();

    // board.load_fen("8/8/8/8/8/8/8/R3K1PR w - - 0 1");
    // board.set_piece<true>(fC | r3, WHITE | KNIGHT);
    // std::cout << "loaded FEN" << std::endl;

    // std::ostringstream oss1;
    // debug_tostr_board(oss1, board, { });
    // debug_tostr_bitboard(oss1, board.allPiecesBB);
    // std::cout << oss1.str() << std::endl;

    // ExtMove<true> extMove({ .piece = WHITE | PAWN, .src = fE | r2, .dst = fE | r4, .isDoublePush = true });
    // board.make_move_unchecked(&extMove);

    // std::cout << perft<options>(&board, 4) << std::endl;

    // Handled In Movegen:
    // x castling
    // x checks
    // x rook movegen    } queen movegen
    // x bishop movegen  } 
    
    // Handled During Search:
    // - pins
    // - check blocking
    // - check for attacks on castling squares (maybe attacked squares bb usable for checks asw?)

    struct timeval timev;
    gettimeofday(&timev, NULL);
    srand(timev.tv_sec ^ timev.tv_usec);

    // board.whiteTurn = true;
    // int turnsToPlay = 1;
    // for (int i = 0; i < turnsToPlay; i++) {
    //     MoveList<200> moveList;
    //     board.gen_legal_moves(board.whiteTurn, &moveList);

    //     // pick random move
    //     Move move = NULL_MOVE;
    //     for (int i = 0; i < moveList.count; i++) {
    //         if (moveList.moves[i].enPassant) {
    //             move = moveList.moves[i];
    //             break;
    //         }
    //     }
        
    //     if (move.isNull()) { 
    //         int idx = (rand() % (moveList.count));
    //         move = moveList.moves[idx];
    //     }

    //     board.make_move_unchecked(move);

    //     std::ostringstream oss;
    //     debug_tostr_move(oss, move);
    //     std::cout << "[*] Turn " << i << " | " << (board.whiteTurn ? "WHITE" : "BLACK") << " played: " << oss.str() << "\n\n";  
    //     std::cout.flush();

    //     oss.str("");
    //     oss.clear();
    //     debug_tostr_board(oss, board, { .highlightedMove = move });
    //     std::cout << oss.str() << "\n\n";
    //     std::cout.flush();

    //     board.whiteTurn = !board.whiteTurn;
    // }

    // board.whiteTurn = true;
    // MoveList<200> moveList;
    // gen_pseudo_legal_moves<MoveList<200>, options, Engine<options>::standardMovegenOptions>(&board, board.whiteTurn, &moveList);

    // std::cout << "\ngenerated " << moveList.count << " moves for " << (board.whiteTurn ? "white" : "black") << std::endl;
    // for (int i = 0; i < moveList.count; i++) {
    //     std::ostringstream oss;
    //     debug_tostr_move(oss, moveList.moves[i]);
    //     std::cout << "[" << i << "] " << oss.str() << std::endl;

        // board.make_move_unchecked(moveList.moves[i]);
        // oss.str("");
        // oss.clear();
        // debug_tostr_board(oss, board, { .highlightedMove =  });
        // std::cout << oss.str();
        // board.unmake_move_unchecked(moveList.moves[i]);
    // }
}