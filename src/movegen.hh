#pragma once

#include "board.hh"
#include "move.hh"
#include "lookup.hh"

#include <algorithm>

#define MAX_MOVES 216

namespace tc {

// fwd decl
struct BoardToStrOptions;
void debug_tostr_move(std::ostream& oss, Move move);
void debug_tostr_board(std::ostream& oss, Board& b);

enum MoveOrderingType {
    NO_MOVE_ORDERING,      // No move ordering, doesn't perform logic
    COMPARE_MOVE_ORDERING, // Compares each move with every other move
    SCORE_MOVE_ORDERING    // Gives a score to each move
};

struct MoveScorePair {
    Move move;
    i16 score;
};

/// @brief An automatically sorted, stack allocated move list which can 
/// be provided as a consumer for movegen functions.
/// @param _MoveOrderer Must be a class with a field `constexpr MoveOrderingType moveOrderingType` 
template<typename _MoveOrderer, u16 _Capacity, bool useEvalTable>
struct MoveList {
    u16 count = 0;                                   // The amount of moves in the array.
    MoveScorePair moves[_Capacity];                  // The data array.

    /* Move Ordering */
    MoveEvalTable* moveEvalTable;                    // The move eval table, used if enabled

    inline void set(int i, Move move, i16 score) {
        moves[i] = { .move = move, .score = score };
    }

    inline Move get_move(int i) {
        return moves[i].move;
    }

    inline i16 get_score(int i) {
        return moves[i].score;
    }

    template<bool turn>
    inline i16 score_move(Board* board, Move move) {
        i16 score = _MoveOrderer::template score_move<turn>(board, move);
            
        if constexpr (useEvalTable) {
            score += moveEvalTable->get_adjustment(move);
        }

        return score;
    }

    template<bool turn>
    inline void sort_moves(Board* board) {
#ifndef TC_MOVE_INSERT_SORT
        if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING) {
            // score all moves
            for (int i = count; i >= 0; i--) {
                if (moves[i].move.null()) continue; 
                moves[i].score = score_move<turn>(board, moves[i].move);
            }

            // sort the array using custom comp
            std::sort((MoveScorePair*) &moves, (MoveScorePair*) &moves[count], [&](MoveScorePair a, MoveScorePair b) {
                return a.score < b.score; // ascending order
            });
        }
#endif
    }

    template<Color turn>
    inline void accept(Board* board, Move move) {
#ifndef TC_MOVE_INSERT_SORT
        if (true) {
            moves[count++] = { .move = move, .score = -9999 };
            return;
        }
#endif
        /* used to do sort on insertion, but its actually slower due to the necessity to move 
           data on insertion into the middle of the array */
#ifdef TC_MOVE_INSERT_SORT
        if constexpr (_MoveOrderer::moveOrderingType == NO_MOVE_ORDERING) { 
            moves[count++] = { .move = move, .score = -9999 };
        } else if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING) {
            i16 score = _MoveOrderer::template score_move<turn>(board, move);
            
            if constexpr (useEvalTable) {
                score += moveEvalTable->get_adjustment(move);
            }

            // check trivial case, we sort in ascending order because
            // we can simply traverse the array in reverse
            if (count == 0 || score >= moves[count - 1].score) {
                moves[count++] = { .move = move, .score = score };
                return;
            }

            // find index and insert into appropriate location
            int index = 0;
            while (score >= moves[index].score && index < count) index++;
            memmove(&moves[index + 1], &moves[index], (count - index) * sizeof(MoveScorePair));
            moves[index] = { .move = move, .score = score };
            count++;
        }
#endif
    }
};

/// Doesn't perform any move ordering.
struct NoOrderMoveOrderer {
    constexpr static MoveOrderingType moveOrderingType = NO_MOVE_ORDERING;
};

/// Scores each move on some basic properties
struct BasicScoreMoveOrderer {
    constexpr static MoveOrderingType moveOrderingType = SCORE_MOVE_ORDERING;

    template<Color turn>
    static i16 score_move(Board* board, Move move) {
        Piece moved = move.moved_piece(board);
        Piece captured = move.captured_piece(board);
        i16 captureScore = (captured != NULL_PIECE ? materialValuePerType[TYPE_OF_PIECE(captured)] * 200 - materialValuePerType[TYPE_OF_PIECE(moved)] * 50 : 0);
        
        i16 promotionScore = (move.flags == MOVE_PROMOTE_QUEEN) * 750 + (move.flags == MOVE_PROMOTE_KNIGHT) * 500 + (move.flags == MOVE_PROMOTE_ROOK) * 300 + (move.flags == MOVE_PROMOTE_BISHOP) * 300;
        return (i16) ( 
                /* capture */ (captureScore >= 0 ? captureScore : 0) +
                /* promotion */ promotionScore +
                // /* direct check */ ((board->volatile_state()->checkingSquares[!turn][TYPE_OF_PIECE(moved)] & (1ULL << move.destination())) > 1) * 500 +
                /* en passant */ (move.is_en_passant() * 100)
        );
    }
}; 

