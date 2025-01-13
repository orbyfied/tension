#pragma once

#include "board.hh"
#include "debug.hh"
#include "movegen.hh"
#include "evaldef.h"

namespace tc {

/// @brief Additional data about the current position provided to the evaluator by the search,
/// also used to return additional information back to the search algorithm by the evaluator.
struct EvalData {
    /* Inputs */
    i32 legalMoveCount;

    /* Outputs */
};

/// @brief Static position evaluator.
struct Evaluator {
    virtual i32 eval(Board* board, EvalData* data) = 0;
};

// Used when a position is has a sure evaluation independant of search depth,
// such as a checkmate
#define TT_SURE_DEPTH 999999

enum TTEntryType {
    TT_NULL = 0, TT_PV, TT_LOWER_BOUND, TT_UPPER_BOUND
};

/// @brief An entry in the transposition table
struct TTEntry {
    PositionHash hash;
    TTEntryType type;
    i16 depth; // The depth at which this entry was added/evaluated
    i32 score;  // The ABSOLUTE evaluation at this depth
    union {
        Move move; // The best move in this position, as determined by the search, only available when type == EXACT
    } data;
};

/// @brief Heap-allocated hashtable containing cached evaluations for positions
struct TranspositionTable {
    TTEntry* data = nullptr;
    u64 capacity = 0;
    u64 used = 0;

    void alloc(u32 entryCount);
    inline u64 index(Board* board);
    inline TTEntry* add(Board* board, TTEntryType type, i32 depth, i32 eval, /* should be removed if unused bc inlined */ bool* overwritten);
    inline TTEntry* get(Board* board);
};

/// @brief The best move as a result of a search and it's signed evaluation
/// If this is a leaf node, the move will be a null move and the evaluation will be
/// the static evaluation of the position.
struct SearchEvalResult {
    Move move = NULL_MOVE;
    i32 eval;

    inline bool null() { return eval == ERR_EVAL; }
};

inline SearchEvalResult make_eval(i32 eval) {
    return { .move = NULL_MOVE, .eval = eval };
}

inline SearchEvalResult null_eval() {
    return { .eval = ERR_EVAL };
}

/// @brief Compile time search options
struct StaticSearchOptions {
    bool useTranspositionTable;
    bool useMoveEvalTable;

    bool maintainStack;

    // Debug and performance metrics
    bool debugMetrics;
};

struct SearchMetrics {
    u64 totalNodes = 0;
    u64 totalLeafNodes = 0;
    u64 prunes = 0;
    u64 ttPrunes = 0;

    u64 checks = 0;
    u64 doubleChecks = 0;
    u64 checkmates = 0;
    u64 stalemates = 0;
    u64 captures = 0;
    u64 rule50Draws = 0;
    u64 insufficientMaterial = 0;

    u64 illegal = 0;
    u64 totalPseudoLegal = 0;
    u64 totalLegalMoves = 0;

    u64 ttPvHit = 0;
    u64 ttWrites = 0;
    u64 ttOverwrites = 0;
    u64 ttHashMoves = 0;
    u64 ttHashMovePrunes = 0;
};

#define MAX_DEPTH 64

/// @brief The stack frame for a node
struct SearchStackFrame {
    const Move* move; // The move being currently evaluated
};

/// @brief Stack allocated search stack
struct SearchStack {
    SearchStackFrame data[MAX_DEPTH];
    u8 index = 0;

    inline SearchStackFrame* push();
    inline void pop();
    inline SearchStackFrame* get(u8 index);
    inline u8 size();
    inline SearchStackFrame* last() { return get(index - 1); }
    inline SearchStackFrame* first() { return get(0); }
    inline bool empty() { return index == 0; }
};

/// @brief The state object for each fixed depth search
template<StaticSearchOptions const& _SearchOptions>
struct SearchState {
    TranspositionTable* transpositionTable = nullptr;
    MoveEvalTable* moveEvalTable = nullptr;

    SearchStack stack;

    /* Only when _SearchOptions.debugMetrics is enabled */
    SearchMetrics metrics;
};

/// @brief The thread local object for fixed depth searches=
template<StaticSearchOptions const& _SearchOptions>
struct ThreadSearchState {

};

/// @brief The state object for an iterative search
template<StaticSearchOptions const& _SearchOptions>
struct IterativeSearchState {
    /// @brief The state to use for each fixed depth search
    SearchState<_SearchOptions> searchState;

    /// @brief Whether to end the search on this iteration
    bool end = false;
};

struct SearchManager {
    /// @brief The current board being evaluated/searched.
    Board* board;

    /* Evaluator */
    Evaluator* leafEval;
    
