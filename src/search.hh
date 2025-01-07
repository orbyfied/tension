#pragma once

#include "board.hh"
#include "debug.hh"
#include "movegen.hh"

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

/// @brief Heap-allocated hashtable containing cached evaluations for positions
struct TranspositionTable {
    void add(Board* board, i32 depth, i32 eval);
};

/// @brief The best move as a result of a search and it's signed evaluation
/// If this is a leaf node, the move will be a null move and the evaluation will be
/// the static evaluation of the position.
struct MoveResult {
    Move move = NULL_MOVE;
    i32 eval;
};

/// @brief Compile time search options
struct StaticSearchOptions {
    bool useTranspositionTable;
    bool useMoveEvalTable;

    // Debug and performance metrics
    bool debugMetrics;
};

/// @brief The state object for each fixed depth search
template<StaticSearchOptions const& _SearchOptions>
struct SearchState {
    TranspositionTable* transpositionTable;
    MoveEvalTable* moveEvalTable;

    /* Only when _SearchOptions.debugMetrics is enabled */
    u64 metricTotalNodes = 0;
    u64 metricTotalLeafNodes = 0;
    u64 metricChecks = 0;
    u64 metricCheckmates = 0;
    u64 metricStalemates = 0;
    u64 metricCaptures = 0;
    u64 metricIllegal = 0;
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
    MoveResult search_fixed_internal_sync(SearchState<_SearchOptions>* state, ThreadSearchState<_SearchOptions>* threadState, 
                                          i32 alpha, i32 beta, u16 maxDepth, u16 depthRemaining) {
        if constexpr (_SearchOptions.debugMetrics) {
            state->metricTotalNodes += 1;
        }

        const i32 nextDepth = depthRemaining - 1;
        const i32 currentPositiveDepth = maxDepth - depthRemaining; // starts at 0
        
        // generate pseudo legal moves in the position
        //  rn we do this for leaf nodes as well because it is currently how we
        //  evaluate check-/stalemates, although preferably we wouldnt have to do this.
        MoveList<BasicScoreMoveOrderer, MAX_MOVES, true> moveList;
        gen_all_moves<decltype(moveList), standardMovegenOptions, turn>(board, &moveList);

        constexpr i32 sign = -1 + 2 * turn; // the integer sign for the current turn, constexpr evaluated bc its a template arg

        // track the best known move and its eval
        Move bestMove;
        i32 bestEval = 0; 

        // iterate over list
        i32 legalMoves = 0;
        for (int i = 0; i < moveList.reserved + moveList.count; i++) {
            Move move = moveList.moves[i];
            if (move.null()) continue;

            if constexpr (_SearchOptions.debugMetrics) {
                if (move.captured_piece(board) != NULL_PIECE) {
                    state->metricCaptures += 1;
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
                    state->metricIllegal += 1;
                }

                board->unmake_move_unchecked<turn, true>(&extMove);
                continue;
            }

            // register legal move
            legalMoves++;

            if constexpr (!leaf) {
                // perform search on move
                MoveResult result;
                if (nextDepth == 0) {
                    // evaluate leaf
                    result = sign * search_fixed_internal_sync<!turn, true>(state, threadState, -alpha, -beta, maxDepth, 0);
                } else {
                    // continue search at next depth
                    result = sign * search_fixed_internal_sync<!turn, false>(state, threadState, -alpha, -beta, maxDepth, nextDepth);
                }

                i32 evalForUs = -result.eval;

                /* [-] remember eval from this move in the move table */
                if constexpr (_SearchOptions.useMoveEvalTable) {
                    state->moveEvalTable->add(move, evalForUs);
                }

                // alpha-beta pruning
                if (evalForUs >= beta) {
                    return { .move = NULL_MOVE, .eval = beta };
                } 

                alpha = MAX(evalForUs, alpha);
            }

            // unmake move
            board->unmake_move_unchecked<turn, true>(&extMove);
        }

        // evaluate checkmate or stalemate
        if (legalMoves == 0) {
            if (board->is_in_check<turn>()) {
                if (_SearchOptions.debugMetrics) {
                    state->metricCheckmates += 1;
                }

                return { .eval = MATE_IN_PLY(/* current positive depth */ currentPositiveDepth) };
            }

            if (_SearchOptions.debugMetrics) {
                state->metricStalemates += 1;
            }

            return { .eval = DRAW_EVAL };
        }

        // statically evaluate leaf node
        if constexpr (leaf) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metricTotalLeafNodes += 1;
            }

            EvalData evalData;
            evalData.legalMoveCount = legalMoves;

            // evaluate leaf node and return result
            i32 eval = leafEval->eval(board, &evalData);
            return { .move = NULL_MOVE, .eval = eval };
        }

        return { .move = NULL_MOVE, .eval = 0 };
    }

    /// @brief Start an iterative deepening search with the given search state.
    /// This is performed without multithreading
    /// @param state The search state.
    /// The best move found and it's evaluation is saved to the search state.
    template<StaticSearchOptions const& _SearchOptions>
    MoveResult search_iterative_sync(IterativeSearchState<_SearchOptions>* state) {
        // create sync thread state
        ThreadSearchState<_SearchOptions> threadState;

        // start at depth 1
        u16 depth = 1;

        // perform minimum depth search
        MoveResult bestMoveResult;
        if (board->whiteTurn) bestMoveResult = search_fixed_internal_sync<_SearchOptions, true, false>(state->searchState, &threadState, -9999999, 9999999, depth, depth);
        else bestMoveResult = search_fixed_internal_sync<_SearchOptions, false, false>(state->searchState, &threadState, -9999999, 9999999, depth, depth);

        // then iteratively go 2 and higher
        while (!state->end) {
            // increment depth
            depth++;

            // reset cached search states

            // perform search
            if (board->whiteTurn) bestMoveResult = search_fixed_internal_sync<_SearchOptions, true, false>(state->searchState, &threadState, -9999999, 9999999, depth, depth);
            else bestMoveResult = search_fixed_internal_sync<_SearchOptions, false, false>(state->searchState, &threadState, -9999999, 9999999, depth, depth);

            // update optimization for next search
        }

        return bestMoveResult;
    }
};

}