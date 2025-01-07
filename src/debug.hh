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
void debug_tostr_board(std::ostringstream& oss, Board& b, BoardToStrOptions options);

/// @brief Write a string containing all move info
void debug_tostr_move(std::ostringstream& oss, Board& b, Move move);

}