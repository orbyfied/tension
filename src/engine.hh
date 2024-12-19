#pragma once

#include "board.hh"
#include "debug.hh"
#include "movegen.hh"

namespace tc {

/// @brief Heap-allocated hashtable containing cached evaluations for positions
struct TranspositionTable {

};

/// @brief The best move as a result of a search and it's signed evaluation
/// If this is a leaf node, the move will be a null move and the evaluation will be
/// the static evaluation of the position.
struct MoveResult {
    Move move;
    i32 eval;
};

/// @brief Compile time search options
struct StaticSearchOptions {
    bool useTranspositionTable;
};

/// @brief The state object for each fixed depth search
template<StaticSearchOptions const& _SearchOptions>
struct SearchState {
    TranspositionTable* transpositionTable;
};

template <StaticBoardOptions const& _BoardOptions>
using EvaluationFunction = i32(*)(Board<_BoardOptions>*);

/// @brief The state object for an iterative search
template<StaticSearchOptions const& _SearchOptions>
struct IterativeSearchState {
    /// @brief The state to use for each fixed depth search
    SearchState<_SearchOptions> searchState;

    /// @brief Whether to end the search on this iteration
    bool end = false;
};

template<StaticBoardOptions const& _BoardOptions>
struct Engine {
    constexpr static StaticBoardOptions const& boardOptions = _BoardOptions;

    /// @brief The current board being evaluated/searched.
    Board<_BoardOptions>* board;

    /* Evaluation Functions */
    EvaluationFunction<_BoardOptions> evalLeaf0; // Static evaluation for leaf nodes
    
    constexpr static StaticMovegenOptions standardMovegenOptions = { };

    /// @brief Search the current position to the given fixed depth
    /// @param state The search state
    template<StaticSearchOptions const& _SearchOptions>
    MoveResult search_fixed_internal(SearchState<_SearchOptions>* state, i32 alpha, i32 beta, u16 depth) {
        if (depth == 0) {
            // evaluate leaf node and return result
            i32 eval = evalLeaf0(board);
            return { .move = NULL_MOVE, .eval = eval };
        }

        bool turn = board->whiteTurn;
        i32 side = -1 + 2 * turn;

        Move bestMove;
        i32 bestEval = 0; 

        MoveList<BasicScoreMoveOrderer, 196> moveList;
        gen_pseudo_legal_moves<decltype(moveList), standardMovegenOptions, _BoardOptions>(board, turn, &moveList);

        // check for count
        if (moveList.count == 0) {
            if (board->is_in_check(turn)) {
                return { .move = NULL_MOVE, .eval = -99999 };
            }

            return { .move = NULL_MOVE, .eval = 0 } ;
        }

        // iterate over list
        i32 nextDepth = depth - 1;
        for (int i = 0; i < moveList.reserved + moveList.count; i++) {
            Move move = moveList.moves[i];
            if (move.isNull()) continue;

            ExtMove<true> extMove(move);

            board->make_move_unchecked(&extMove);

            // test if we left our own king in check, should happen rarely but 
            // the moves are still pseudo legal so we have to check to avoid performing
            // a search on an illegal move wasting performance. should be relatively 
            // inexpensive due to the tracking
            if (board->is_in_check(turn)) { 
                board->unmake_move_unchecked(&extMove);
                continue;
            }

            // perform search on move
            MoveResult result = side * search_fixed_internal(state, -alpha, -beta, nextDepth);
            i32 evalForUs = -result.eval;

            // alpha-beta pruning
            if (evalForUs >= beta) {
                return { .move = NULL_MOVE, .eval = beta };
            }

            if (evalForUs > alpha) {
                alpha = evalForUs;
            }

            // unmake move
            board->unmake_move_unchecked(&extMove);
        }
    }

    /// @brief Start an iterative deepening search with the given search state.
    /// @param state The search state.
    /// The best move found and it's evaluation is saved to the search state.
    template<StaticSearchOptions const& _SearchOptions>
    MoveResult search_iterative(IterativeSearchState<_SearchOptions>* state) {
        // first do a depth 1 search
        MoveResult bestMoveResult = search_fixed_internal(state, 99999999, -99999999, 1);

        // then iteratively go 2 and higher
        u16 depth = 2;
        while (!state->end) {
            // reset cached search states

            // perform search

            // update optimization for next search
        }

        return bestMoveResult;
    }
};

}