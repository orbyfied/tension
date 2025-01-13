#pragma once

#include "bitboard.hh"
#include "move.hh"
#include "constexpr.hh"

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
                Sq index = rank * 8 + file;

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
                    Sq index = file + rank * 8;

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
                Sq index = file + rank * 8;

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

/// Pre-calculated bitboards for king movement and attacks per square
struct PrecalcKingMovementBBs {
    Bitboard values[64];

    constexpr PrecalcKingMovementBBs() : values() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                Sq index = file + rank * 8;

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
struct PrecalcUnobstructedRookSlidingAttackBBs {
    Bitboard values[64];
    Bitboard blockerMasks[64];

    constexpr PrecalcUnobstructedRookSlidingAttackBBs() : values(), blockerMasks() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                Sq index = file + rank * 8;

                Bitboard bb = /* rank */ (BITBOARD_RANK0_MASK << (rank * 8)) | /* file */ (BITBOARD_FILE0_MASK << file);
                bb &= ~(1ULL << index); // exclude own square
                values[index] = bb;

                Bitboard blockerMask = /* rank */ (BITBOARD_RANK0_27_MASK << (rank * 8)) | /* file */ (BITBOARD_FILE0_27_MASK << file);
                blockerMask &= ~(1ULL << index); // exclude own square
                blockerMasks[index] = blockerMask;
            }
        }
    }
};

/// Pre-calculated bitboards for diagonal sliding attacks per square without account for blockers
struct PrecalcUnobstructedBishopSlidingAttackBBs {
    Bitboard values[64];
    Bitboard blockerMasks[64];

    constexpr PrecalcUnobstructedBishopSlidingAttackBBs() : values(), blockerMasks() {
        for (u8 file = 0; file < 8; file++) {
            for (u8 rank = 0; rank < 8; rank++) {
                Sq index = file + rank * 8;

                Bitboard bb = 0;
                Bitboard blockerMask = 0;
                int x = 0, y = 0;

                /* NE */ {
                    x = file; y = rank;
                    while (x < 7 && y < 7)
                    {
                        Sq index = INDEX(x, y);
                        bb |= (1ULL << index);
                        blockerMask |= (1ULL << index);
                        x++; y++;
                    }

                    Sq index = INDEX(x, y);
                    bb |= (1ULL << index);
                }

                /* SE */ {
                    x = file; y = rank;
                    while (x < 7 && y >= 1)
                    {
                        Sq index = INDEX(x, y);
                        bb |= (1ULL << index);
                        blockerMask |= (1ULL << index);
                        x++; y--;
                    }

                    Sq index = INDEX(x, y);
                    bb |= (1ULL << index);
                }

                /* NW */ {
                    x = file; y = rank;
                    while (x >= 1 && y < 7)
                    {
                        Sq index = INDEX(x, y);
                        bb |= (1ULL << index);
                        blockerMask |= (1ULL << index);
                        x--; y++;
                    }

                    Sq index = INDEX(x, y);
                    bb |= (1ULL << index);
                }

                /* SW */ {
                    x = file; y = rank;
                    while (x >= 1 && y >= 1)
                    {
                        Sq index = INDEX(x, y);
                        bb |= (1ULL << index);
                        blockerMask |= (1ULL << index);
                        x--; y--;
                    }

                    Sq index = INDEX(x, y);
                    bb |= (1ULL << index);
                }

                Bitboard clearSelfSq = ~(1ULL << index);
                values[index] = bb & clearSelfSq;
                blockerMasks[index] = blockerMask & clearSelfSq;
            }
        }
    }
};

extern const PrecalcDistanceFromEdge distanceFromEdge;
extern const PrecalcPawnAttackBBs pawnAttackBBs;
extern const PrecalcKnightAttackBBs knightAttackBBs;
extern const PrecalcKingMovementBBs kingMovementBBs;
extern const PrecalcUnobstructedRookSlidingAttackBBs unobstructedRookAttackBBs;
extern const PrecalcUnobstructedBishopSlidingAttackBBs unobstructedBishopAttackBBs;

#ifndef TC_LOOKUP_TYPE
#define TC_LOOKUP_TYPE pext
#endif

/* ------------- Lookup using PEXT instructions ------------- */
#if TC_LOOKUP_TYPE == pext
namespace __pext {

inline u64 rook_attack_key(Sq index, u64 blockers) {
    Bitboard mask = unobstructedRookAttackBBs.blockerMasks[index];
    return _pext_u64(blockers, mask);
}

inline u64 bishop_attack_key(Sq index, u64 blockers) {
    Bitboard mask = unobstructedBishopAttackBBs.blockerMasks[index];
    return _pext_u64(blockers, mask);
}

/// The magic tables are not marked as constexpr so the calculation is done at program start, because
/// for some reason doing these at compile time takes extremely long.

/// Pre-calculated bitboards per blockers by mask per square.
struct PrecalcRookAttackBBs {
    Bitboard* values[64];
    u16* xrayKeys[64]; // xray key per bb

