#include "lookup.hh"

namespace tc::lookup {

// Provide all vars
extern constexpr PrecalcDistanceFromEdge distanceFromEdge { };
extern constexpr PrecalcPawnAttackBBs pawnAttackBBs { };
extern constexpr PrecalcKnightAttackBBs precalcKnightAttackBBs { };
extern constexpr PrecalcKingAttackBBs kingAttackBBs { };
extern constexpr PrecalcUnobstructedRookSlidingAttackBBs unobstructedRookAttackBBs { };
extern constexpr PrecalcUnobstructedBishopSlidingAttackBBs unobstructedBishopAttackBBs { };

namespace __pext {

// Not computed at compile time, instead done at program start
extern const PrecalcRookAttackBBs rookAttackBBs { };
extern const PrecalcBishopAttackBBs bishopAttackBBs { };

}

}