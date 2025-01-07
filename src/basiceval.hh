#pragma once

#include "search.hh"

namespace tc {

/// @brief Basic, classic static evaluation.
struct BasicStaticEvaluator : Evaluator {
    virtual i32 eval(Board* board, EvalData& evalData) {
        i32 score = 0;

        // count material score
        score += board->count_material<WHITE>() - board->count_material<BLACK>();
        
        return score;
    }
};

}