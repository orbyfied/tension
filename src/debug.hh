#pragma once

#include <string>
#include <sstream>
#include "board.hh"
#include "util/debug.hh"

namespace tc {

struct BoardToStrOptions {
    Move highlightedMove = NULL_MOVE;
};

/// @brief Visualize the given board using a board layout and ANSI color codes, also prints information such as castling
void debug_tostr_board(std::ostream& oss, Board& b, BoardToStrOptions options);
void debug_tostr_board(std::ostream& oss, Board& b);

/// @brief Write a string containing all move info
void debug_tostr_move(std::ostream& oss, Board& b, Move move);
void debug_tostr_xmove(std::ostream& oss, Board& b, ExtMove<true>* xMove);
void debug_tostr_move(std::ostream& oss, Move move);

inline void write_repeated(std::ostream& oss, int depth, const char* c) {
    for (int i = 0; i < depth; i++) {
        oss << c;
    }
}

}