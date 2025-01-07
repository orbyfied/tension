#pragma once

#include <bit>
#include <immintrin.h>

#include "types.hh"

// Compiler
#if defined(__clang__)

#define _forceinline __attribute__((always_inline))

#endif

// Architecture
#if defined(__clang__) || defined(__gcc__)

// ctz (count trailing zeroes), clz (count leading zeroes)
#define _ctz64(x) (__builtin_ctzll(x))
#define _clz64(x) (__builtin_clzll(x))
#define _ctz32(x) (__builtin_ctz(x))
#define _clz32(x) (__builtin_clz(x))
#define _ctz8(x) (__builtin_ctz((u32)(x)))
#define _clz8(x) (__builtin_clz((u32)(x) << /* shl to move to start for leading zero cnt */ 24))

// csb (count set bits AKA popcount), czb (count zero bits AKA width - popcount)
#define _popcount64(x) (__builtin_popcountll(x))
#define _popcount32(x) (__builtin_popcount(x))
#define _popcount8(x)  (__builtin_popcount(x))
#define _zcount64(x) (64 - __builtin_popcountll(x))
#define _zcount32(x) (32 - __builtin_popcount(x))
#define _zcount8(x)  (8 - __builtin_popcount(x))

#define _pext(src, mask) _pext_u64(src, mask) 
#define _pdep(src, mask) _pdep_u64(src, mask)

inline u8 _pop_lsb(u64& i) {
    const u8 idx = _ctz64(i);
    i &= i - 1;
    return idx;
}

#endif

// System