    PrecalcRookAttackBBs() {
        for (Sq index = 0; index < 64; index++) {
            u8 file = FILE(index); u8 rank = RANK(index);

            // extract blocker mask and gen info
            Bitboard mask = unobstructedRookAttackBBs.blockerMasks[index];
            u8 blockerCount = _popcount64(mask);

            // allocate tables
            Bitboard* bbTable = new Bitboard[1 << blockerCount];
            u16* xrayKeyTable = new u16[1 << blockerCount];
            values[index] = bbTable;
            xrayKeys[index] = xrayKeyTable;

            // use pdep to gen blockers from numbers
            for (u64 key = 0; key < (1 << blockerCount); key++) {
                Bitboard blockers = _pdep_u64(key, mask);
                Bitboard fb = 0;
                Bitboard bb = 0;
                int x = file, y = rank;

                for (x = file; x < 8; x++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file; x >= 0; x--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; y < 8; y++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; y >= 0; y--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                bb &= ~(1ULL << index); // exclude own sq
                bbTable[key] = bb;

                u16 xrayKey = _pext_u64(blockers & ~fb, mask);
                xrayKeyTable[key] = xrayKey;
            }
        }
    }
};

/// Pre-calculated bitboards per blockers by mask per square.
struct PrecalcBishopAttackBBs {
    Bitboard* values[64];
    u16* xrayKeys[64]; // xray key per bb

    PrecalcBishopAttackBBs() {
        for (Sq index = 0; index < 64; index++) {
            u8 file = FILE(index); u8 rank = RANK(index);

            // extract blocker mask and gen info
            Bitboard mask = unobstructedBishopAttackBBs.blockerMasks[index];
            u8 blockerCount = _popcount64(mask);

            // allocate tables
            Bitboard* bbTable = new Bitboard[1 << blockerCount];
            u16* xrayKeyTable = new u16[1 << blockerCount];
            values[index] = bbTable;
            xrayKeys[index] = xrayKeyTable;

            // use pdep to gen blockers from numbers
            for (u64 key = 0; key < (1 << blockerCount); key++) {
                Bitboard blockers = _pdep_u64(key, mask);
                Bitboard fb = 0;
                Bitboard bb = 0;
                int x = file, y = rank;

                for (x = file, y = rank; x < 8 && y < 8; x++, y++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; x >= 0 && y < 8; x--, y++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; x >= 0 && y >= 0; x--, y--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; x < 8 && y >= 0; x++, y--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                bb &= ~(1ULL << index); // exclude own sq
                bbTable[key] = bb;

                u16 xrayKey = _pext_u64(blockers & ~fb, mask);
                xrayKeyTable[key] = xrayKey;
            }
        }
    }
};

extern const PrecalcRookAttackBBs rookAttackBBs;     // ~1280 kb each, which is kinda insane
extern const PrecalcBishopAttackBBs bishopAttackBBs; // bruh

inline Bitboard rook_attack_bb(Sq index, u64 blockers) {
    return rookAttackBBs.values[index][rook_attack_key(index, blockers)];
}

inline Bitboard bishop_attack_bb(Sq index, u64 blockers) {
    return bishopAttackBBs.values[index][bishop_attack_key(index, blockers)];
}

inline u64 rook_xray_key(Sq index, u64 lastKey) {
    return rookAttackBBs.xrayKeys[index][lastKey];
}

inline u64 bishop_xray_key(Sq index, u64 lastKey) {
    return bishopAttackBBs.xrayKeys[index][lastKey];
}

inline Bitboard rook_xray_bb(Sq index, u64 lastKey) {
    return rookAttackBBs.values[index][rook_xray_key(index, lastKey)];
}

inline Bitboard bishop_xray_bb(Sq index, u64 lastKey) {
    return bishopAttackBBs.values[index][bishop_xray_key(index, lastKey)];
}

inline Bitboard get_rook_attack_bb(Sq index, u64 key) {
    return rookAttackBBs.values[index][key];
}

inline Bitboard get_bishop_attack_bb(Sq index, u64 key) {
    return bishopAttackBBs.values[index][key];
}

}
#endif

#if TC_LOOKUP_TYPE == magic && false
/* ------------- Lookup using magic bitboards ------------- */
namespace __magic {

extern const u64 rookMagicPerSq[64];
extern const u64 bishopMagicPerSq[64]; 

extern const u8 rookMagicShift[64];    // 64 - index bits
extern const u8 bishopMagicShift[64]; 

inline u64 rook_attack_key(Sq index, u64 blockers) {
    return (rookMagicPerSq[index] * (unobstructedRookAttackBBs.blockerMasks[index] & blockers)) >> (rookMagicShift[index]);
}

inline u64 bishop_attack_key(Sq index, u64 blockers) {
    return (bishopMagicPerSq[index] * (unobstructedBishopAttackBBs.blockerMasks[index] & blockers)) >> (bishopMagicShift[index]);
}

/// Pre-calculated bitboards per blockers by mask per square.
struct PrecalcRookAttackBBs {
    Bitboard* values[64];
    u16* xrayKeys[64]; // xray key per bb

