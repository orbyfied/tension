#pragma once

#include "bitboard.hh"
#include "move.hh"

/*
    Lookups used for movegen, tracking attacks and checks, fast distance calculations,
    fast eval, etc. All under namespace tc::lookup
 */

namespace tc::lookup {

/* Chess Related Constants and masks */

#define LOOKUP_INDEX_CLOSEST_EDGE   8
#define LOOKUP_INDEX_CLOSEST_H_EDGE 9
#define LOOKUP_INDEX_CLOSEST_V_EDGE 10

/// Pre-calculated EXCLUSIVE distance to the edge for each direction per square, as well as
/// the closest edge to the piece per square (can only be NSEW, no diagonal directions).
/// Exclusive meaning, if the piece is touching the edge it's distance to that edge is zero.
struct PrecalcDistanceFromEdge {
    u8 values[64][DIRECTION_COUNT + /* closest edges */ 3];

    constexpr PrecalcDistanceFromEdge() : values() {
        for (int file = 0; file < 8; file++) {
            for (int rank = 0; rank < 8; rank++) {
                u8 index = rank * 8 + file;

                // straight directions
                values[index][NORTH] = 7 - rank;
                values[index][SOUTH] = rank;
                values[index][WEST]  = file;
                values[index][EAST]  = 7 - file;

                // diagonal directions
                values[index][NORTH_EAST] = MIN(7 - rank, 7 - file);
                values[index][NORTH_WEST] = MIN(7 - rank, file);
                values[index][SOUTH_EAST] = MIN(rank, 7 - file);
                values[index][SOUTH_WEST] = MIN(rank, file);

                // closest edge
                Direction closestH = (file > 3) ? WEST : EAST;
                Direction closestV = (rank > 3) ? SOUTH : NORTH; 
                Direction closest = (values[index][closestH] > values[index][closestV]) ? closestV : closestH;
                values[index][LOOKUP_INDEX_CLOSEST_EDGE] = closest;
                values[index][LOOKUP_INDEX_CLOSEST_H_EDGE] = closestH;
                values[index][LOOKUP_INDEX_CLOSEST_V_EDGE] = closestV;
             }
        }
    }
};

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

/// Pre-calculated bitboards for straight sliding attacks per square without accounting for blockers
struct PrecalcUnobstructedStraightSlidingAttackBBs {
    Bitboard values[64];

    constexpr PrecalcUnobstructedStraightSlidingAttackBBs() : values() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                u8 index = file + rank * 8;

                Bitboard bb = /* rank */ (BITBOARD_RANK0_MASK << (rank * 8)) | /* file */ (BITBOARD_FILE0_MASK << file);
                bb &= ~(1ULL << index); // exclude own square
                values[index] = bb;
            }
        }
    }
};

/// Pre-calculated bitboards for diagonal sliding attacks per square without account for blockers
struct PrecalcUnobstructedDiagonalSlidingAttackBBs {
    Bitboard values[64];

    constexpr PrecalcUnobstructedDiagonalSlidingAttackBBs() : values() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                u8 index = file + rank * 8;

                Bitboard bb = 0;
                int x = 0, y = 0;

                x = file + 1; y = rank + 1;
                while (x < 8 && y < 8)
                {
                    u8 index = INDEX(x, y);
                    bb |= (1ULL << index);
                    x++; y++;
                }

                x = file + 1; y = rank - 1;
                while (x < 8 && y >= 0)
                {
                    u8 index = INDEX(x, y);
                    bb |= (1ULL << index);
                    x++; y--;
                }

                x = file - 1; y = rank + 1;
                while (x >= 0 && y < 8)
                {
                    u8 index = INDEX(x, y);
                    bb |= (1ULL << index);
                    x--; y++;
                }

                x = file - 1; y = rank - 1;
                while (x >= 0 && y >= 0)
                {
                    u8 index = INDEX(x, y);
                    bb |= (1ULL << index);
                    x--; y--;
                }

                values[index] = bb;
            }
        }
    }
};

extern const PrecalcDistanceFromEdge precalcDistanceFromEdge;
extern const PrecalcPawnAttackBBs precalcPawnAttackBBs;
extern const PrecalcKnightAttackBBs precalcKnightAttackBBs;
extern const PrecalcKingAttackBBs precalcKingAttackBBs;
extern const PrecalcUnobstructedStraightSlidingAttackBBs precalcUnobstructedStraightSlidingAttackBBs;
extern const PrecalcUnobstructedDiagonalSlidingAttackBBs precalcUnobstructedDiagonalSlidingAttackBBs;

// todo: PEXT/magic bb precalc for sliders

}