/// @brief The compile-time movegen options
struct StaticMovegenOptions {
    const bool leafMoves = false;             // Only generate moves needed to determine check-/stalemate

    const bool verifyInCheckLegal = false;    // Whether to check if moves made while in check are legal at movegen time
    const bool verifyPinLegal = true;         // Whether to check for each move if it reveals a check on the ally king.

    const bool updateAttackState = false;     // Whether to recalculate the attack state for the moving color during movegen

    const bool onlyEvasions = false;          // Whether to only generate evasions
    const bool onlyCaptures = false;          // Whether to only generate captures
};

constexpr static StaticMovegenOptions defaultPsuedoLegalMovegenOptions = { .verifyInCheckLegal = false, .verifyPinLegal = false };
constexpr static StaticMovegenOptions defaultFullyLegalMovegenOptions  = { .verifyInCheckLegal = true, .verifyPinLegal = true  };

/// @brief Generate all (pseudo-)legal moves on the board for the given color.
/// @tparam _Consumer The move consumer type.
/// @tparam _Options The movegen options.
/// @tparam turn The turn (0 for black, 1 for white).
/// @param board The pointer to the board.
/// @param consumer The move consumer instance.
/// @return The amount of moves generated.
template<typename _Consumer, StaticMovegenOptions const& _Options, Color turn>
void gen_all_moves(Board* board, _Consumer* consumer) {
    // only generate non-evasion moves when not in double check 
    if (_popcount64(board->checkers(turn)) < 2) {
        gen_pawn_moves<_Consumer, _Options, turn>(board, consumer);
        gen_bb_moves<_Consumer, _Options, turn, KNIGHT>(board, consumer);
        gen_bb_moves<_Consumer, _Options, turn, BISHOP>(board, consumer);
        gen_bb_moves<_Consumer, _Options, turn, ROOK>(board, consumer);
        gen_bb_moves<_Consumer, _Options, turn, QUEEN>(board, consumer);
    }
    
    // todo: generate king moves for this side
    if (board->kingIndexPerColor[turn] != NULL_SQ) {
        movegen_king<_Consumer, _Options, turn>(board, consumer, board->kingIndexPerColor[turn], (turn * WHITE) | KING);
    }
}

template<typename _Consumer, StaticMovegenOptions const& _Options>
inline void gen_all_moves(Board* board, _Consumer* consumer, Color turn) {
    if (turn) gen_all_moves<_Consumer, _Options, WHITE>(board, consumer);
    else gen_all_moves<_Consumer, _Options, BLACK>(board, consumer);
}

/// @brief Generate all moves for the pieces which can be generated for using attack bitboards.
template<typename _Consumer, StaticMovegenOptions const& _Options, Color turn, PieceType pieceType>
void gen_bb_moves(Board* board, _Consumer* consumer) {
    Bitboard bb = board->pieces(turn, pieceType);

    const Bitboard ourPieces = board->pieces_for_side(turn);
    const Bitboard theirPieces = _Options.onlyEvasions ? board->checkers(turn) : board->pieces_for_side(!turn);

    u8 fromIndex = 0;
    while (bb) {
        fromIndex = _pop_lsb(bb);

        // create attack bitboard
        Sq toIndex = 0;
        Bitboard attackBB = board->trivial_attack_bb<pieceType>(fromIndex) & ~ourPieces;

        if constexpr (_Options.onlyCaptures) {
            attackBB &= theirPieces;
        }
        
        while (attackBB) {
            toIndex = _pop_lsb(attackBB);

            // create move
            Move move = Move::make(fromIndex, toIndex);
            consumer->template accept<turn>(board, move);
        }
    }
}

/* Special Piece Move Generation */

