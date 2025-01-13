#include "lookup.hh"

namespace tc::lookup {

// Provide all vars
extern constexpr PrecalcDistanceFromEdge distanceFromEdge { };
extern constexpr PrecalcPawnAttackBBs pawnAttackBBs { };
extern constexpr PrecalcKnightAttackBBs knightAttackBBs { };
extern constexpr PrecalcKingMovementBBs kingMovementBBs { };
extern constexpr PrecalcUnobstructedRookSlidingAttackBBs unobstructedRookAttackBBs { };
extern constexpr PrecalcUnobstructedBishopSlidingAttackBBs unobstructedBishopAttackBBs { };

#if TC_LOOKUP_TYPE == pext
namespace __pext {

// Not computed at compile time, instead done at program start
extern const PrecalcRookAttackBBs rookAttackBBs { };
extern const PrecalcBishopAttackBBs bishopAttackBBs { };

}
#endif

#if TC_LOOKUP_TYPE == magic && false
namespace __magic {

extern const u64 rookMagicPerSq[64] = { 0 };
extern const u64 bishopMagicPerSq[64] = { 0 };

extern const u8 rookMagicShift[64] = { 0 };
extern const u8 bishopMagicShift[64] = { 0 }; 

// Not computed at compile time, instead done at program start
extern const PrecalcRookAttackBBs rookAttackBBs { };
extern const PrecalcBishopAttackBBs bishopAttackBBs { };

}
#endif

}