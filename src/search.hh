#pragma once

#include "board.hh"
#include "debug.hh"
#include "movegen.hh"
#include "evaldef.hh"

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
    virtual i32 eval(Board* board) = 0;
};

/// @brief Compile time search options
struct StaticSearchOptions {
    bool useTranspositionTable;

    bool maintainPV = true;

    // Debug and performance metrics
    bool debugMetrics;
};

struct SearchMetrics {
    u64 totalNodes = 0;
    u64 totalPrimaryNodes = 0;
    u64 totalLeafNodes = 0;
    u64 totalQuiescenceNodes = 0;
    u64 maxDepth = 0;
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
    Move move; // The move being currently evaluated
};

/// @brief Stack allocated search stack
struct SearchStack {
    SearchStackFrame data[MAX_DEPTH];
    u8 index = 0;

    forceinline SearchStackFrame* push();
    forceinline void pop();
    forceinline SearchStackFrame* get(u8 index);
    forceinline u8 size();
    forceinline SearchStackFrame* last() { return get(index - 1); }
    forceinline SearchStackFrame* first() { return get(0); }
    forceinline bool empty() { return index == 0; }
};

/// @brief Stack-like structure used to track the PV across a search if enabled.
struct PVStack {

};

/// @brief The state object for each fixed depth search
template<StaticSearchOptions const& _SearchOptions, typename _Evaluator>
struct SearchState {
    Board* board;
    _Evaluator* leafEval;

    TranspositionTable* transpositionTable = nullptr;

    u32 maxPrimaryDepth;
    SearchStack stack;

    /* Only when _SearchOptions.debugMetrics is enabled */
    SearchMetrics metrics;
};

/// @brief The thread local object for fixed depth searches=
template<StaticSearchOptions const& _SearchOptions>
struct ThreadSearchState {

};

/// @brief The state object for an iterative search
template<StaticSearchOptions const& _SearchOptions, typename _Evaluator>
struct IterativeSearchState {
    /// @brief The state to use for each fixed depth search
    SearchState<_SearchOptions, _Evaluator> searchState;

    /// @brief Whether to end the search on this iteration
    bool end = false;
};

struct SearchManager {
    /// @brief The current board being evaluated/searched.
    Board* board;

    /* Evaluator */
    Evaluator* leafEval;
    
    constexpr static StaticMovegenOptions standardMovegenOptions = { };

    /// @brief Start an iterative deepening search with the given search state.
    /// This is performed without multithreading
    /// @param state The search state.
    template<StaticSearchOptions const& _SearchOptions, typename _Evaluator>
    i32 search_iterative_sync(IterativeSearchState<_SearchOptions, _Evaluator>* state) {

    }
};

