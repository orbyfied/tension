#include "debug.hh"

using namespace tc;

void tc::debug_tostr_bitboard(std::ostringstream& oss, u64 bb, BitboardToStrOptions options) {
    const std::string rowSep = "   +---+---+---+---+---+---+---+---+";
    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
    oss << rowSep << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        oss << " " << (rank + 1) << " |";
        for (int file = 0; file < 8; file++) {
            Sq index = INDEX(file, rank);
            bool b = (bool)((bb >> index) & 0x1);

            char c = 0;
            if (options.highlightChars != nullptr) {
                c = options.highlightChars[index];
                bool highlighted = c != 0;
            }

            if (c == 0) {
                c = '0' + b;
            }

            oss << (b ? GRNB : REDB) << " " << c << " " << reset << "|";
        }

        oss << "\n" << rowSep << "\n";
    }

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
}

void tc::debug_tostr_board(std::ostringstream& oss, Board& b, BoardToStrOptions options) {
    // collect additional info
    u64 enPassantTarget = b.volatile_state()->enPassantTarget;

    const std::string rowSep = "   +---+---+---+---+---+---+---+---+";
    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
    oss << rowSep << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        oss << " " << (rank + 1) << " |";
        for (int file = 0; file < 8; file++) {
            Sq index = INDEX(file, rank);
            Piece p = b.pieceArray[index];

            // collect additional info
            bool enPassant = index == enPassantTarget;
            bool visualize = enPassant;

            Color color = IS_WHITE_PIECE(p);

            oss << (color ? BLK : WHT) << (p == NULL_PIECE ? reset : (color ? WHTB : BLKB));

            if (enPassant) {
                oss << BLUB;
            }

            if (!options.highlightedMove.null() && options.highlightedMove.src == index) {
                oss << YELHB;
            }

            if (!options.highlightedMove.null() && options.highlightedMove.dst == index) {
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

    VolatileBoardState& state = *b.volatile_state();

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl << std::endl;
    oss << "   * in check? W: " << b.template is_in_check<WHITE>() << " B: " << b.template is_in_check<BLACK>() << std::endl;
    oss << "   * castling W: 0b" << std::bitset<4>(state.castlingStatus[1]) << " " << (state.castlingStatus[1] & CASTLED_L ? "castled Q" : (state.castlingStatus[1] & CASTLED_R ? "castled K" : "not castled")) 
        << ", rights: " << (state.castlingStatus[1] & CAN_CASTLE_L ? "Q" : "") << (state.castlingStatus[1] & CAN_CASTLE_R ? "K" : "") << std::endl;
    oss << "   * castling B: 0b" << std::bitset<4>(state.castlingStatus[1]) << " " << (state.castlingStatus[0] & CASTLED_L ? "castled Q" : (state.castlingStatus[0] & CASTLED_R ? "castled K" : "not castled")) 
        << ", rights: " << (state.castlingStatus[0] & CAN_CASTLE_L ? "Q" : "") << (state.castlingStatus[0] & CAN_CASTLE_R ? "K" : "") << std::endl;
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