    constexpr static StaticMovegenOptions standardMovegenOptions = { };

    /// @brief Search the current position to the given fixed depth
    /// @param state The search state
    template<StaticSearchOptions const& _SearchOptions, Color turn, bool leaf /* depth = 0 */>
    SearchEvalResult search_fixed_internal_sync(SearchState<_SearchOptions>* state, ThreadSearchState<_SearchOptions>* threadState, 
                                                i32 alpha, i32 beta, u16 maxDepth, u16 depthRemaining) {
                                            
        if constexpr (_SearchOptions.debugMetrics) {
            state->metrics.totalNodes += 1;
        }

        constexpr i32 sign = -1 + 2 * turn; // the integer sign for the current turn, constexpr evaluated bc its a template arg
        
        // function to register the given eval to the tt
        auto addTT = [&](TTEntryType type, i32 depth, i32 eval) __attribute__((always_inline)) -> TTEntry* {
            if constexpr (!_SearchOptions.useTranspositionTable) {
                return nullptr;
            } 

            bool overwritten = false;
            TTEntry* entry = state->transpositionTable->add(board, type, depth, eval, &overwritten);

            if (_SearchOptions.debugMetrics && entry) {
                state->metrics.ttWrites++;
                if (overwritten) {
                    state->metrics.ttOverwrites++;
                }
            }

            return entry;
        };

        // function to convert the local eval into an absolute eval
        auto absEval = [&](i32 eval) __attribute__((always_inline)) {
            return sign * eval;
        };
 
        const i32 nextDepth = depthRemaining - 1;
        const i32 currentPositiveDepth = maxDepth - depthRemaining; // starts at 0

        // check for 50 move rule draw
        if (board->volatile_state()->rule50Ply >= 50) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metrics.rule50Draws += 1;
            }

            return make_eval(EVAL_DRAW);
        }

        // check for king capture, should never occur during normal play
        if (board->kingIndexPerColor[!turn] == NULL_SQ) {
            return make_eval(EVAL_WIN);
        } else if (board->kingIndexPerColor[turn] == NULL_SQ) {
            return make_eval(EVAL_LOSS);
        }

