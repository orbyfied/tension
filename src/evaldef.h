#pragma once

#include "types.hh"
#include <iomanip>
#include <ostream>

// The scale of the integer evaluation values, determines the precision
// possible when computing evaluation results. 
#define EVAL_SCALE 1000
#define EVAL_NEGATIVE_INFINITY ((i32)(-2147483647))
#define EVAL_POSITIVE_INFINITY ((i32)( 2147483647))

#define EVAL_WIN  (-M0 + 999)    // win for white
#define EVAL_LOSS (-EVAL_WIN)    // loss for current color/white
#define ERR_EVAL  (0x1F1F1F1F)   // error evaluation
#define EVAL_DRAW 0              // any draw
#define M0  (-9999 * EVAL_SCALE) // mate in 0
#define MRS (9000 * EVAL_SCALE)  // where the mate range starts in positive eval

/// Retrieves the amount of moves until mate from the evaluation score, or 0 if no mate was found.
#define COUNT_MATE_IN_PLY(eval) ((-M0 - (eval < 0 ? -eval : eval)))
#define MATED_IN_PLY(moves)     (M0 + moves)
#define IS_MATE_EVAL(eval)      ((eval) < -MRS || (eval) > MRS)

constexpr inline f32 fEval(i32 eval) {
  return eval / (EVAL_SCALE * (float)1);
}

constexpr inline i32 iEval(f32 eval) {
  return (i32)(eval * EVAL_SCALE);
}

inline void write_eval(std::ostream& os, i32 intEval) {
    if (intEval == ERR_EVAL) { os << "ERREVAL"; return; }
    if (intEval == EVAL_WIN) { os << "1-0"; return; }
    if (intEval == EVAL_LOSS) { os << "0-1"; return; }
    if (intEval == EVAL_NEGATIVE_INFINITY) { os << "-INF"; return; }
    if (intEval == EVAL_POSITIVE_INFINITY) { os << "+INF"; return; }
    
    // M in ply
    if (IS_MATE_EVAL(intEval)) {
        i32 inPly = COUNT_MATE_IN_PLY(intEval);
        os << (intEval < 0 ? /* reversed bc mated in ply not mate in */ "-" : "+") << "M" << inPly;
        return;
    }

    os << (intEval > 0 ? "+" : "") << std::setprecision(4) << fEval(intEval);
}