/// @brief Search the current position to the given fixed depth
/// @param state The search state
/// The top level stack frame created by the root call has to be popped by the caller.
template<StaticSearchOptions const& _SearchOptions, typename _Evaluator, Color turn>
i32 search_sync(SearchState<_SearchOptions, _Evaluator>* state, ThreadSearchState<_SearchOptions>* threadState, 
                i32 alpha, i32 beta, u16 depthRemaining) {
                                        
    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalNodes++;
        state->metrics.totalPrimaryNodes++;
    }

    /* push the stack frame, this stack frame is expected to be popped by the caller */
    SearchStackFrame* frame = state->stack.push();

    Board* board = state->board;

    constexpr i32 sign = -1 + 2 * turn; // the integer sign for the current turn, constexpr evaluated bc its a template arg
    
    // function to register the given eval to the tt
    auto addTT = [&](TTEntryType type, i32 depth, i32 eval) __attribute__((always_inline)) -> TTEntry* {
        if constexpr (!_SearchOptions.useTranspositionTable) {
            return nullptr;
        } 

        [[maybe_unused]] bool overwritten = false;
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

    const i32 currentPositiveDepth = state->maxPrimaryDepth - depthRemaining; // starts at 0

    // check for 50 move rule draw
    if (board->volatile_state()->rule50Ply >= 50) {
        if constexpr (_SearchOptions.debugMetrics) {
            state->metrics.rule50Draws += 1;
        }

        return EVAL_DRAW;
    }

    // check for king capture, should never occur during normal play
    if (board->kingIndexPerColor[!turn] == NULL_SQ) {
        return EVAL_WIN;
    }

    // check for material draw
    if (board->is_insufficient_material()) {
        if constexpr (_SearchOptions.debugMetrics) {
            state->metrics.insufficientMaterial += 1;
        }

        return EVAL_DRAW;
    }

    i32 oldAlpha = alpha;

    // transposition table lookup
    TTEntry* ttEntry = nullptr;
    if constexpr (_SearchOptions.useTranspositionTable) {
        // try lookup in tt
        ttEntry = state->transpositionTable->get(board);
        if (ttEntry->depth >= depthRemaining) {
            switch (ttEntry->type) {
                case TT_PV: {
                    if constexpr (_SearchOptions.debugMetrics) {
                        state->metrics.ttPvHit++;
                    }

                    frame->move = ttEntry->data.move;
                    return sign * ttEntry->score;
                } break;

                case TT_LOWER_BOUND: alpha = std::max(alpha, ttEntry->score); break;
                case TT_UPPER_BOUND: beta = std::min(beta, ttEntry->score); break;
                default: break; // also covers TT_NULL
            }

            if (alpha >= beta) {
                if constexpr (_SearchOptions.debugMetrics) {
                    state->metrics.prunes++;
                    state->metrics.ttPrunes++;
                }

                return beta;
            }
        }
    }
    
    // initialize move picker
    MoveSupplier moveSupplier(board);

    // check for hash moves, we can cut movegen if this move
    // cuts this node with pruning
    if (_SearchOptions.useTranspositionTable && ttEntry) {
        if (board->check_pseudo_legal<turn>(ttEntry->data.move)) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metrics.ttHashMoves++;
            }

            moveSupplier.init_tt(ttEntry);
        }
    }

    // track the best known move and its eval
    i32 bestEval = EVAL_NEGATIVE_INFINITY;
    Move bestMove = NULL_MOVE;

    i32 legalMoves = 0;

    /* main move search loop */
    while (moveSupplier.has_next()) {
        Move move = moveSupplier.next_move<turn>();
        if (move.null()) continue;

        ExtMove<true> extMove(move);
        board->make_move_unchecked<turn, true>(&extMove);

        // check if the position is legal after making the move,
        // to do this we just have to do the inexpensive test of being in check
        if (board->is_in_check<turn>()) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metrics.illegal += 1;
            }

            board->unmake_move_unchecked<turn, true>(&extMove);
            continue;
        }

        if constexpr (_SearchOptions.debugMetrics) {
            if (move.captured_piece(board) != NULL_PIECE) {
                state->metrics.captures += 1;
            }
        }

        frame->move = move;

        u16 nextDepth = depthRemaining - 1;

        // register legal move
        legalMoves++;

        // perform search on move
        i32 evalForUs;
        if (nextDepth == 0) {
            // evaluate leaf
            evalForUs = -qsearch_root<_SearchOptions, _Evaluator, !turn>(state, threadState, -beta, -alpha, currentPositiveDepth + 1);
        } else {
            // continue search at next depth
            evalForUs = -search_sync<_SearchOptions, _Evaluator, !turn>(state, threadState, -beta, -alpha, nextDepth);
            state->stack.pop();
        }

        if (evalForUs > bestEval) {
            bestMove = move;
            bestEval = evalForUs;
        }

        // check for new alpha
        if (evalForUs > alpha) {
            alpha = evalForUs;

            // alpha-beta fail high
            if (alpha >= beta) {
                if constexpr (_SearchOptions.debugMetrics) {
                    state->metrics.prunes++;
                    if (moveSupplier.stage == CAPTURES_INIT) { // last move was a special/hash move
                        state->metrics.ttHashMovePrunes++;
                    }
                }

                if constexpr (_SearchOptions.useTranspositionTable) {
                    // create lower bound entry
                    TTEntry* entry = addTT(TT_LOWER_BOUND, depthRemaining, alpha);
                    if (entry) {
                        entry->data.move = move;
                    }
                }

                // unmake move
                board->unmake_move_unchecked<turn, true>(&extMove);
                return beta;
            }
        }

        // unmake move
        board->unmake_move_unchecked<turn, true>(&extMove);
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
    }

    // evaluate stalemate or checkmate
    if (legalMoves == 0) {
        // evaluate checkmate
        if (board->is_in_check<turn>())  {
            if (_SearchOptions.debugMetrics) {
                state->metrics.checkmates += 1;
            }

            i32 eval = MATED_IN_PLY(/* current positive depth */ currentPositiveDepth);
            if constexpr (_SearchOptions.useTranspositionTable) {
                addTT(TT_PV, TT_SURE_DEPTH, eval);
            }

            return eval;
        }

        if (_SearchOptions.debugMetrics) {
            state->metrics.stalemates += 1;
        }

        // return stalemate
        i32 eval = EVAL_DRAW;
        if constexpr (_SearchOptions.useTranspositionTable) {
            addTT(TT_PV, TT_SURE_DEPTH, eval);
        }

        return eval;
    }   
    
    // store evaluation and move in tt
    if constexpr (_SearchOptions.useTranspositionTable) {
        const TTEntryType type = alpha <= oldAlpha ? TT_UPPER_BOUND : TT_PV;
        TTEntry* entry = addTT(type, depthRemaining, alpha);
        if (entry) {
            entry->data.move = bestMove;
        }
    }

    frame->move = bestMove;
    return bestEval;
}

