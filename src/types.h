#include <stdint.h>

#if defined(__clang__)

#endif

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

// constexpr ternary solution using force inlined lambdas
// and a constexpr if statement
#define CONSTEXPR_TERNARY(cond, ifTrue, ifFalse) ([&]() __attribute__((always_inline)) { \
          if constexpr (cond) { \
            return (ifTrue); \
          } else { \
            return (ifFalse); \
          } \
        }()) \