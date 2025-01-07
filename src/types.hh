#pragma once

#include <stdint.h>
#include <immintrin.h>

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define CONCAT_AUX(A, B) A ## B
#define CONCAT(A, B) CONCAT_AUX(A, B)

/* Base Types */
typedef unsigned char u8;
typedef unsigned short u16;
typedef short i16;
typedef unsigned int u32;
typedef int i32;
typedef unsigned long long u64;
typedef long long i64;
typedef float f32;
typedef long double f64; 

/* Basic Types */
typedef u8 Sq;
#define NULL_SQ ((Sq)0xFF)

/// @brief The boolean color of a player
typedef bool Color;

enum : bool {
  BLACK = 0,
  WHITE = 1
};

enum Direction {
  NORTH = 0,
  SOUTH = 1,
  EAST  = 2,
  WEST  = 3,

  NORTH_EAST = 4,
  NORTH_WEST = 5,
  SOUTH_EAST = 6,
  SOUTH_WEST = 7,

  DIRECTION_COUNT = 8
};

enum DirectionOffset : int {
  OFF_NORTH = 8,
  OFF_SOUTH = -8,
  OFF_EAST  = 1,
  OFF_WEST  = -1,

  OFF_NORTH_EAST = 9,
  OFF_NORTH_WEST = 7,
  OFF_SOUTH_EAST = -7,
  OFF_SOUTH_WEST = -9,
};

constexpr DirectionOffset DIRECTION_TO_OFFSET[DIRECTION_COUNT] = { OFF_NORTH, OFF_SOUTH, OFF_EAST, OFF_WEST, OFF_NORTH_EAST, OFF_NORTH_WEST, OFF_SOUTH_EAST, OFF_SOUTH_WEST };

// The scale of the integer evaluation values, determines the precision
// possible when computing evaluation results. 
#define EVAL_SCALE 1000

#define DRAW_EVAL 0              // any draw
#define M0  (-9999 * EVAL_SCALE) // mate in 0
#define MRS (9000 * EVAL_SCALE)  // where the mate range starts in positive eval

/// Retrieves the amount of moves until mate from the evaluation score, or 0 if no mate was found.
#define COUNT_MATE_IN_PLY(eval) ((eval < 0 ? -eval : eval) > MRS ? (-M0 - (eval < 0 ? -eval : eval)) : 0)
#define MATE_IN_PLY(moves)       (M0 + moves)

constexpr inline f32 fEval(i32 eval) {
  return eval / (EVAL_SCALE * (float)1);
}

constexpr inline i32 iEval(f32 eval) {
  return (i32)(eval * EVAL_SCALE);
}

// constexpr ternary solution using force inlined lambdas
// and a constexpr if statement
#define CONSTEXPR_TERNARY(cond, ifTrue, ifFalse) ([&]() __attribute__((always_inline)) { \
          if constexpr (cond) { \
            return (ifTrue); \
          } else { \
            return (ifFalse); \
          } \
        }()) \