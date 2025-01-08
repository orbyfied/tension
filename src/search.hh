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

/// @brief Heap-allocated hashtable containing cached evaluations for positions
struct TranspositionTable {
    void add(Board* board, i32 depth, i32 eval);
};

/// @brief The best move as a result of a search and it's signed evaluation
/// If this is a leaf node, the move will be a null move and the evaluation will be
/// the static evaluation of the position.
struct SearchEvalResult {
    Move move = NULL_MOVE;
    i32 eval;
};

inline SearchEvalResult make_leaf_eval(i32 eval) {
    return { .move = NULL_MOVE, .eval = eval };
}

inline SearchEvalResult err_eval() {
    return { .move = NULL_MOVE, .eval = ERR_EVAL };
}

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
    TranspositionTable* transpositionTable = nullptr;
    MoveEvalTable* moveEvalTable = nullptr;

    /* Only when _SearchOptions.debugMetrics is enabled */
    u64 metricTotalNodes = 0;
    u64 metricTotalLeafNodes = 0;
    u64 metricChecks = 0;
    u64 metricCheckmates = 0;
    u64 metricStalemates = 0;
    u64 metricCaptures = 0;
    u64 metricIllegal = 0;
    u64 metricRule50Draws = 0;
    u64 metricInsufficientMaterial = 0;
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
            state->metricTotalNodes += 1;
        }

        const i32 nextDepth = depthRemaining - 1;
        const i32 currentPositiveDepth = maxDepth - depthRemaining; // starts at 0

        // check for 50 move rule draw
        if (board->volatile_state()->rule50Ply >= 50) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metricRule50Draws += 1;
            }

            return make_leaf_eval(EVAL_DRAW);
        }

        // check for king capture, should never occur during normal play
        if (board->kingIndexPerColor[!turn] == NULL_SQ) {
            return make_leaf_eval(EVAL_WIN);
        } else if (board->kingIndexPerColor[turn] == NULL_SQ) {
            return make_leaf_eval(EVAL_LOSS);
        }

        // check for material draw
        if (board->is_insufficient_material()) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metricInsufficientMaterial += 1;
            }

            return make_leaf_eval(EVAL_DRAW);
        }
        
        // generate pseudo legal moves in the position
        //  rn we do this for leaf nodes as well because it is currently how we
        //  evaluate check-/stalemates, although preferably we wouldnt have to do this.
        MoveList<BasicScoreMoveOrderer, MAX_MOVES, _SearchOptions.useMoveEvalTable> moveList;
        moveList.moveEvalTable = state->moveEvalTable;
        gen_all_moves<decltype(moveList), standardMovegenOptions, turn>(board, &moveList);
        
        constexpr i32 sign = -1 + 2 * turn; // the integer sign for the current turn, constexpr evaluated bc its a template arg

        // track the best known move and its eval
        Move bestMove;

        // iterate over list
        i32 legalMoves = 0;
        for (int i = moveList.count; i >= 0; i--) {
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
                // write_repeated(std::cout, currentPositiveDepth, "   ");
                // std::cout << "   + " << pieceToChar(extMove.piece) << " ";
                // std::cout << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
                // std::cout << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst)); 
                // std::cout << (move.is_en_passant() ? " ep" : "");
                // std::cout << "  -  " << moveList.scores[i] << "\n";

                // perform search on move
                SearchEvalResult result;
                if (nextDepth == 0) {
                    // evaluate leaf
                    result = search_fixed_internal_sync<_SearchOptions, !turn, true>(state, threadState, -beta, -alpha , maxDepth, 0);
                } else {
                    // continue search at next depth
                    result = search_fixed_internal_sync<_SearchOptions, !turn, false>(state, threadState, -beta, -alpha, maxDepth, nextDepth);
                }

                i32 evalForUs = -result.eval;

                /* [-] remember eval from this move in the move table */
                if constexpr (_SearchOptions.useMoveEvalTable) {
                    state->moveEvalTable->add(move, evalForUs / 1000);
                }

                write_repeated(std::cout, currentPositiveDepth, "   ");
                std::cout << "   [" << currentPositiveDepth << "] " << (turn ? "WHITE " : "BLACK ");
                debug_tostr_xmove(std::cout, *board, &extMove);
                std::cout << "  ->  eval: ";
                write_eval(std::cout, evalForUs);
                std::cout << ", alpha: ";
                write_eval(std::cout, alpha);
                std::cout << ", beta: "; 
                write_eval(std::cout, beta);
                std::cout << "\n";

                // alpha-beta pruning
                if (evalForUs >= beta) {
                    return { .move = NULL_MOVE, .eval = beta };
                } 

                if (evalForUs > alpha) {
                    alpha = evalForUs;
                    bestMove = move;
                }
            }

            // unmake move
            board->unmake_move_unchecked<turn, true>(&extMove);
        }

        // evaluate checks and checkmate
        if (board->is_in_check<turn>()) {
            if (_SearchOptions.debugMetrics) {
                state->metricChecks += 1;
            }

            if (legalMoves == 0) {
                if (_SearchOptions.debugMetrics) {
                    state->metricCheckmates += 1;
                }

                return make_leaf_eval(MATED_IN_PLY(/* current positive depth */ currentPositiveDepth));
            }
        }

        // evaluate stalemate
        if (legalMoves == 0) {
            if (_SearchOptions.debugMetrics) {
                state->metricStalemates += 1;
            }

            return make_leaf_eval(EVAL_DRAW);
        }

        // statically evaluate leaf node
        if constexpr (leaf) {
            if constexpr (_SearchOptions.debugMetrics) {
                state->metricTotalLeafNodes += 1;
            }

            EvalData evalData;
            evalData.legalMoveCount = legalMoves;

            // evaluate leaf node and return result
            i32 eval = sign * leafEval->eval(board, &evalData);
            return make_leaf_eval(eval);
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

/* Debug */
template<StaticSearchOptions const& _SearchOptions>
static void debug_tostr_search_metrics(std::ostream& os, SearchState<_SearchOptions>* state) {
    os << "[Search Metrics]\n";
    os << " Total Nodes Searched: " << state->metricTotalNodes << "\n";
    os << " Total Leaf Nodes Searched: " << state->metricTotalLeafNodes << "\n";
    os << " Captures: " << state->metricCaptures << "\n";
    os << " Checks: " << state->metricChecks << "\n";
    os << " Checkmates: " << state->metricCheckmates << "\n";
    os << " Stalemates: " << state->metricStalemates << "\n";
    os << " Illegal Generated: " << state->metricIllegal << "\n";
}

}