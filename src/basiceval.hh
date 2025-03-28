#pragma once

#include "search.hh"

namespace tc {

template<Color color>
forceinline i32 count_material_eval(Board* b) {
    i32 count = 0;
    count += _popcount64(b->pieces(color, PAWN)) * evalValuePawn;
    count += _popcount64(b->pieces(color, KNIGHT)) * evalValueKnight;
    count += _popcount64(b->pieces(color, BISHOP)) * evalValueBishop;
    count += _popcount64(b->pieces(color, ROOK)) * evalValueRook;
    count += _popcount64(b->pieces(color, QUEEN)) * evalValueQueen;
    return count;
}

/// @brief Basic, classic static evaluation.
struct BasicStaticEvaluator {
    inline i32 eval(Board* board) {
        i32 score = 0;

        // count material score
        score += count_material_eval<WHITE>(board) - count_material_eval<BLACK>(board);

        // score pawn structure
        
        return score;
    }
};

}