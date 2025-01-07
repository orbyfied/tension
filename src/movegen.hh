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
    i16 scores[_Capacity] = { 0 };          // The estimated scores for every move in the list, used for sorting.
    MoveEvalTable* moveEvalTable;           // The move eval table, used if enabled

    template<Color turn>
    inline void accept(Board* board, Move move) {
        if constexpr (_MoveOrderer::moveOrderingType == NO_MOVE_ORDERING) { 
            moves[count++] = move;
        } else if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING) {
            i16 score = _MoveOrderer::template score_move<turn>(board, move);
            
            if constexpr (useEvalTable) {
                score *= 10;
                score += moveEvalTable->get_adjustment(move);
            }

            // check trivial case, we sort in ascending order because
            // we can simply traverse the array in reverse
            if (count == 0 || score >= scores[count - 1]) {
                moves[count] = move;
                scores[count] = score;
                count++;
                return;
            }

            // find index and insert into appropriate location
            int index = 0;
            while (score >= scores[index]) index++;
            memmove(&moves[index + 1], &moves[index], (count - index) * sizeof(Move));
            memmove(&scores[index + 1], &scores[index], (count - index) * sizeof(Move));
            moves[index] = move;
            scores[index] = score;
            count++;
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

    template<Color turn>
    static i16 score_move(Board* board, Move move) {
        Piece moved = move.moved_piece(board);
        Piece captured = move.captured_piece(board);
        return  /* capture */ (captured != NULL_PIECE) * (materialValuePerType[TYPE_OF_PIECE(captured)] * 150 - materialValuePerType[TYPE_OF_PIECE(moved)] * 100) +
                /* promotion */ (move.is_promotion()) * 200 +
                /* direct check */ ((board->volatile_state()->checkingSquares[!turn][TYPE_OF_PIECE(moved)] & (1ULL << move.destination())) > 1) * 500;
    }
};

/// @brief The compile-time movegen options
struct StaticMovegenOptions {
    const bool verifyInCheckLegal = false;    // Whether to check if moves made while in check are legal at movegen time
    const bool verifyPinLegal = false;        // Whether to check for each move if it reveals a check on the ally king.

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
        u8 toIndex = 0;
        const Bitboard attackBB = board->trivial_attack_bb<pieceType>(fromIndex) & ~ourPieces;
        Bitboard capturesBB = attackBB & theirPieces;
        Bitboard nonCapturesBB = attackBB & ~theirPieces;

        if constexpr (!_Options.onlyCaptures) {
            while (capturesBB) {
                toIndex = _pop_lsb(capturesBB);

                // create move
                Move move = Move::make(fromIndex, toIndex);
                consumer->template accept<turn>(board, move);
            }
        }

        while (nonCapturesBB) {
            toIndex = _pop_lsb(nonCapturesBB);

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

    const Bitboard ourPawns = board->pieces(WHITE, PAWN);
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
        u8 dst = _pop_lsb(b);
        consumer->template accept<color>(board, Move::make(dst - UpOffset - OFF_EAST, dst));
    }

    b = capturesWest & BB_1_OR_8_RANK;
    while (b) { 
        u8 dst = _pop_lsb(b);
        makePromotions(dst - UpOffset - OFF_WEST, dst);
    }

    capturesEast = capturesWest & ~BB_1_OR_8_RANK;
    while (capturesEast) { 
        u8 dst = _pop_lsb(b);
        consumer->template accept<color>(board, Move::make(dst - UpOffset - OFF_WEST, dst));
    }

    std::cout << "hi3\n";

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
inline int movegen_king(Board* board, _Consumer* consumer, Sq index, Piece p) {
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

        consumer->template accept<color>(board, move);
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

        consumer->template accept<color>(board, move);
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
    u8 flags = board->volatile_state()->castlingStatus[color];
    if ((flags & CAN_CASTLE_R) > 0) { addCastlingMove(file + 2, board->find_file_of_first_rook_on_rank<color, true>(rank), true); }
    if ((flags & CAN_CASTLE_L) > 0) { addCastlingMove(file - 2, board->find_file_of_first_rook_on_rank<color, false>(rank), false); }

    // todo: castling
    return count;
}

template<Color color>
inline bool kingmove_check_legal(Board* board, Move move) {
    // check moving into attack from other king
    u8 otherKingIndex = board->kingIndexPerColor[!color];
    u8 kingIndex = move.dst;
    if (FILE(kingIndex) >= FILE(otherKingIndex) - 1 && FILE(kingIndex) <= FILE(otherKingIndex) + 1 &&
        RANK(kingIndex) >= RANK(otherKingIndex) - 1 && RANK(kingIndex) <= RANK(otherKingIndex) + 1) return false; 

    // check moving into attack
    return (board->volatile_state()->attackBBsPerColor[!color] & (1ULL << kingIndex)) < 1;
}

}