/// @brief Root node quesience search, used when depth 0 is reached in the main search
template<StaticSearchOptions const& _SearchOptions, typename _Evaluator, Color turn>
i32 qsearch_root(SearchState<_SearchOptions, _Evaluator>* state, ThreadSearchState<_SearchOptions>* threadState, 
                 i32 alpha, i32 beta, i32 positiveDepth) {

    return qsearch<_SearchOptions, _Evaluator, turn>(state, threadState, alpha, beta, positiveDepth);
}

/// @brief Quesience search, used when depth 0 is reached in the main search
template<StaticSearchOptions const& _SearchOptions, typename _Evaluator, Color turn>
i32 qsearch(SearchState<_SearchOptions, _Evaluator>* state, ThreadSearchState<_SearchOptions>* threadState, 
            i32 alpha, i32 beta, i32 positiveDepth) {

    Board* board = state->board;

    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalNodes++;
        state->metrics.totalQuiescenceNodes++;

        if (positiveDepth > state->metrics.maxDepth) {
            state->metrics.maxDepth = positiveDepth;
        }
    }

    // generate and iterate captures
    MoveList<NoOrderMoveOrderer, MAX_MOVES> moveList;
    gen_all_moves<decltype(moveList), movegenCapturesPL, turn>(board, &moveList);

    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalPseudoLegal += moveList.count;
    }

    // iterate legal captures
    i32 bestEval = 0;
    i32 legalMoves = 0;
    for (i32 i = moveList.count - 1; i >= 0; i--) {
        Move move = moveList.get_move(i);
        if (move.null()) continue;

        ExtMove<true> extMove(move);
        board->make_move_unchecked<turn, true>(&extMove);

        // check whether the move is legal
        if (board->is_in_check<turn>()) {
            board->unmake_move_unchecked<turn, true>(&extMove);
            continue;
        }

        legalMoves++;

        // perform deeper qsearch
        i32 eval = -qsearch<_SearchOptions, _Evaluator, !turn>(state, threadState, -beta, -alpha, positiveDepth + 1);
        if (eval > bestEval) {
            bestEval = eval;
        }

        board->unmake_move_unchecked<turn, true>(&extMove);

        if (eval > alpha) {
            alpha = eval;
            if (alpha > beta) {
                return beta;
            }
        }
    }

    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalLegalMoves += legalMoves;
    }

    if (legalMoves > 0) {
        return bestEval;
    }

    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalLeafNodes++;
    }

    // no legal captures, check for legal quiets to find checkmate
    moveList.reset();
    gen_all_moves<decltype(moveList), movegenQuietsPL, turn>(board, &moveList);

    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalPseudoLegal += moveList.count;
    }

    for (i32 i = moveList.count - 1; i >= 0; i--) {
        Move move = moveList.get_move(i);
        if (move.null()) continue;

        ExtMove<true> extMove(move);
        board->make_move_unchecked<turn, true>(&extMove);

        // check whether the move is legal
        if (!board->is_in_check<turn>()) {
            legalMoves++;
        }
        
        board->unmake_move_unchecked<turn, true>(&extMove);
    }

    if (legalMoves == 0) {
        if (board->is_in_check<turn>()) {
            return MATED_IN_PLY(/* current positive depth */ positiveDepth);
        }

        return EVAL_DRAW;
    }

    if constexpr (_SearchOptions.debugMetrics) {
        state->metrics.totalLegalMoves += legalMoves;
    }
    
    // statically evaluate position
    constexpr i32 sign = turn == WHITE ? 1 : -1;

    _Evaluator* eval = state->leafEval;
    return sign * eval->eval(state->board);
}

inline u8 SearchStack::size() {
    return index;
}

forceinline SearchStackFrame* SearchStack::push() {
    return &data[index++];
}

forceinline void SearchStack::pop() {
    index--;
}

forceinline SearchStackFrame* SearchStack::get(u8 index) {
    return &data[index];
}

/* Debug */
template<StaticSearchOptions const& _SearchOptions, typename _Evaluator>
static void debug_tostr_search_metrics(std::ostream& os, SearchState<_SearchOptions, _Evaluator>* state) {
    os << "[Search Metrics]\n";
    os << " Total Nodes Searched: " << state->metrics.totalNodes << "\n";
    os << " Total Primary Nodes: " << state->metrics.totalPrimaryNodes << "\n";
    os << " Total Quiescence Nodes: " << state->metrics.totalQuiescenceNodes << "\n";
    os << " Total Leaf Nodes Searched: " << state->metrics.totalLeafNodes << "\n";
    os << " Max Depth: " << state->metrics.maxDepth << "\n";
    os << " Prunes: " << state->metrics.prunes << "\n";
    os << " Captures: " << state->metrics.captures << "\n";
    os << " Checks: " << state->metrics.checks << "\n";
    os << " Double Checks: " << state->metrics.doubleChecks << "\n";
    os << " Checkmates: " << state->metrics.checkmates << "\n";
    os << " Stalemates: " << state->metrics.stalemates << "\n";
    os << " Insufficient Material: " << state->metrics.insufficientMaterial << "\n";
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