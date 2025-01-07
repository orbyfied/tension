#pragma once

#include <memory.h>
#include <iostream>

#include "types.hh"
#include "platform.hh"

namespace tc {

// fwd ref
void debug_tostr_bitboard(std::ostringstream& oss, u64 bb);

/* Bitboards */

typedef u64 Bitboard;

#define BITBOARD_FULL_MASK     (0xFF'FF'FF'FF'FF'FF'FF'FFULL)
#define BITBOARD_RANK0_MASK    (0x00'00'00'00'00'00'00'7EULL)
#define BITBOARD_RANK0_27_MASK (0x00'00'00'00'00'00'00'FFULL)
#define BITBOARD_FILE0_MASK    (0x01'01'01'01'01'01'01'01ULL)
#define BITBOARD_FILE0_27_MASK (0x00'01'01'01'01'01'01'00ULL)

#define BITBOARD_RANK_BOUND_MASK(end) (BITBOARD_RANK0_MASK + ())
#define BITBOARD_FILE_BOUND_MASK(end) (BITBOARD_FILE0_MASK + ((1ULL << (end)) * BITBOARD_FILE0_MASK))
#define BITBOARD_FILE_RANGE_MASK(start, end) (BITBOARD_FILE_BOUND_MASK((end) - (start)) << (start))

#define BITBOARD_RANK_MASK(rank) (BITBOARD_RANK0_MASK << (rank * 8))
#define BITBOARD_FILE_MASK(file) (BITBOARD_FILE0_MASK << (file))

/// Get the bits corresponding to the given rank from the bitboard in a u8.
#define BITBOARD_RANK(bb, rank) (((bb) << (rank * 8)) & 0xFF)

/// Load the bits corresponding to the given file from bottom to top in a u8, essentially
/// transposing the vertical file to a horizontal rank.
#define BITBOARD_LOAD_FILE_U8(bb, file) (_pext(bb, BITBOARD_FILE_MASK(file)))
/// Store a u8 of bits to a specific file, bottom to top, in the given bitboard.
#define BITBOARD_STORE_FILE_U8(bb, file) (_pdep(bb, BITBOARD_FILE_MASK(file)))

#define BITBOARD_INDEX_LOOP_MUT_LE(bb, index) \
                                index = 0; \
                                for (; bb > 0; index++, bb >>= 1ULL) { \
                                    int tz = __builtin_ctzll(bb); \
                                    bb >>= tz; \
                                    index += tz; \
                                    if (bb == 0) { \
                                        break; \
                                    } \

#define CLOSE }

}