        // check for material draw
        if (board->is_insufficient_material()) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metrics.insufficientMaterial += 1;
            }

            return make_eval(EVAL_DRAW);
        }

        i32 oldAlpha = alpha;

        // transposition table lookup
        TTEntry* ttEntry = nullptr;
        if constexpr (_SearchOptions.useTranspositionTable) {
            // try lookup in tt
            ttEntry = state->transpositionTable->get(board);
            if (ttEntry && ttEntry->depth >= depthRemaining) {
                switch (ttEntry->type) {
                    case TT_PV: {
                        if constexpr (_SearchOptions.debugMetrics) {
                            state->metrics.ttPvHit++;
                        }

                        return { .move = ttEntry->data.move, .eval = sign * ttEntry->score };
                    } break;

                    case TT_LOWER_BOUND: alpha = std::max(alpha, ttEntry->score); break;
                    case TT_UPPER_BOUND: beta = std::min(beta, ttEntry->score); break;
                    default: break;
                }

                if (alpha >= beta) {
                    if constexpr (_SearchOptions.debugMetrics) {
                        state->metrics.prunes++;
                        state->metrics.ttPrunes++;
                    }

                    return make_eval(beta);
                }
            }
        }

        SearchStackFrame* frame = state->stack.push();

        // dynamically create the move list type,
        // we dont want to waste performance ordering moves if its a leaf node
        auto MakeMoveList = [&]() {
            if constexpr (leaf) {
                return MoveList<NoOrderMoveOrderer, MAX_MOVES, false> { }; 
            } else {
                return MoveList<BasicScoreMoveOrderer, MAX_MOVES, _SearchOptions.useMoveEvalTable> {  };
            }
        };

        constexpr static StaticMovegenOptions MovegenOptions = defaultFullyLegalMovegenOptions;
        
        // initialize move list with ordering
        auto moveList = MakeMoveList();

        // track the best known move and its eval
        Move bestMove;

        i32 legalMoves = 0;

        /* main move search function, if the return value isnt a null result
           the result is immediately returned from the function */
        auto searchMove = [&](const Move move) __attribute__((always_inline)) -> SearchEvalResult {
            if constexpr (_SearchOptions.debugMetrics) {
                if (move.captured_piece(board) != NULL_PIECE) {
                    state->metrics.captures += 1;
                }
            }

            ExtMove<true> extMove(move);
            board->make_move_unchecked<turn, true>(&extMove);

            // check if the position is legal

            // test if we left our own king in check, should happen rarely but 
            // the moves are still pseudo legal so we have to check to avoid performing
            // a search on an illegal move wasting performance. should be relatively 
            // inexpensive due to the tracking
            if (board->is_in_check<turn>()) {
                if constexpr (_SearchOptions.debugMetrics) {
                    state->metrics.illegal += 1;
                }

                board->unmake_move_unchecked<turn, true>(&extMove);
                return null_eval();
            }

            frame->move = &move;

            // register legal move
            legalMoves++;

            if constexpr (!leaf) {
                // perform search on move
                SearchEvalResult result;
                if (nextDepth == 0) {
                    // evaluate leaf
                    result = search_fixed_internal_sync<_SearchOptions, !turn, true>(state, threadState, -beta, -alpha, maxDepth, 0);
                } else {
                    // continue search at next depth
                    result = search_fixed_internal_sync<_SearchOptions, !turn, false>(state, threadState, -beta, -alpha, maxDepth, nextDepth);
                }

                i32 evalForUs = -result.eval;

                /* [-] remember eval from this move in the move table */
                if constexpr (_SearchOptions.useMoveEvalTable) {
                    state->moveEvalTable->add(move, evalForUs / 1000);
                }

                // check for new alpha
                if (evalForUs > alpha) {
                    alpha = evalForUs;
                    bestMove = move;
                }

                // alpha-beta pruning
                if (alpha >= beta) {
                    if constexpr (_SearchOptions.debugMetrics) {
                        state->metrics.prunes++;
                    }

                    // create lower bound entry
                    TTEntry* entry = addTT(TT_LOWER_BOUND, depthRemaining, alpha);
                    if (entry) {
                        entry->data.move = move;
                    }

                    // unmake move
                    board->unmake_move_unchecked<turn, true>(&extMove);
                    return make_eval(beta);
                } 
            }

            // unmake move
            board->unmake_move_unchecked<turn, true>(&extMove);
            return null_eval();
        };

        // check for hash moves, we can cut movegen if this move
        // cuts this node with pruning
        if constexpr (!leaf) {
            moveList.moveEvalTable = state->moveEvalTable;
            
            if (ttEntry && ttEntry->type == TT_PV) {
                if (board->check_trivial_validity<turn>(ttEntry->data.move)) {
                    if constexpr (_SearchOptions.debugMetrics) {
                        state->metrics.ttHashMoves++;
                    }

                    SearchEvalResult res = searchMove(ttEntry->data.move);
                    if (!res.null()) {
                        if constexpr (_SearchOptions.debugMetrics) {
                            state->metrics.ttHashMovePrunes++;
                        }

                        state->stack.pop();
                        return res;
                    }
                }
            }
        }

        // generate pseudo legal moves
        gen_all_moves<decltype(moveList), MovegenOptions, turn>(board, &moveList);
        moveList.template sort_moves<turn>(board);

        if constexpr (_SearchOptions.debugMetrics) {
            state->metrics.totalPseudoLegal += moveList.count;
        }

        // search generated moves
        for (int i = moveList.count; i >= 0; i--) {
            const Move move = moveList.get_move(i);
            if (move.null()) continue;

            SearchEvalResult res = searchMove(move);
            if (!res.null()) {
                state->stack.pop();
                return res;
            }
        }

        if constexpr (_SearchOptions.debugMetrics) {
            state->metrics.totalLegalMoves += legalMoves;
        }

        // evaluate checks and checkmate
        if (board->is_in_check<turn>()) {
            if (_SearchOptions.debugMetrics) {
                state->metrics.checks += 1;

                if (_popcount64(board->checkers(turn)) >= 2) {
                    state->metrics.doubleChecks += 1;
                }
            }

            if (legalMoves == 0) {
                if (_SearchOptions.debugMetrics) {
                    state->metrics.checkmates += 1;
                }

                i32 eval = MATED_IN_PLY(/* current positive depth */ currentPositiveDepth);
                if constexpr (_SearchOptions.useTranspositionTable) {
                    addTT(TT_PV, TT_SURE_DEPTH, eval);
                }

                state->stack.pop();
                return make_eval(eval);
            }
        }

        // evaluate stalemate
        if (legalMoves == 0) {
            if (_SearchOptions.debugMetrics) {
                state->metrics.stalemates += 1;
            }

            i32 eval = EVAL_DRAW;
            if constexpr (_SearchOptions.useTranspositionTable) {
                addTT(TT_PV, TT_SURE_DEPTH, eval);
            }

            state->stack.pop();
            return make_eval(eval);
        }

        // statically evaluate leaf node
        if constexpr (leaf) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metrics.totalLeafNodes += 1;
            }

            EvalData evalData;
            evalData.legalMoveCount = legalMoves;

            // evaluate leaf node and return result
            i32 eval = sign * leafEval->eval(board, &evalData);
            alpha = eval;
        }

