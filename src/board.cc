#include "board.hh"

namespace tc {

Board::Board() {
    // init bitboards to 0 idk if this is needed tbh
    memset(&pieces, NULL_PIECE, 64);
    memset(&pieceBBs, 0, 32 * sizeof(Bitboard));
    memset(&allPiecesPerColor, 0, 2 * sizeof(Bitboard));
    allPieces = 0;
}

void Board::load_fen(const char* str) {
    int len = strlen(str);
    if (strcmp(str, "startpos") == 0) {
        load_fen(startFEN);
        return;
    }

    // parse piece placement
    u8 rank = 7;
    u8 file = 0;
    int i = 0;
    while (i < len && str[i] != ' ')
    {
        if (str[i] == '/') {
            rank--;
            file = 0;
            i++;
            continue;
        }

        // parse skip
        if (isdigit(str[i])) {
            file += str[i] - '0';
            i++;
            continue;
        }
        

        // parse pieces
        char pieceChar = str[i];
        u8 color = isupper(pieceChar) * WHITE_PIECE;
        PieceType type = charToPieceType(pieceChar);
        this->set_piece<true>(INDEX(file, rank), type | color);
        file += 1;
        i++;
    }
}

}