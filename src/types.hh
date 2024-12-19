#pragma once

#include <stdint.h>

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

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

// constexpr ternary solution using force inlined lambdas
// and a constexpr if statement
#define CONSTEXPR_TERNARY(cond, ifTrue, ifFalse) ([&]() __attribute__((always_inline)) { \
          if constexpr (cond) { \
            return (ifTrue); \
          } else { \
            return (ifFalse); \
          } \
        }()) \