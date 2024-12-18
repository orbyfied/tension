#include "board.h"
#include "debug.h"
#include "movegen.h"

/// @brief The best move as a result of a search and it's signed evaluation
/// If this is a leaf node, the move will be a null move and the evaluation will be
/// the static evaluation of the position.
struct MoveResult {
    Move move;
    i32 eval;
};

/// @brief The state object for each fixed depth search
struct SearchState {

};

template <StaticBoardOptions const& _BoardOptions>
using EvaluationFunction = i32(*)(Board<_BoardOptions>*);

/// @brief The state object for an iterative search
struct IterativeSearchState {
    /// @brief The state to use for each fixed depth search
    SearchState searchState;

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

    /// @brief Search the current position to the given fixed depth
    /// @param state The search state
    MoveResult search_fixed(SearchState* state, u16 depth) {

    }

    constexpr static StaticMovegenOptions standardMovegenOptions = { };

    MoveResult search_fixed_internal(SearchState* state, i32 maxEval /* a */, i32 minEval /* b */, u16 depth) {
        if (depth == 0) {
            i32 eval = evalLeaf0(board);
            return { .move = NULL_MOVE, .eval = eval };
        }

        // int x = depth;
        // char moves[x];
        // MoveList<1> moveList;
    }

    /// @brief Start an iterative deepening search with the given search state.
    /// @param state The search state.
    /// The best move found and it's evaluation is saved to the search state.
    void search_iterative(IterativeSearchState* state) {
        MoveResult bestMoveResult;
        u16 depth = 1;
        while (!state->end) {
            bestMoveResult = search_fixed_internal(state, 99999999, -99999999, depth);
        }
    }
};