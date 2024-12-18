#pragma once

#include <string>
#include <sstream>
#include <concepts>

#include "types.h"
#include "piece.h"

#define CASTLED_L (1 << 0)      // Has castled Queen Side (left)
#define CASTLED_R (1 << 1)      // Has castled King Side (right)
#define CAN_CASTLE_L (1 << 2)   // Castle rights Queen side (left)
#define CAN_CASTLE_R (1 << 3)   // Castle rights King side (right)
#define MOVE_CASTLE_L CASTLED_L // Castle Queen side move (left)
#define MOVE_CASTLE_R CASTLED_R // Castle King side move (left)

enum MobilityType {
    PAWN_MOBILITY,
    KNIGHT_MOBILITY,
    DIAGONAL,
    STRAIGHT,

    MOBILITY_TYPE_COUNT
};

#define INDEX(file, rank) ((rank) * 8 + (file))
#define FILE(index) ((index) & 0x7)
#define RANK(index) (((index) >> 3) & 0x7)

/* INDEX DECLARATIONS */
#define FILE_TO_CHAR(file) ((char)((file) + 'a'))
#define RANK_TO_CHAR(rank) ((char)((rank) + '1'))

#define fA 0
#define fB 1
#define fC 2
#define fD 3
#define fE 4
#define fF 5
#define fG 6
#define fH 7

#define r1 0 << 3
#define r2 1 << 3
#define r3 2 << 3
#define r4 3 << 3
#define r5 4 << 3
#define r6 5 << 3
#define r7 6 << 3
#define r8 7 << 3

/// @brief All data about a move 
struct alignas(4) Move {
    // The source piece originally moved
    Piece piece : 6;
    // The source and destination indices
    u8 src : 8;
    u8 dst : 8;
    // The captured piece or NULL_PIECE of no capture occurred
    Piece captured : 6 = NULL_PIECE;
    // Whether the move creates a check on the king
    bool isCheck : 1 = false;
     // Whether the move is en passant
    bool enPassant : 1 = false;
    // Whether this move is a promotion, if so, to what
    PieceType promotionType : 4 = NULL_PIECE_TYPE;
    // The castling operations this move performs, where the castling right flags actually revoke the castling rights.
    u8 castleOperations : 4 = 0;
    // The position (file) on the rank of the rook before castling
    u8 rookFile : 3 = 0;
    // Whether the move is a double pawn push
    bool isDoublePush : 1 = false;

    inline bool isNull() {
        return src == 0 && dst == 0;
    }

    inline u8 captureIndex() {
        return dst - (enPassant * SIDE_OF_PIECE(piece) * 8);
    }
};

/// Represents the absence of a move.
static Move NULL_MOVE = { .piece = NULL_PIECE, .src = 0, .dst = 0 };

template<typename _Func>
struct FunctionMoveConsumer {
    FunctionMoveConsumer(_Func func): func(func) { }

    _Func func;

    inline void accept(Move move) {
        func(move);
    }
};

enum MoveOrderingType {
    NO_MOVE_ORDERING,      // No move ordering, doesn't perform logic
    COMPARE_MOVE_ORDERING, // Compares each move with every other move
    SCORE_MOVE_ORDERING    // Gives a score to each move
};

/// @brief An automatically sorted, stack allocated move list which can 
/// be provided as a consumer for movegen functions.
/// @param _MoveOrderer Must be a class with a field `constexpr MoveOrderingType moveOrderingType` 
template<typename _MoveOrderer, u16 _Capacity>
struct MoveList {
    u16 count = 0;          // The amount of moves in the array
    Move moves[_Capacity];  // The data array

    u16 scores[_Capacity];  // The estimated scores for every move in the list, used for sorting
    
    inline void accept(Move move) {
        if constexpr (_MoveOrderer::moveOrderingType == NO_MOVE_ORDERING) { 
            moves[count++] = move;
        } else if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING) {
            u16 score = _MoveOrderer::score_move(move);

            // check trivial case, we sort in ascending order because
            // we can simply traverse the array in reverse
            if (score >= moves[count - 1]) {
                moves[count++] = move;
                scores[count] = score;
                return;
            }

            // find index and insert into appropriate location
            int index = 0;
            while (score >= scores[index]) index++;
            memmove(&moves[index + 1], &moves[index], (count - index) * sizeof(Move));
            memmove(&scores[index + 1], &scores[index], (count - index) * sizeof(Move));
            moves[index] = move;
            scores[index] = score;
        }
    }
};

/// Doesn't perform any move ordering.
struct NoOrderMoveOrderer {
    constexpr static MoveOrderingType moveOrderingType = NO_MOVE_ORDERING;
};

/// Scores each move on some basic properties
struct BasicScoreMoveOrderer {
    constexpr static MoveOrderingType moveOrderingType = SCORE_MOVE_ORDERING;

    static u16 score_move(Move move) {
        return /* checks */ move.isCheck * 5000 +
                /* capture */ (move.captured != NULL_PIECE) * (materialValuePerType[TYPE_OF_PIECE(move.captured)] * 4 - materialValuePerType[TYPE_OF_PIECE(move.piece)]) +
                /* promotion */ (move.promotionType != NULL_PIECE_TYPE) * 1000;
    }
};

/// Compares each move
struct BasicCompareMoveOrderer {
    constexpr static MoveOrderingType moveOrderingType = COMPARE_MOVE_ORDERING;

    static int compare_move(Move a, Move b) {
        return 0; // todo
    }
};