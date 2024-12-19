#include "lookup.hh"

namespace tc::lookup {

// Provide all vars
extern constexpr PrecalcDistanceFromEdge precalcDistanceFromEdge { };
extern constexpr PrecalcPawnAttackBBs precalcPawnAttackBBs { };
extern constexpr PrecalcKnightAttackBBs precalcKnightAttackBBs { };
extern constexpr PrecalcKingAttackBBs precalcKingAttackBBs { };
extern constexpr PrecalcUnobstructedStraightSlidingAttackBBs precalcUnobstructedStraightSlidingAttackBBs { };
extern constexpr PrecalcUnobstructedDiagonalSlidingAttackBBs precalcUnobstructedDiagonalSlidingAttackBBs { };

}