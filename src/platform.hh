#pragma once

#include "types.hh"

// Architecture

// ctz (count trailing zeroes), clz (count leading zeroes)
#define _ctz64(x) (__builtin_ctzll(x))
#define _clz64(x) (__builtin_clzll(x))
#define _ctz32(x) (__builtin_ctz(x))
#define _clz32(x) (__builtin_clz(x))
#define _ctz8(x) (__builtin_ctz((u32)(x)))
#define _clz8(x) (__builtin_clz((u32)(x) << /* shl to move to start for leading zero cnt */ 24))

// csb (count set bits AKA popcount), czb (count zero bits AKA width - popcount)
#define _csb64(x) (__builtin_popcountll(x))
#define _csb32(x) (__builtin_popcount(x))
#define _csb8(x)  (__builtin_popcount(x))
#define _czb64(x) (64 - __builtin_popcountll(x))
#define _czb32(x) (32 - __builtin_popcount(x))
#define _czb8(x)  (8 - __builtin_popcount(x))

// mbe (masked bit extract AKA pext), mbd (masked bit deposit AKA pdep)
#define _mbe(src, mask, dst) 
#define _mbd(src, mask, dst) 

// Compiler
#if defined(__clang__)

#define _forceinline __attribute__((always_inline))

#endif

// System