finalize:
        state->stack.pop();
        if constexpr (_SearchOptions.useTranspositionTable) {
            const TTEntryType type = alpha <= oldAlpha ? TT_UPPER_BOUND : TT_PV;
            TTEntry* entry = addTT(type, depthRemaining, alpha);
            if (entry) {
                entry->data.move = bestMove;
            }
        }

        return { .move = bestMove, .eval = alpha };
    }

    /// @brief Start an iterative deepening search with the given search state.
    /// This is performed without multithreading
    /// @param state The search state.
    /// The best move found and it's evaluation is saved to the search state.
    template<StaticSearchOptions const& _SearchOptions>
    SearchEvalResult search_iterative_sync(IterativeSearchState<_SearchOptions>* state) {
        // create sync thread state
        ThreadSearchState<_SearchOptions> threadState;

        // start at depth 1
        u16 depth = 1;

        // perform minimum depth search
        SearchEvalResult bestMoveResult;
        if (board->turn) bestMoveResult = search_fixed_internal_sync<_SearchOptions, true, false>(state->searchState, &threadState, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth, depth);
        else bestMoveResult = search_fixed_internal_sync<_SearchOptions, false, false>(state->searchState, &threadState, EVAL_NEGATIVE_INFINITY, EVAL_POSITIVE_INFINITY, depth, depth);

        // then iteratively go 2 and higher
        while (!state->end) {
            // increment depth
            depth++;

            // reset cached search states

            // perform search
            if (board->turn) bestMoveResult = search_fixed_internal_sync<_SearchOptions, true, false>(state->searchState, &threadState, -99999999, 99999999, depth, depth);
            else bestMoveResult = search_fixed_internal_sync<_SearchOptions, false, false>(state->searchState, &threadState, -99999999, 99999999, depth, depth);

            // update optimization for next search
        }

        return bestMoveResult;
    }
};

inline u8 SearchStack::size() {
    return index;
}

inline SearchStackFrame* SearchStack::push() {
    return &data[index++];
}

inline void SearchStack::pop() {
    index--;
}

inline SearchStackFrame* SearchStack::get(u8 index) {
    return &data[index];
}

inline u64 TranspositionTable::index(Board* board) {
    PositionHash hash = board->zhash();
    // std::cout << std::bitset<64>(hash) << "\n";
    return hash % capacity;
}

inline TTEntry* TranspositionTable::add(Board* board, TTEntryType type, i32 depth, i32 eval, /* should be removed if unused bc inlined */ bool* overwritten) {
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
    TTEntry* entry = &data[index(board)];
    if (entry->type == TT_NULL) return nullptr;
    return entry;
}

/* Debug */
template<StaticSearchOptions const& _SearchOptions>
static void debug_tostr_search_metrics(std::ostream& os, SearchState<_SearchOptions>* state) {
    os << "[Search Metrics]\n";
    os << " Total Nodes Searched: " << state->metrics.totalNodes << "\n";
    os << " Total Leaf Nodes Searched: " << state->metrics.totalLeafNodes << "\n";
    os << " Prunes: " << state->metrics.prunes << "\n";
    os << " Captures: " << state->metrics.captures << "\n";
    os << " Checks: " << state->metrics.checks << "\n";
    os << " Double Checks: " << state->metrics.doubleChecks << "\n";
    os << " Checkmates: " << state->metrics.checkmates << "\n";
    os << " Stalemates: " << state->metrics.stalemates << "\n";
    os << " Pseudo-legal generated: " << state->metrics.totalPseudoLegal << "\n";
    os << " Total legal moves iterated: " << state->metrics.totalLegalMoves << "\n";
    os << " Illegal Discarded: " << state->metrics.illegal << "\n";
    if constexpr (_SearchOptions.useTranspositionTable) {
        os << " TT PV Hit: " << state->metrics.ttPvHit << "\n";
        os << " TT Writes: " << state->metrics.ttWrites << "\n";
        os << " TT Overwrites: " << state->metrics.ttOverwrites << " (" << (state->metrics.ttWrites > 0 ? (((float)state->metrics.ttOverwrites / (float)state->metrics.ttWrites) * 100) : 0) << "%)\n";  
        os << " TT Used: " << state->transpositionTable->used << " (" << (((float)(state->transpositionTable->used) / (float)(state->transpositionTable->capacity)) * 100) << "% full)\n";
        os << " TT Hash Move Hits: " << state->metrics.ttHashMoves << " (" << state->metrics.ttHashMovePrunes << " prunes)\n";
    }
}

}