#pragma once

#include "board.hh"
#include "move.hh"
#include "lookup.hh"

#define MAX_MOVES 216

namespace tc {

enum MoveOrderingType {
    NO_MOVE_ORDERING,      // No move ordering, doesn't perform logic
    COMPARE_MOVE_ORDERING, // Compares each move with every other move
    SCORE_MOVE_ORDERING    // Gives a score to each move
};

/// @brief An automatically sorted, stack allocated move list which can 
/// be provided as a consumer for movegen functions.
/// @param _MoveOrderer Must be a class with a field `constexpr MoveOrderingType moveOrderingType` 
template<typename _MoveOrderer, u16 _Capacity, bool useEvalTable>
struct MoveList {
    u16 reserved = 0;                       // The amount of indices reserved at the start of the array for pre-defined
                                            // highly sorted moves.
    u16 count = 0;                          // The amount of moves in the array.
    Move moves[_Capacity] = { NULL_MOVE };  // The data array.

    /* Move Ordering */
    u16 scores[_Capacity];                  // The estimated scores for every move in the list, used for sorting.
    MoveEvalTable* moveEvalTable;           // The move eval table, used if enabled
    
    inline void accept(Board* board, Move move) {
        if constexpr (_MoveOrderer::moveOrderingType == NO_MOVE_ORDERING) { 
            moves[count++] = move;
        } else if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING) {
            u16 score = _MoveOrderer::score_move(board, move);
            
            if constexpr (useEvalTable) {
                score *= 10;
                score += moveEvalTable->get_adjustment(move);
            }

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

    static u16 score_move(Board* board, Move move) {
        Piece moved = move.moved_piece(board);
        Piece captured = move.captured_piece(board);
        return  /* capture */ (captured != NULL_PIECE) * (materialValuePerType[TYPE_OF_PIECE(captured)] * 4 - materialValuePerType[TYPE_OF_PIECE(moved)]) +
                /* promotion */ (move.is_promotion()) * 1000;
    }
};

/// @brief The compile-time movegen options
struct StaticMovegenOptions {
    const bool verifyChecksOnOpponent = true; // Whether to check for each move if it attacks the king, CURRENTLY EXCLUDING DISCOVERED CHECKS
    const bool verifyInCheckLegal = false;    // Whether to check if moves made while in check are legal at movegen time
    const bool verifyPinLegal = false;        // Whether to check for each move if it reveals a check on the ally king.