    PrecalcRookAttackBBs() : values(), xrayKeys() {
        for (Sq index = 0; index < 64; index++) {
            u8 file = FILE(index); u8 rank = RANK(index);

            // extract blocker mask and gen info
            Bitboard mask = unobstructedRookAttackBBs.blockerMasks[index];
            u8 blockerCount = _popcount64(mask);

            // allocate tables
            u8 tableSize = 1 << (64 - rookMagicShift[index]);
            Bitboard* bbTable = new Bitboard[tableSize];
            u16* xrayKeyTable = new u16[tableSize];
            values[index] = bbTable;
            xrayKeys[index] = xrayKeyTable;

            // use pdep to gen blockers from numbers
            for (u64 blockersExt = 0; blockersExt < (1 << blockerCount); blockersExt++) {
                Bitboard blockers = _pdep_u64(blockersExt, mask);
                u64 key = blockers * rookMagicPerSq[index];
                Bitboard fb = 0;
                Bitboard bb = 0;
                int x = file, y = rank;

                for (x = file; x < 8; x++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file; x >= 0; x--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; y < 8; y++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; y >= 0; y--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                bb &= ~(1ULL << index); // exclude own sq
                bbTable[key] = bb;

                u16 xrayKey = (blockers & ~fb) * rookMagicPerSq[index];
                xrayKeyTable[key] = xrayKey;
            }
        }
    }
};

/// Pre-calculated bitboards per blockers by mask per square.
struct PrecalcBishopAttackBBs {
    Bitboard* values[64];
    u16* xrayKeys[64]; // xray key per bb

    PrecalcBishopAttackBBs() : values(), xrayKeys() {
        for (Sq index = 0; index < 64; index++) {
            u8 file = FILE(index); u8 rank = RANK(index);

            // extract blocker mask and gen info
            Bitboard mask = unobstructedBishopAttackBBs.blockerMasks[index];
            u8 blockerCount = _popcount64(mask);

            // allocate tables
            u8 tableSize = 1 << (64 - bishopMagicShift[index]);
            Bitboard* bbTable = new Bitboard[tableSize];
            u16* xrayKeyTable = new u16[tableSize];
            values[index] = bbTable;
            xrayKeys[index] = xrayKeyTable;

            // use pdep to gen blockers from numbers
            for (u64 blockersExt = 0; blockersExt < (1 << blockerCount); blockersExt++) {
                Bitboard blockers = _pdep_u64(blockersExt, mask);
                u64 key = blockers * rookMagicPerSq[index];
                Bitboard fb = 0;
                Bitboard bb = 0;
                int x = file, y = rank;

                for (x = file, y = rank; x < 8 && y < 8; x++, y++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; x >= 0 && y < 8; x--, y++) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; x >= 0 && y >= 0; x--, y--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                for (x = file, y = rank; x < 8 && y >= 0; x++, y--) {
                    Sq index = INDEX(x, y);
                    bb |= 1ULL << index;
                    if ((blockers & (1ULL << index)) > 1) { fb |= 1ULL << index; break; };
                }

                bb &= ~(1ULL << index); // exclude own sq
                bbTable[key] = bb;

                u16 xrayKey = (blockers & ~fb) * rookMagicPerSq[index];
                xrayKeyTable[key] = xrayKey;
            }
        }
    }
};

extern const PrecalcRookAttackBBs rookAttackBBs;     // ~1280 kb each, which is kinda insane
extern const PrecalcBishopAttackBBs bishopAttackBBs; // bruh

inline Bitboard rook_attack_bb(Sq index, u64 blockers) {
    return rookAttackBBs.values[index][rook_attack_key(index, blockers)];
}

inline Bitboard bishop_attack_bb(Sq index, u64 blockers) {
    return bishopAttackBBs.values[index][bishop_attack_key(index, blockers)];
}

inline u64 rook_xray_key(Sq index, u64 lastKey) {
    return rookAttackBBs.xrayKeys[index][lastKey];
}

inline u64 bishop_xray_key(Sq index, u64 lastKey) {
    return bishopAttackBBs.xrayKeys[index][lastKey];
}

inline Bitboard get_rook_attack_bb(Sq index, u64 key) {
    return rookAttackBBs.values[index][key];
}

inline Bitboard get_bishop_attack_bb(Sq index, u64 key) {
    return bishopAttackBBs.values[index][key];
}

}
#endif


/*
    Select lookup/magic type
*/

namespace magic = CONCAT(__, TC_LOOKUP_TYPE);

}