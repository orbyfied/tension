#include "debug.h"

void debug_tostr_bitboard(std::ostringstream& oss, u64 bb) {
    const std::string rowSep = "   +---+---+---+---+---+---+---+---+";
    oss <<                     "     H   G   F   E   D   C   B   A" << std::endl;
    oss << rowSep << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        oss << " " << (rank + 1) << " |";
        for (int file = 7; file >= 0; file--) {
            bool b = (bool)((bb >> INDEX(file, rank)) & 0x1);
            oss << (b ? GRNB : REDB) << " " << (int)b << " " << reset << "|";
        }

        oss << "\n" << rowSep << "\n";
    }

    oss <<                     "     H   G   F   E   D   C   B   A" << std::endl;
}

void debug_tostr_move(std::ostringstream& oss, Move move) {
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