#include "debug.hh"

using namespace tc;

void tc::debug_tostr_bitboard(std::ostream& oss, u64 bb, BitboardToStrOptions options) {
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

            oss << (b ? GRNB : REDB) << " " << c << " " << CRESET << "|";
        }

        oss << "\n" << rowSep << "\n";
    }

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl;
}

void tc::debug_tostr_board(std::ostream& oss, Board& b) {
    debug_tostr_board(oss, b, { });
}

void tc::debug_tostr_board(std::ostream& oss, Board& b, BoardToStrOptions options) {
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

            oss << (color ? BLK : WHT) << (p == NULL_PIECE ? CRESET : (color ? WHTB : BLKB));

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
                oss << "   " << CRESET << "|";
                continue;
            }

            oss << " " << pieceToChar(p) << " " << CRESET << "|";
        }

        // append info to the right
        VolatileBoardState& state = *b.volatile_state();
        switch (rank)
        {
        case 6:
            oss << " To move: " << (b.turn ? "WHITE" : "BLACK") << ", ply played: " << b.ply << ", 50MR counter: " << (int)b.volatile_state()->rule50Ply;
            break;
        case 5:
            oss << " In check? W: " << b.template is_in_check<WHITE>() << " B: " << b.template is_in_check<BLACK>();
            break;
        case 4:
            oss << " Castling W: 0b" << std::bitset<4>(state.castlingStatus[1]) << " " << (state.castlingStatus[1] & CASTLED_L ? "castled Q" : (state.castlingStatus[1] & CASTLED_R ? "castled K" : "not castled")) 
                << ", rights: " << (state.castlingStatus[1] & CAN_CASTLE_L ? "Q" : "") << (state.castlingStatus[1] & CAN_CASTLE_R ? "K" : "");
            break;
        case 3:
            oss << " Castling B: 0b" << std::bitset<4>(state.castlingStatus[1]) << " " << (state.castlingStatus[0] & CASTLED_L ? "castled Q" : (state.castlingStatus[0] & CASTLED_R ? "castled K" : "not castled")) 
                << ", rights: " << (state.castlingStatus[0] & CAN_CASTLE_L ? "Q" : "") << (state.castlingStatus[0] & CAN_CASTLE_R ? "K" : "");
            break;
        }

        oss << "\n" << rowSep << "\n";
    }

    oss <<                     "     A   B   C   D   E   F   G   H" << std::endl << std::endl;
}

void tc::debug_tostr_move(std::ostream& oss, Board& b, Move move) {
    if (move.null()) {
        oss << "<NULL MOVE>";
        return;
    }

    oss << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
    oss << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst));
    if (move.is_promotion()) oss << " =" << typeToCharLowercase[move.promotion_piece()];
    if (move.is_en_passant()) oss << " ep";
    if (move.is_castle_left()) oss << " O-O-O";
    if (move.is_castle_right()) oss << " O-O";
}

void tc::debug_tostr_move(std::ostream& oss, Move move) {
    if (move.null()) {
        oss << "<NULL MOVE>";
        return;
    }

    oss << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
    oss << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst));
    if (move.is_promotion()) oss << " =" << typeToCharLowercase[move.promotion_piece()];
    if (move.is_en_passant()) oss << " ep";
    if (move.is_castle_left()) oss << " O-O-O";
    if (move.is_castle_right()) oss << " O-O";
}

void tc::debug_tostr_xmove(std::ostream& oss, Board& b, ExtMove<true>* xMove) {
    Move move = xMove->move;
    if (move.null()) {
        oss << "<NULL MOVE>";
        return;
    }

    Piece p = xMove->piece;
    oss << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
    oss << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst));
    if (xMove->captured != NULL_PIECE) oss << " x" << pieceToChar(xMove->captured);
    bool isCheck = (b.checkingSquares[!IS_WHITE_PIECE(p)][TYPE_OF_PIECE(p)] & (1ULL << move.dst)) > 1;
    if (isCheck) oss << " #";
    if (move.is_promotion()) oss << " =" << typeToCharLowercase[move.promotion_piece()];
    if (move.is_en_passant()) oss << " ep";
    if (move.is_castle_left()) oss << " O-O-O";
    if (move.is_castle_right()) oss << " O-O";
}