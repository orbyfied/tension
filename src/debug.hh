#pragma once

#include <string>
#include <sstream>
#include "board.hh"
#include "util/debug.hh"

namespace tc {

struct BitboardToStrOptions {
    char highlightChars[64] = { 0 };
};

struct BoardToStrOptions {
    Move highlightedMove = NULL_MOVE;
};

/// @brief Visualize the bitboard in a string using a board layout and ANSI color codes
void debug_tostr_bitboard(std::ostringstream& oss, u64 bb, BitboardToStrOptions options);

/// @brief Visualize the given board using a board layout and ANSI color codes, also prints information such as castling
void debug_tostr_board(std::ostringstream& oss, Board& b, BoardToStrOptions options) {
    // collect additional info
    u64 enPassantTargets = b.current_state()->enPassantTargets[0] | b.current_state()->enPassantTargets[1];

    const std::string rowSep = "   +---+---+---+---+---+---+---+---+";
    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
    oss << rowSep << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        oss << " " << (rank + 1) << " |";
        for (int file = 0; file < 8; file++) {
            u8 index = INDEX(file, rank);
            Piece p = b.pieces[index];

            // collect additional info
            bool enPassant = (enPassantTargets >> index) & 0x1;
            bool visualize = enPassant;

            bool color = IS_WHITE_PIECE(p);

            oss << (color ? BLK : WHT) << (p == NULL_PIECE ? reset : (color ? WHTB : BLKB));

            if (enPassant) {
                oss << BLUB;
            }

            if (!options.highlightedMove.isNull() && options.highlightedMove.src == index) {
                oss << YELHB;
            }

            if (!options.highlightedMove.isNull() && options.highlightedMove.dst == index) {
                oss << YELB;
            }

            if (p == NULL_PIECE) {
                oss << "   " << reset << "|";
                continue;
            }

            oss << " " << pieceToChar(p) << " " << reset << "|";
        }

        // append info to the right
        switch (rank)
        {
        case 3:
            // oss << "  Material: W " << b.template count_material<WHITE>() << " B " << b.template count_material<BLACK>();
            break;
        }

        oss << "\n" << rowSep << "\n";
    }

    BoardState& state = *b.current_state();

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl << std::endl;
    oss << "   * in check? W: " << b.template is_in_check<WHITE>() << " B: " << b.template is_in_check<BLACK>() << std::endl;
    oss << "   * castling W: 0b" << std::bitset<4>(state.castlingStatus[1]) << " " << (state.castlingStatus[1] & CASTLED_L ? "castled Q" : (state.castlingStatus[1] & CASTLED_R ? "castled K" : "not castled")) 
        << ", rights: " << (state.castlingStatus[1] & CAN_CASTLE_L ? "Q" : "") << (state.castlingStatus[1] & CAN_CASTLE_R ? "K" : "") << std::endl;
    oss << "   * castling B: 0b" << std::bitset<4>(state.castlingStatus[1]) << " " << (state.castlingStatus[0] & CASTLED_L ? "castled Q" : (state.castlingStatus[0] & CASTLED_R ? "castled K" : "not castled")) 
        << ", rights: " << (state.castlingStatus[0] & CAN_CASTLE_L ? "Q" : "") << (state.castlingStatus[0] & CAN_CASTLE_R ? "K" : "") << std::endl;
}

/// @brief Write a string containing all move info
void debug_tostr_move(std::ostringstream& oss, Board& b, Move move);

}