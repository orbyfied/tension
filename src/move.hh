#pragma once

#include <string>
#include <sstream>
#include <concepts>

#include "types.hh"
#include "piece.hh"

namespace tc {

#define CASTLED_L (1 << 0)      // Has castled Queen Side (left)
#define CASTLED_R (1 << 1)      // Has castled King Side (right)
#define CAN_CASTLE_L (1 << 2)   // Castle rights Queen side (left)
#define CAN_CASTLE_R (1 << 3)   // Castle rights King side (right)

#define MOVE_EN_PASSANT     0x1
#define MOVE_PROMOTE_KNIGHT 0x2
#define MOVE_PROMOTE_BISHOP 0x3
#define MOVE_PROMOTE_ROOK   0x4
#define MOVE_PROMOTE_QUEEN  0x5
#define MOVE_CASTLE_RIGHT   0x6
#define MOVE_CASTLE_LEFT    0x7
#define MOVE_DOUBLE_PUSH    0x8

enum MobilityType {
    PAWN_MOBILITY = PAWN,
    KNIGHT_MOBILITY = KNIGHT,
    DIAGONAL = BISHOP,
    STRAIGHT = ROOK,
    QUEEN_MOBILITY = QUEEN,

    MOBILITY_TYPE_COUNT = NULL_PIECE_TYPE
};

#define INDEX(file, rank) ((rank) * 8 + (file))
#define FILE(index) ((index) & 0x7)
#define RANK(index) (((index) >> 3) & 0x7)

/* INDEX DECLARATIONS */
#define FILE_TO_CHAR(file) ((char)((file) + 'a'))
#define RANK_TO_CHAR(rank) ((char)((rank) + '1'))
#define CHAR_TO_FILE(c) ((u8)((c) - 'a'))
#define CHAR_TO_RANK(c) ((u8)((c) - '1'))

inline Sq sq_str_to_index(const char* ptr) {
    return INDEX(CHAR_TO_FILE(ptr[0]), RANK_TO_CHAR(ptr[1]));
}

enum : int {
    fA = 0,
    fB = 1,
    fC = 2,
    fD = 3,
    fE = 4,
    fF = 5,
    fG = 6,
    fH = 7
};

enum : int {
    r1 = 0 << 3,
    r2 = 1 << 3,
    r3 = 2 << 3,
    r4 = 3 << 3,
    r5 = 4 << 3,
    r6 = 5 << 3,
    r7 = 6 << 3,
    r8 = 7 << 3
};

struct Board;

/// @brief All data about a move 
struct Move {
    static inline Move make(Sq src, Sq dst) { return { .src = src, .dst = dst }; }
    static inline Move make(Sq src, Sq dst, u8 flags) { return { .src = src, .dst = dst, .flags = flags }; }
    static inline Move make_en_passant(Sq src, Sq dst) { return { .src = src, .dst = dst, .flags = MOVE_EN_PASSANT }; }
    static inline Move make_castle_left(Sq src, Sq dst) { return { .src = src, .dst = dst, .flags = MOVE_CASTLE_LEFT }; }
    static inline Move make_castle_right(Sq src, Sq dst) { return { .src = src, .dst = dst, .flags = MOVE_CASTLE_RIGHT }; }
    static inline Move make_double_push(Sq src, Sq dst) { return { .src = src, .dst = dst, .flags = MOVE_DOUBLE_PUSH }; }

    // The source and destination indices
    Sq src : 6;
    Sq dst : 6;
    // The additional move flags
    u8 flags : 4 = 0;

    inline bool null() const { return src == 0 && dst == 0; }

    inline Sq source() const { return src; }
    inline Sq destination() const { return dst; }
    inline u8 flags_raw() const { return flags; }

    Piece moved_piece(Board const* b) const;
    Piece captured_piece(Board const* b) const;
    bool is_capture(Board const* b) const;
    bool is_check_estimated(Board const* b) const;

    template<Color color>
    inline Sq capture_index() const { return dst - (is_en_passant() * SIDE_OF_COLOR(color) * 8); }

    inline bool is_double_push() const { return flags == MOVE_DOUBLE_PUSH; }
    inline bool is_en_passant() const { return flags == MOVE_EN_PASSANT; }
    inline bool is_promotion() const { return flags == MOVE_PROMOTE_KNIGHT || flags == MOVE_PROMOTE_BISHOP || flags == MOVE_PROMOTE_ROOK || flags == MOVE_PROMOTE_QUEEN; }
    inline bool is_castle() const { return flags == MOVE_CASTLE_LEFT || flags == MOVE_CASTLE_RIGHT; }
    inline bool is_castle_left() const { return flags == MOVE_CASTLE_LEFT; }
    inline bool is_castle_right() const { return flags == MOVE_CASTLE_RIGHT; }
    inline PieceType promotion_piece() const {
        switch (flags) {
            case MOVE_PROMOTE_KNIGHT: return KNIGHT;
            case MOVE_PROMOTE_BISHOP: return BISHOP;
            case MOVE_PROMOTE_ROOK:   return ROOK;
            case MOVE_PROMOTE_QUEEN:  return QUEEN;
        }

        return NULL_PIECE_TYPE;
    }
};

/// Represents the absence of a move.
static Move NULL_MOVE = { .src = 0, .dst = 0 };

#define MOVE_HASH(move) ((i32)((move.src) | (move.dst << 6)))

/// @brief Heap-allocated hashtable containing cached scores for moves derived from the evaluation during search
struct MoveEvalTable {
    i16* data = nullptr;
    i16 capacity;

    void alloc(i32 capacityInEntries); 
    void free();
    ~MoveEvalTable();

    inline void add(Move move, i16 eval) {
        this->data[MOVE_HASH(move) % capacity] = eval;
    }

    inline i16 get_adjustment(Move move) {
        return this->data[MOVE_HASH(move) % capacity];
    }
};

}