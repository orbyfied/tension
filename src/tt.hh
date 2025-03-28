#pragma once

#include "types.hh"
#include "bitboard.hh"
#include "move.hh"
#include "board.hh"

namespace tc {

// Used when a position is has a sure evaluation independant of search depth,
// such as a checkmate
#define TT_SURE_DEPTH 999999

enum TTEntryType {
    TT_NULL = 0, TT_PV, TT_LOWER_BOUND, TT_UPPER_BOUND
};

/// @brief An entry in the transposition table
struct TTEntry {
    // PositionHash hash;
    TTEntryType type;
    u8 depth;  // The depth at which this entry was added/evaluated
    i32 score; // The ABSOLUTE evaluation at this depth
    union {
        Move move; // The best move in this position, as determined by the search, only available when type == EXACT
    } data;

    inline bool null() { return type == TT_NULL; }
    inline bool nn()   { return type != TT_NULL; }
};

/// @brief Heap-allocated hashtable containing cached evaluations for positions
struct TranspositionTable {
    TTEntry* data = nullptr;
    u64 capacity = 0;  // Must be a power of 2
    u32 indexMask = 0; // Computed from power of 2 capacity
    u64 used = 0;

    void alloc(u32 powerOf2);
    inline u64 index(Board* board);
    inline TTEntry* add(Board* board, TTEntryType type, i32 depth, i32 eval, /* should be removed if unused bc inlined */ bool* overwritten);
    inline TTEntry* get(Board* board);
};

forceinline u64 TranspositionTable::index(Board* board) {
    PositionHash hash = board->zhash();
    return hash & indexMask;
}

forceinline TTEntry* TranspositionTable::add(Board* board, TTEntryType type, i32 depth, i32 eval, /* should be removed if unused bc inlined */ bool* overwritten) {
    TTEntry* entry = &data[index(board)];
    if (entry->type != TT_NULL) {
        // check if we should overwrite this entry
        bool overwrite = entry->depth <= depth;
        if (!overwrite) {
            return nullptr; // dont overwrite this
        }

        *overwritten = true;
    } else {
        used++;
    }

    entry->type = type;
    entry->depth = (i16)depth;
    entry->score = eval;
    return entry;
}

inline TTEntry* TranspositionTable::get(Board* board) {
    return &data[index(board)];
}

}