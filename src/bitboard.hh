#pragma once

#include <memory.h>
#include <iostream>

#include "types.hh"
#include "platform.hh"

namespace tc {

/* Bitboards */

typedef u64 Bitboard;

/* Basic Masks */

#define sqbb(s) (1ULL << s)
#define SQBB(s) (1ULL << s)

#define BITBOARD_FULL_MASK     (0xFF'FF'FF'FF'FF'FF'FF'FFULL)
#define BITBOARD_RANK0_MASK    (0x00'00'00'00'00'00'00'FFULL)
#define BITBOARD_RANK0_27_MASK (0x00'00'00'00'00'00'00'7EULL)
#define BITBOARD_FILE0_MASK    (0x01'01'01'01'01'01'01'01ULL)
#define BITBOARD_FILE0_27_MASK (0x00'01'01'01'01'01'01'00ULL)

#define BITBOARD_RANK_MASK(rank) (BITBOARD_RANK0_MASK << (rank * 8))
#define BITBOARD_FILE_MASK(file) (BITBOARD_FILE0_MASK << (file))

#define BB_FILES_17_MASK (0x9F'9F'9F'9F'9F'9F'9F'9FULL) // Files 1 to 7, allows movement to the right by one square
#define BB_FILES_28_MASK (0xFE'FE'FE'FE'FE'FE'FE'FEULL) // Files 2 to 8, allows movement to the left by one square

/* Block Masks [INCLUSIVE] */

// #define BB_FILES_XY_MASK(x, y) todo
#define BB_RANKS_XY_MASK(x, y) ((BITBOARD_FULL_MASK >> ((y - x - 1) * 8) << (x * 8)))
#define BB_FILE_SE_MASK(s, e) (())

/* Chess Bitboards */
#define BB_2_OR_7_RANK (0x00'FF'00'00'00'00'FF'00ULL)
#define BB_1_OR_8_RANK (0xFF'00'00'00'00'00'00'FFULL)

/* Precomputed lines on bitboards between 2 squares */
extern Bitboard betweenBBsExcl[64][64];

forceinline Bitboard between_bb_inclusive(Sq a, Sq b) {
    return betweenBBsExcl[a][b] | sqbb(a) | sqbb(b);
}

forceinline Bitboard between_bb_exclusive(Sq a, Sq b) {
    return betweenBBsExcl[a][b];
}

#define CLOSE }

template<int off>
inline u64 shift(u64 bb) {
    if constexpr (off < 0) {
        return bb >> -off;
    } else {
        return bb << off;
    }
}

inline u64 shift(u64 bb, int off) {
    if (off < 0) {
        return bb >> -off;
    } else {
        return bb << off;
    }
}

// template<int off>
// inline u64 shift(u64 bb, int amount) {
//     if constexpr (off < 0) {
//         return bb >> -off * amount;
//     } else {
//         return bb << off * amount;
//     }
// }

struct BitboardToStrOptions {
    char* highlightChars = nullptr;
};

/// @brief Visualize the bitboard in a string using a board layout and ANSI color codes
void debug_tostr_bitboard(std::ostream& oss, u64 bb, BitboardToStrOptions options);

}