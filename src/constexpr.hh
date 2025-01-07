#pragma once

#include "types.hh"

/* Utilities for working with constexpr, especially much bit manipulation */

constexpr u64 pdep_constexpr64(u64 x, u64 m) {
    u64 result = 0;
    for (u64 src_pos = 0, mask_pos = 0; mask_pos != 64; ++mask_pos) {
        if (m >> mask_pos & 1) {
            result |= (x >> src_pos++ & 1ULL) << mask_pos;
        }
    }

    return result;
}