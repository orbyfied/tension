#include <memory.h>
#include <iostream>

#include "types.h"

void debug_tostr_bitboard(std::ostringstream& oss, u64 bb);

/* Bitboards */

typedef u64 Bitboard;

inline Bitboard bitMaskExcludeRanks(u8 pivotRank, u8 exludeStartRank) {
    return 0;
}

inline Bitboard bitMaskExcludeFiles(u8 pivotFile, u8 exludeStartFile) {
    return 0;
}

#define BITBOARD_RANK0_MASK 0x0000_0000_0000_00FF
#define BITBOARD_FILE0_MASK 0x0101_0101_0101_0101

#define BITBOARD_RANK_MASK(rank) (BITBOARD_RANK0_MASK << (rank * 8))
#define BITBOARD_FILE_MASK(file) (BITBOARD_FILE0_MASK << (file))

/// Get the bits corresponding to the given rank from the bitboard in a u8.
#define BITBOARD_RANK(bb, rank) (((bb) << (rank * 8)) & 0xFF)

/// Load the bits corresponding to the given file from bottom to top in a u8, essentially
/// transposing the vertical file to a horizontal rank.
#define BITBOARD_LOAD_FILE_U8(bb, file) \
                                
/// Store a u8 of bits to a specific file, bottom to top, in the given bitboard.
#define BITBOARD_STORE_FILE_U8(bb, file, u) \

#define BITBOARD_INDEX_LOOP_MUT_LE(bb, index) \
                                index = 0; \
                                for (; bb > 0; index++, bb >>= 1ULL) { \
                                    if ((bb & 0b1ULL) == 0) { \
                                        int tz = __builtin_ctzll(bb); \
                                        bb >>= tz; \
                                        index += tz; \
                                        if (bb == 0) { \
                                            break; \
                                        } \
                                    }\

#define CLOSE }

/* Chess Related Constants and masks */

/// Pre-calculated bitboards for pawn attacks per color per square excluding en passant
struct PrecalcPawnAttackBBs {
    Bitboard values[2][64];

    constexpr PrecalcPawnAttackBBs() : values() {
        for (int color = 1; color >= 0; color--) {
            i32 direction = (color ? 8 : -8);
            for (u8 file = 0; file < 8; file++) {
                for (u8 rank = 0; rank < 8; rank++) {
                    values[color][file + rank * 8] = 0;
                }

                for (u8 rank = (color ? 0 : 1); rank < (color ? 7 : 8); rank++) {
                    u8 index = file + rank * 8;

                    // create bitboard
                    Bitboard bb = 0;

                    if (file > 0) bb |= (1ULL << (index + direction - 1));
                    if (file < 7) bb |= (1ULL << (index + direction + 1));

                    values[color][index] = bb;
                }
            }
        }
    }
};

/// Pre-calculated bitboards for knight attacks per square
struct PrecalcKnightAttackBBs {
    Bitboard values[64];

    constexpr PrecalcKnightAttackBBs() : values() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                u8 index = file + rank * 8;

                // create bitboard
                Bitboard bb = 0;

                if (file > 0 && rank < 6) bb |= 1ULL << (index + 8 * 2 - 1);
                if (file > 1 && rank < 7) bb |= 1ULL << (index + 8 * 1 - 2);
                if (file > 0 && rank > 1) bb |= 1ULL << (index - 8 * 2 - 1);
                if (file > 1 && rank > 0) bb |= 1ULL << (index - 8 * 1 - 2);
                if (file < 7 && rank < 6) bb |= 1ULL << (index + 8 * 2 + 1);
                if (file < 6 && rank < 7) bb |= 1ULL << (index + 8 * 1 + 2);
                if (file < 7 && rank > 1) bb |= 1ULL << (index - 8 * 2 + 1);
                if (file < 6 && rank > 0) bb |= 1ULL << (index - 8 * 1 + 2);

                values[index] = bb;
            }
        }
    }
};

/// Pre-calculated bitboards for king attacks per square
struct PrecalcKingAttackBBs {
    Bitboard values[64];

    constexpr PrecalcKingAttackBBs() : values() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                u8 index = file + rank * 8;

                // create bitboard
                Bitboard bb = 0;

                if (file > 0) bb |= 1ULL << (index - 1);
                if (file < 7) bb |= 1ULL << (index + 1);
                if (rank > 0) bb |= 1ULL << (index - 8);
                if (rank < 7) bb |= 1ULL << (index + 8);
                if (file > 0 && rank > 0) bb |= 1ULL << (index - 1 - 8);
                if (file > 0 && rank < 7) bb |= 1ULL << (index - 1 + 8);
                if (file < 7 && rank > 0) bb |= 1ULL << (index + 1 - 8);
                if (file < 7 && rank < 7) bb |= 1ULL << (index + 1 + 8);

                values[index] = bb;
            }
        }
    }
};