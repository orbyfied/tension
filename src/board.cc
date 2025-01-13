#include "board.hh"

#include <sys/time.h>
#include <random>

namespace tc {

static u64 seedrng() {
    // seed the random number gen
    struct timeval tv;
    gettimeofday(&tv, NULL);
    u64 seed = tv.tv_sec * 1000ULL + (tv.tv_usec);
    srand(seed);
    return seed;
}

template<int arraySize>
static const PositionHashArray<arraySize> init_zarray() {
    PositionHashArray<arraySize> array;

    // generate random number for each index
    for (int i = 0; i < array.size(); i++) {
        u64 r = ((u64)rand() | ((u64)rand() << 32) | ((u64)rand() << 16) | ((u64)rand() << 48));  
        array[i] = r;
    }

    return array;
}

static u64 __seed = seedrng();

extern const PositionHashArray<1 << 12> pieceSqHashes = init_zarray<1 << 12>();
extern const PositionHashArray<65> enPassantSqHashes = init_zarray<65>();

Board::Board() {
    // init bitboards to 0 idk if this is needed tbh
    memset(&pieceArray, NULL_PIECE, 64);
    memset(&pieceBBs, 0, 32 * sizeof(Bitboard));
    memset(&allPiecesPerColor, 0, 2 * sizeof(Bitboard));
    allPieces = 0;
}

void Board::recalculate_state() {
    clear_state_for_recalculation();
    recalculate_state_sided<WHITE>();
    recalculate_state_sided<BLACK>();
}

void Board::load_fen(const char* str) {
    int len = strlen(str);
    if (strcmp(str, "startpos") == 0) {
        load_fen(startFEN);
        return;
    }

    VolatileBoardState* state = volatile_state();

    // parse piece placement
    u8 rank = 7;
    u8 file = 0;
    int i = 0;
    while (i < len && str[i] != ' ') {
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
        this->set_piece<false>(INDEX(file, rank), type | color);
        file += 1;
        i++;
    }

    i++;
    if (i < len) {
        // parse side to move
        turn = tolower(str[i]) == 'w';
        i++;
    }

    // parse castling rights
    i++;
    state->castlingStatus[BLACK] = state->castlingStatus[WHITE] = 0;
    while (i < len && str[i] != ' ') {
        Color color = isupper(str[i]);
        u8 flags = tolower(str[i]) == 'k' ? CAN_CASTLE_R : CAN_CASTLE_L;
        state->castlingStatus[color] |= flags;
        i++;
    }

    // parse en passant sq
    i++;
    if ((i + 2) < len) {
        if (str[i] == '-') {
            state->enPassantTarget = NULL_SQ;
            i++;
        } else {
            state->enPassantTarget = sq_str_to_index(&str[i]);
            i += 2;
        }
    }

    auto parseInt = [&]() -> int {
        int res = 0;
        while (i < len && isdigit(str[i])) {
            res *= 10;
            res += str[i] - '0';
            i++;
        }

        return res;
    };

    // parse halfmove clock
    i++;
    if (i < len) {
        state->rule50Ply = parseInt();
    }

    // parse full move counter
    i++;
    if (i < len) {
        ply = (parseInt() - 1) * 2;
    }

    this->recalculate_state();
}

}