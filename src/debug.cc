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

void tc::debug_tostr_move(std::ostringstream& oss, Move move) {
    oss << pieceToChar(move.piece) << " ";
    oss << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
    oss << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst));
    if (move.captured != NULL_PIECE) oss << " x" << pieceToChar(move.captured);
    if (move.isCheck) oss << " #";
    if (move.promotionType != NULL_PIECE_TYPE) oss << " =" << typeToCharLowercase[move.promotionType];
    if (move.enPassant) oss << " ep";
    if ((move.castleOperations & CASTLED_L)) oss << " O-O-O";
    if ((move.castleOperations & CASTLED_R)) oss << " O-O";

    if ((move.castleOperations & CAN_CASTLE_R)) oss << " ~cK";
    if ((move.castleOperations & CAN_CASTLE_L)) oss << " -cQ";
}