#include "debug.hh"

using namespace tc;

void tc::debug_tostr_bitboard(std::ostringstream& oss, u64 bb, BitboardToStrOptions options) {
    const std::string rowSep = "   +---+---+---+---+---+---+---+---+";
    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
    oss << rowSep << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        oss << " " << (rank + 1) << " |";
        for (int file = 0; file < 8; file++) {
            u8 index = INDEX(file, rank);
            bool b = (bool)((bb >> index) & 0x1);

            char c = options.highlightChars[index];
            bool highlighted = c != 0;
            if (c == 0) {
                c = '0' + b;
            }

            oss << (b ? GRNB : REDB) << " " << c << " " << reset << "|";
        }

        oss << "\n" << rowSep << "\n";
    }

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
}

void tc::debug_tostr_move(std::ostringstream& oss, Board& b, Move move) {
    oss << move.moved_piece(&b) << " ";
    oss << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
    oss << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst));
    if (move.captured_piece(&b) != NULL_PIECE) oss << " x" << pieceToChar(move.captured_piece(&b));
    if (move.is_promotion()) oss << " =" << typeToCharLowercase[move.promotion_piece()];
    if (move.is_en_passant()) oss << " ep";
    if (move.is_castle_left()) oss << " O-O-O";
    if (move.is_castle_right()) oss << " O-O";
}