template<typename _Consumer, StaticMovegenOptions const& _Options, Color color>
inline void gen_pawn_moves(Board* board, _Consumer* consumer) {
    auto makePromotions = [&](u8 src, u8 dst) __attribute__((always_inline)) {
        consumer->template accept<color>(board, Move::make(src, dst, MOVE_PROMOTE_KNIGHT));
        consumer->template accept<color>(board, Move::make(src, dst, MOVE_PROMOTE_BISHOP));
        consumer->template accept<color>(board, Move::make(src, dst, MOVE_PROMOTE_ROOK));
        consumer->template accept<color>(board, Move::make(src, dst, MOVE_PROMOTE_QUEEN));
    };

    constexpr DirectionOffset UpOffset = (color ? OFF_NORTH : OFF_SOUTH);

    const Bitboard ourPawns = board->pieces(color, PAWN);
    const Bitboard freeSquares = ~board->all_pieces();
    const Bitboard enemies = _Options.onlyEvasions ? board->checkers(color) : board->pieces_for_side(!color);

    // make single and double pushes
    if constexpr (!_Options.onlyCaptures) {
        Bitboard push1BB = shift<UpOffset>(ourPawns) & freeSquares;
        Bitboard push1BBPromotions = push1BB & BB_1_OR_8_RANK;
        push1BB = push1BB & ~BB_1_OR_8_RANK;
        Bitboard push2BB = shift<UpOffset>(shift<UpOffset>(ourPawns & BB_2_OR_7_RANK) & freeSquares) & freeSquares;

        while (push1BB) { 
            u8 dst = _pop_lsb(push1BB);
            consumer->template accept<color>(board, Move::make(dst - UpOffset, dst));
        }

        while (push1BBPromotions) { 
            u8 dst = _pop_lsb(push1BBPromotions);
            makePromotions(dst - UpOffset, dst);
        }

        while (push2BB) { 
            u8 dst = _pop_lsb(push2BB);
            consumer->template accept<color>(board, Move::make_double_push(dst - UpOffset * 2, dst));
        }
    }

    // make captures
    Bitboard capturesEast = shift<UpOffset + OFF_EAST>(ourPawns & BB_FILES_17_MASK) & enemies;
    Bitboard capturesWest = shift<UpOffset + OFF_WEST>(ourPawns & BB_FILES_28_MASK) & enemies;
    Bitboard b;

    b = capturesEast & BB_1_OR_8_RANK;
    while (b) { 
        u8 dst = _pop_lsb(b);
        makePromotions(dst - UpOffset - OFF_EAST, dst);
    }

    capturesEast = capturesEast & ~BB_1_OR_8_RANK;
    while (capturesEast) { 
        u8 dst = _pop_lsb(capturesEast);
        consumer->template accept<color>(board, Move::make(dst - UpOffset - OFF_EAST, dst));
    }

    b = capturesWest & BB_1_OR_8_RANK;
    while (b) { 
        u8 dst = _pop_lsb(b);
        makePromotions(dst - UpOffset - OFF_WEST, dst);
    }

    capturesWest = capturesWest & ~BB_1_OR_8_RANK;
    while (capturesWest) { 
        u8 dst = _pop_lsb(capturesWest);
        consumer->template accept<color>(board, Move::make(dst - UpOffset - OFF_WEST, dst));
    }

    // make en passant
    Sq enPassantTarget = board->volatile_state()->enPassantTarget;
    if (enPassantTarget != NULL_SQ) {
        Bitboard movablePawns = ourPawns & lookup::pawnAttackBBs.values[!color][enPassantTarget];

        while (movablePawns) {
            Sq src = _pop_lsb(movablePawns);
            consumer->template accept<color>(board, Move::make_en_passant(src, enPassantTarget));
        }
    }
}

template<typename _Consumer, StaticMovegenOptions const& _Options, Color color>
inline void movegen_king(Board* board, _Consumer* consumer, Sq index, Piece p) {
    const Bitboard friendlyBB = board->allPiecesPerColor[color];
    const Bitboard enemyBB = board->allPiecesPerColor[!color];
    const u8 file = FILE(index);
    const u8 rank = RANK(index);
    const Bitboard attacked = board->attacks_by(!color);
    // const Bitboard attacked = 0;

    // castling move gen //
    auto addCastlingMove = [&](u8 dstIndex, u8 rookFile, bool right) __attribute__((always_inline)) {
        if (rookFile == NULL_SQ) return;

        Bitboard unattackedCondition = (right ? (0b00000011ULL << index) : (0b00000011ULL << (index - 3)));
        if ((attacked & unattackedCondition) > 0) {
            return; // discard
        }

        consumer->template accept<color>(board, Move::make(index, dstIndex, (right ? MOVE_CASTLE_RIGHT : MOVE_CASTLE_LEFT)));
    };

    // normal movement
    Bitboard dstBB = lookup::kingMovementBBs.values[index] & ~attacked & ~friendlyBB;
    while (dstBB) {
        u8 index = _pop_lsb(dstBB);
        consumer->template accept<color>(board, Move::make(index, index));
    }
    
    // can not castle while in check
    if (board->checkers(color) > 0) { 
        return;
    }

    // castling moves
    u8 flags = board->volatile_state()->castlingStatus[color];
    if ((flags & CAN_CASTLE_R) > 0) { addCastlingMove(index + 2, board->find_file_of_first_rook_on_rank<color, true>(rank), true); }
    if ((flags & CAN_CASTLE_L) > 0) { addCastlingMove(index - 2, board->find_file_of_first_rook_on_rank<color, false>(rank), false); }
}

}