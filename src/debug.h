#pragma once

#include <string>
#include <sstream>
#include "board.h"
#include "util/debug.h"

struct BoardToStrOptions {
    Move highlightedMove = NULL_MOVE;
};

/// @brief Visualize the bitboard in a string using a board layout and ANSI color codes
void debug_tostr_bitboard(std::ostringstream& oss, u64 bb);

/// @brief Visualize the given board using a board layout and ANSI color codes, also prints information such as castling
template<StaticBoardOptions const& _BoardOptions>
void debug_tostr_board(std::ostringstream& oss, Board<_BoardOptions>& b, BoardToStrOptions options) {
    // collect additional info
    u64 enPassantTargets = b.enPassantTargetBBs[0] | b.enPassantTargetBBs[1];

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
            oss << "  Material: W " << b.count_material(1) << " B " << b.count_material(0);
            break;
        }

        oss << "\n" << rowSep << "\n";
    }

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl << std::endl;
    oss << "   * in check? W: " << b.is_in_check(true) << " B: " << b.is_in_check(false) << std::endl;
    oss << "   * castling W: 0b" << std::bitset<4>(b.castlingStatus[1]) << " " << (b.castlingStatus[1] & CASTLED_L ? "castled Q" : (b.castlingStatus[1] & CASTLED_R ? "castled K" : "not castled")) 
        << ", rights: " << (b.castlingStatus[1] & CAN_CASTLE_L ? "Q" : "") << (b.castlingStatus[1] & CAN_CASTLE_R ? "K" : "") << std::endl;
    oss << "   * castling B: 0b" << std::bitset<4>(b.castlingStatus[1]) << " " << (b.castlingStatus[0] & CASTLED_L ? "castled Q" : (b.castlingStatus[0] & CASTLED_R ? "castled K" : "not castled")) 
        << ", rights: " << (b.castlingStatus[0] & CAN_CASTLE_L ? "Q" : "") << (b.castlingStatus[0] & CAN_CASTLE_R ? "K" : "") << std::endl;
}

/// @brief Write a string containing all move info
void debug_tostr_move(std::ostringstream& oss, Move move);