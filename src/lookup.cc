#include "lookup.hh"

namespace tc::lookup {

// Provide all vars
extern constexpr PrecalcDistanceFromEdge precalcDistanceFromEdge { };
extern constexpr PrecalcPawnAttackBBs precalcPawnAttackBBs { };
extern constexpr PrecalcKnightAttackBBs precalcKnightAttackBBs { };
extern constexpr PrecalcKingAttackBBs precalcKingAttackBBs { };
extern constexpr PrecalcUnobstructedRookSlidingAttackBBs precalcUnobstructedRookAttackBBs { };
extern constexpr PrecalcUnobstructedBishopSlidingAttackBBs precalcUnobstructedBishopAttackBBs { };

namespace __pext {

extern constexpr PrecalcRookAttackBBs precalcRookAttackBBs { };
extern constexpr PrecalcBishopAttackBBs precalcBishopAttackBBs { };

}

}