    const bool onlyCaptures = false;          // Whether to only generate captures
};

constexpr static StaticMovegenOptions defaultPsuedoLegalMovegenOptions = { .verifyChecksOnOpponent = true, .verifyInCheckLegal = false, .verifyPinLegal = false };
constexpr static StaticMovegenOptions defaultFullyLegalMovegenOptions  = { .verifyChecksOnOpponent = true, .verifyInCheckLegal = true, .verifyPinLegal = true  };

/// @brief Generate all (pseudo-)legal moves on the board for the given color.
/// @tparam _Consumer The move consumer type.
/// @tparam _Options The movegen options.
/// @tparam turn The turn (0 for black, 1 for white).
/// @param board The pointer to the board.
/// @param consumer The move consumer instance.
/// @return The amount of moves generated.
template<typename _Consumer, StaticMovegenOptions const& _Options, bool turn>
u8 gen_all_moves(Board* board, _Consumer* consumer) {
    u8 count = 0;

    // only generate non-evasion moves when not in double check 
    if (_popcount64(board->state.checkers[turn]) < 2) {
        count += gen_pawn_moves<_Consumer, _Options, turn>(board, consumer);
        count += gen_bb_moves<_Consumer, _Options, turn, KNIGHT, KNIGHT_MOBILITY>(board, consumer);
        count += gen_bb_moves<_Consumer, _Options, turn, BISHOP, DIAGONAL>(board, consumer);
        count += gen_bb_moves<_Consumer, _Options, turn, ROOK, STRAIGHT>(board, consumer);
        count += gen_bb_moves<_Consumer, _Options, turn, QUEEN, QUEEN_MOBILITY>(board, consumer);
    }

    count += movegen_king<_Consumer, _Options, turn>(board, consumer, board->kingIndexPerColor[turn], (turn * WHITE) | KING);

    return count;
}

/// @brief Generate all moves for the pieces which can be generated for using attack bitboards.
template<typename _Consumer, StaticMovegenOptions const& _Options, bool turn, PieceType pieceType, MobilityType mt>
u8 gen_bb_moves(Board* board, _Consumer* consumer) {
    u8 count = 0;
    Bitboard bb = board->pieceBBs[PIECE_COLOR_FOR(turn) | pieceType];

    Bitboard ourPieces = board->allPiecesPerColor[turn];
    Bitboard theirPieces = board->allPiecesPerColor[!turn];

    u8 fromIndex = 0;
    while (bb) {
        fromIndex = _pop_lsb(bb);

        // create attack bitboard
        u8 toIndex = 0;
        Bitboard attackBB = board->trivial_attack_bb<pieceType>() & ~ourPieces;
        Bitboard capturesBB = attackBB & theirPieces;
        Bitboard nonCapturesBB = attackBB & ourPieces;

        if constexpr (!_Options.onlyCaptures) {
            while (capturesBB) {
                toIndex = _pop_lsb(capturesBB);

                // create move
                Move move { .piece = (turn * WHITE_PIECE) | pieceType, .src = fromIndex, .dst = toIndex, .captured = board->pieces[toIndex], .isCheckEstimatedEstimated = board->is_check_attack<turn, mt>(toIndex) };
                consumer->accept(board, move);
            }
        }

        while (nonCapturesBB) {
            toIndex = _pop_lsb(nonCapturesBB);

            // create move
            Move move { .piece = (turn * WHITE_PIECE) | pieceType, .src = fromIndex, .dst = toIndex, .captured = NULL_PIECE, .isCheckEstimatedEstimated = board->is_check_attack<turn, mt>(toIndex) };
            consumer->accept(board, move);
        }
    }

    return count;
}

/* Special Piece Move Generation */

template<typename _Consumer, StaticMovegenOptions const& _Options, bool color>
inline int gen_pawn_moves(Board* board, _Consumer* consumer) {
    int count = 0;
    Bitboard bb = board->pieceBBs[(color * WHITE) | PAWN];
    while (bb) {
        u8 index = _pop_lsb(bb);

        count += movegen_pawn(board, consumer);
    }

    return count;
}

template<typename _Consumer, StaticMovegenOptions const& _Options, bool color>
inline int movegen_pawn(Board* board, _Consumer* consumer, u8 index) {
    Move move;
    u8 dst;
    u8 vdir = SIDE_OF_COLOR(color) * 8;
    u8 c = 0;

    // pawn move processing //
    auto acceptMove = [&](Move move) __attribute__((always_inline)) {
        // check for promotion 
        u8 rank = RANK(move.dst);
        if (rank == 0 || rank == 7) {
            consumer->accept(board, Move::make(move.src, move.dst, MOVE_PROMOTE_KNIGHT));
            consumer->accept(board, Move::make(move.src, move.dst, MOVE_PROMOTE_BISHOP));
            consumer->accept(board, Move::make(move.src, move.dst, MOVE_PROMOTE_ROOK));
            consumer->accept(board, Move::make(move.src, move.dst, MOVE_PROMOTE_QUEEN));
            c += 4;
            return;
        }

        consumer->accept(board, move);
        c++;
    };

    // pushing 1
    dst = index + vdir;
    if (dst >= 0 && dst < 64) {
        if (((board->allPieces >> dst) & 0x1) == 0) { 
            move = Move::make(index, dst);
            acceptMove(move);
        }
    }

    // pushing 2
    // pushing 2 squares can never be a promotion, so we dont
    // have to use acceptMove()
    u8 rank = RANK(index);
    if ((rank == 1 && color || rank == 6 && !color) && ((board->allPieces >> dst) & 0x1) == 0) {
        dst += vdir;
        if (((board->allPieces >> dst) & 0x1) == 0) { 
            move = Move::make_double_push(index, dst);
            consumer->accept(board, move);
            c++;
        }
    }

    // capturing
    u8 file = FILE(index);
    u8 front = index + vdir;
    if (front >= 0 && front < 64) {
        Bitboard enemyBB = board->allPiecesPerColor[!color];
        if (file > 0 && ((enemyBB >> (dst = front - 1)) & 0x1) == 1) {
            move = Move::make(index, dst);
            acceptMove(move);
        }

        if (file < 7 && ((enemyBB >> (dst = front + 1)) & 0x1) == 1) {
            move = Move::make(index, dst);
            acceptMove(move);
        }

        Bitboard enPassant = board->current_state()->enPassantTargets[!color];
        if (enPassant > 0) {
            // en passant can never be a promotion, so we dont
            // have to use acceptMove()
            if (file > 0 && ((enPassant >> (dst = front - 1)) & 0x1) == 1) {
                move = Move::make_en_passant(index, dst);
                consumer->accept(board, move);
                c++;
            }

            if (file < 7 && ((enPassant >> (dst = front + 1)) & 0x1) == 1) {
                move = Move::make_en_passant(index, dst);
                consumer->accept(board, move);
                c++;
            }
        }
    }

    return c;
}

template<typename _Consumer, StaticMovegenOptions const& _Options, bool color>
inline int movegen_king(Board* board, _Consumer* consumer, u8 index, Piece p) {
    int count = 0;
    Bitboard friendlyBB = board->allPiecesPerColor[color];
    Bitboard enemyBB = board->allPiecesPerColor[!color];
    u8 file = FILE(index);
    u8 rank = RANK(index);

    // king move processing //
    auto addMoveTo = [&](u8 dst) __attribute__((always_inline)) {
        if (((friendlyBB >> dst) & 0x1) > 0) return; // discard

        Move move = Move::make(index, dst);
        if (!kingmove_check_legal<color>(board, move)) return; // discard

        consumer->accept(board, move);
        count++;
    };

    // castling move gen //
    auto addCastlingMove = [&](u8 dstKingFile, u8 rookFile, bool right) __attribute__((always_inline)) {
        u8 kingDstIndex = INDEX(dstKingFile, rank);
        u8 newRookFile = rookFile + (1 - 2 * right);

        // todo: check for castling into checks

        // check squares empty, we have to check whether the bits
        // on the rank between the king and the rook are all zero
        // for this, we first extract the full rank bitline
        //   H G F E D C B A
        //   1 0 0 1 0 0 0 1
        // we then shift it right or left by the king file, depending on the side
        //  R, so >> 4 + 1
        //   0 0 0 0 0 1 0 0
        //  L, so << 4
        //   0 0 0 1 0 0 0 0
        // now we check if the number is less than or equal to
        // the value 
        u8 bitline = (board->allPieces >> (rank * 8)) & 0xFF;
        u8 shiftedBitline = (right ? bitline >> (file + 1) : bitline << file);
        if (right && __builtin_ctz(shiftedBitline) < rookFile - file - 1 || 
                        __builtin_clz(shiftedBitline << 24) < file - rookFile - 1) {
            return; // discard
        }

        // check if castled rook is check for opponent
        bool isCheckEstimated = board->is_check_attack<!color, STRAIGHT>(INDEX(newRookFile, rank));

        Move move = Move::make(index, kingDstIndex, right ? MOVE_CASTLE_RIGHT : MOVE_CASTLE_LEFT);
        if (!kingmove_check_legal<color>(board, move)) return; // discard

        consumer->accept(board, move);
    };

    if (file > 0) addMoveTo(index - 1);
    if (file < 7) addMoveTo(index + 1);
    if (rank > 0) addMoveTo(index - 8);
    if (rank < 7) addMoveTo(index + 8);
    if (file > 0 && rank > 0) addMoveTo(index - 1 - 8);
    if (file > 0 && rank < 7) addMoveTo(index - 1 + 8);
    if (file < 7 && rank > 0) addMoveTo(index + 1 - 8);
    if (file < 7 && rank < 7) addMoveTo(index + 1 + 8);

    // generate castling moves
    u8 flags = board->current_state()->castlingStatus[color];
    if ((flags & CAN_CASTLE_R) > 0) { addCastlingMove(file + 2, board->find_file_of_first_rook_on_rank<color, true>(rank), true); }
    if ((flags & CAN_CASTLE_L) > 0) { addCastlingMove(file - 2, board->find_file_of_first_rook_on_rank<color, false>(rank), false); }

    // todo: castling
    return count;
}

template<bool color>
inline bool kingmove_check_legal(Board* board, Move move) {
    // check moving into attack from other king
    u8 otherKingIndex = board->kingIndexPerColor[!color];
    u8 kingIndex = move.dst;
    if (FILE(kingIndex) >= FILE(otherKingIndex) - 1 && FILE(kingIndex) <= FILE(otherKingIndex) + 1 &&
        RANK(kingIndex) >= RANK(otherKingIndex) - 1 && RANK(kingIndex) <= RANK(otherKingIndex) + 1) return false; 

    // check moving into attack
    return (board->state.attackBBsPerColor[!color] & (1ULL << kingIndex)) < 1;
}

}