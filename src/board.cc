#include "board.hh"

#include <sys/time.h>
#include <random>
#include "util.hh"

namespace tc {

static u64 seedrng() {
    // seed the random number gen
    struct timeval tv;
    gettimeofday(&tv, NULL);
    u64 seed = tv.tv_sec * 1000ULL + (tv.tv_usec);
    srand(seed);
    log<DEBUG>(P, "Zhash rng seed: %llu", seed);
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
extern const PositionHashArray<256> enPassantSqHashes = init_zarray<256>();

extern const PositionHashArray<2> sideToMoveHashes = init_zarray<2>();

Board::Board() {
    // init bitboards to 0 idk if this is needed tbh
    memset(&pieceArray, NULL_PIECE, 64);
    memset(&pieceBBs, 0, 32 * sizeof(Bitboard));
    memset(&allPiecesPerColor, 0, 2 * sizeof(Bitboard));
    allPieces = 0;
}

void Board::load_fen(const char* cstr) {
    if (strcmp(cstr, "startpos") == 0) {
        load_fen(startFEN);
        return;
    }

    std::string str(cstr);
    std::istringstream iss(str);
    std::noskipws(iss);
    std::istream_iterator<char> it(iss);
    load_fen(it, std::istream_iterator<char>());
}

void Board::load_fen(std::istream_iterator<char>& it, const std::istream_iterator<char>& end) {
    skip_whitespace(it);

    if (*it == 's') {
        load_fen(startFEN);
        return;
    }

    VolatileBoardState* state = volatile_state();

    // parse piece placement
    u8 rank = 7;
    u8 file = 0;
    while (*it != ' ' && it != end && (rank > 0 || file < 8)) {
        if (*it == '/') {
            rank--;
            file = 0;
            it++;
            continue;
        }

        // parse skip
        if (isdigit(*it)) {
            file += *it - '0';
            it++;
            continue;
        }

        // parse pieces
        char pieceChar = *it;
        u8 color = isupper(pieceChar) * WHITE_PIECE;
        PieceType type = charToPieceType(pieceChar);
        this->set_piece<false>(INDEX(file, rank), type | color);
        file += 1;
        it++;
    }

    it++;
    if (it != end) {
        // parse side to move
        turn = tolower(*it) == 'w';
        it++;
    } else {
        goto ret;
    }

    // parse castling rights
    it++;
    state->castlingStatus[BLACK] = state->castlingStatus[WHITE] = 0;
    while (it != end && *it != '-' && *it != ' ') {
        Color color = isupper(*it);
        u8 flags = tolower(*it) == 'k' ? CAN_CASTLE_R : CAN_CASTLE_L;
        state->castlingStatus[color] |= flags;
        it++;
    }

    // parse en passant sq
    it++;
    if (it == end) {
        goto ret;
    }

    if (*it == ' ') {
        it++;
    }

    if (it != end) {
        if (*it == '-') {
            state->enPassantTarget = NULL_SQ;
            it++;
        } else {
            char fileChar = *it;
            it++;
            char rankChar = *it;
            state->enPassantTarget = INDEX(CHAR_TO_FILE(fileChar), CHAR_TO_RANK(rankChar));
            it++; it++;
        }
    } else {
        goto ret;
    }

    // parse halfmove clock
    it++;
    if (it != end) {
        state->rule50Ply = parse_int(it, end);
    } else {
        goto ret;
    }

    // parse full move counter
    it++;
    if (it != end) {
        ply = (parse_int(it, end) - 1) * 2;
    } else {
        goto ret;
    }

ret:
    this->recalculate_state();
}

}