#pragma once

#include "board.hh"
#include "move.hh"
#include "lookup.hh"
#include "tt.hh"

#include <algorithm>

#define MAX_MOVES 216

namespace tc {

// fwd decl
struct BoardToStrOptions;
void debug_tostr_move(std::ostream& oss, Move move);
void debug_tostr_board(std::ostream& oss, Board& b);

/// @brief The compile-time movegen options
struct StaticMovegenOptions {
    const bool onlyEvasions = false;          // Whether to only generate evasions
    const bool captures = true;               // Whether to generate captures
    const bool quiets = true;                 // Whether to generate quiet moves
};

constexpr static StaticMovegenOptions movegenAllPL = {  };
constexpr static StaticMovegenOptions movegenCapturesPL = { .captures = true, .quiets = false };
constexpr static StaticMovegenOptions movegenQuietsPL = { .captures = false, .quiets = true };

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

        // if in check, only allow movement to one of the checking squares
        // this is not necessary due to the way search handles illegals, but it is a
        // simple and very inexpensive check so it will result in less moves having to be checked
        // this also covers captures of the checking pieces. this does not exclude all illegal moves,
        // but the number of moves should still be greatly reduced
        if (board->checkers(turn) > 0) {
            attackBB &= board->checkingSquares[turn][QUEEN] | board->checkers(turn);
        }
        
        // capturesBB will contain all captures, attackBB with only
        // contain quiet move destinations
        Bitboard capturesBB = attackBB & theirPieces;
        attackBB &= ~theirPieces;
        
        if constexpr (_Options.quiets) {
            while (attackBB) {
                toIndex = _pop_lsb(attackBB);

                // create move
                Move move = Move::make(fromIndex, toIndex);
                consumer->template acceptx<turn, pieceType, /* capture */ false, 0>(board, move);
            }
        }
        
        if constexpr (_Options.captures) {
            while (capturesBB) {
                toIndex = _pop_lsb(capturesBB);

                // create move
                Move move = Move::make(fromIndex, toIndex);
                consumer->template acceptx<turn, pieceType, /* capture */ true, 0>(board, move);
            }
        }
    }
}

/* Special Piece Move Generation */

template<typename _Consumer, Color color, bool capture>
forceinline void make_promotions(Board* board, _Consumer* consumer, u8 src, u8 dst) {
    consumer->template acceptx<color, PAWN, capture, MOVE_PROMOTE_KNIGHT>(board, Move::make(src, dst, MOVE_PROMOTE_KNIGHT));
    consumer->template acceptx<color, PAWN, capture, MOVE_PROMOTE_QUEEN>(board, Move::make(src, dst, MOVE_PROMOTE_QUEEN));
    consumer->template acceptx<color, PAWN, capture, MOVE_PROMOTE_BISHOP>(board, Move::make(src, dst, MOVE_PROMOTE_BISHOP));
    consumer->template acceptx<color, PAWN, capture, MOVE_PROMOTE_ROOK>(board, Move::make(src, dst, MOVE_PROMOTE_ROOK));
}

template<typename _Consumer, StaticMovegenOptions const& _Options, Color color>
inline void gen_pawn_moves(Board* board, _Consumer* consumer) {
    constexpr DirectionOffset UpOffset = (color ? OFF_NORTH : OFF_SOUTH);

    const Bitboard ourPawns = board->pieces(color, PAWN);
    const Bitboard freeSquares = ~board->all_pieces();
    const Bitboard enemies = _Options.onlyEvasions ? board->checkers(color) : board->pieces_for_side(!color);

    // make single and double pushes
    if constexpr (_Options.quiets) {
        Bitboard push1BB = shift<UpOffset>(ourPawns) & freeSquares;
        Bitboard push1BBPromotions = push1BB & BB_1_OR_8_RANK;
        push1BB = push1BB & ~BB_1_OR_8_RANK;
        Bitboard push2BB = shift<UpOffset>(shift<UpOffset>(ourPawns & BB_2_OR_7_RANK) & freeSquares) & freeSquares;

        while (push1BB) { 
            u8 dst = _pop_lsb(push1BB);
            consumer->template acceptx<color, PAWN, false, 0>(board, Move::make(dst - UpOffset, dst));
        }

        while (push1BBPromotions) { 
            u8 dst = _pop_lsb(push1BBPromotions);
            make_promotions<_Consumer, color, false>(board, consumer, dst - UpOffset, dst);
        }

        while (push2BB) { 
            u8 dst = _pop_lsb(push2BB);
            consumer->template acceptx<color, PAWN, false, MOVE_DOUBLE_PUSH>(board, Move::make_double_push(dst - UpOffset * 2, dst));
        }
    }

    if constexpr (_Options.captures) {
        // make captures
        Bitboard capturesEast = shift<UpOffset + OFF_EAST>(ourPawns & BB_FILES_17_MASK) & enemies;
        Bitboard capturesWest = shift<UpOffset + OFF_WEST>(ourPawns & BB_FILES_28_MASK) & enemies;
        Bitboard b;

        b = capturesEast & BB_1_OR_8_RANK;
        while (b) { 
            u8 dst = _pop_lsb(b);
            make_promotions<_Consumer, color, true>(board, consumer, dst - UpOffset - OFF_EAST, dst);
        }

        capturesEast = capturesEast & ~BB_1_OR_8_RANK;
        while (capturesEast) { 
            u8 dst = _pop_lsb(capturesEast);
            consumer->template acceptx<color, PAWN, true, 0>(board, Move::make(dst - UpOffset - OFF_EAST, dst));
        }

        b = capturesWest & BB_1_OR_8_RANK;
        while (b) { 
            u8 dst = _pop_lsb(b);
            make_promotions<_Consumer, color, true>(board, consumer, dst - UpOffset - OFF_WEST, dst);
        }

        capturesWest = capturesWest & ~BB_1_OR_8_RANK;
        while (capturesWest) { 
            u8 dst = _pop_lsb(capturesWest);
            consumer->template acceptx<color, PAWN, true, 0>(board, Move::make(dst - UpOffset - OFF_WEST, dst));
        }

        // make en passant
        Sq enPassantTarget = board->volatile_state()->enPassantTarget;
        if (enPassantTarget != NULL_SQ) {
            Bitboard movablePawns = ourPawns & lookup::pawnAttackBBs.values[!color][enPassantTarget];

            while (movablePawns) {
                Sq src = _pop_lsb(movablePawns);
                consumer->template acceptx<color, PAWN, true, MOVE_EN_PASSANT>(board, Move::make_en_passant(src, enPassantTarget));
            }
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

    // castling move gen //
    auto addCastlingMove = [&](u8 dstIndex, u8 rookFile, bool right) __attribute__((always_inline)) {
        if (rookFile == NULL_SQ) return;

        Bitboard unattackedCondition = (right ? (0b00000011ULL << index) : (0b00000011ULL << (index - 3)));
        if ((attacked & unattackedCondition) > 0) {
            return; // discard
        }

        consumer->template acceptx<color, KING, false, /* todo */ 0>(board, Move::make(index, dstIndex, (right ? MOVE_CASTLE_RIGHT : MOVE_CASTLE_LEFT)));
    };

    // normal movement
    Bitboard dstBB = lookup::kingMovementBBs.values[index] & ~attacked & ~friendlyBB;
    Bitboard ibb;

    if constexpr (_Options.quiets) {
        ibb = dstBB & ~enemyBB;
        while (ibb) {
            u8 dstIndex = _pop_lsb(ibb);
            consumer->template acceptx<color, KING, false, 0>(board, Move::make(index, dstIndex));
        }
    }

    if constexpr (_Options.captures) {
        ibb = dstBB & enemyBB;
        while (ibb) {
            u8 dstIndex = _pop_lsb(ibb);
            consumer->template acceptx<color, KING, true, 0>(board, Move::make(index, dstIndex));
        }
    }

    // castling moves are quiets
    if constexpr (!_Options.quiets) { 
        return;
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

/*                            */
/* Move ordering and supplier */
/*                            */

enum MoveOrderingType {
    NO_MOVE_ORDERING,      // No move ordering, doesn't perform logic
    COMPARE_MOVE_ORDERING, // Compares each move with every other move
    SCORE_MOVE_ORDERING    // Gives a score to each move
};

struct MoveScorePair {
    Move move;
    i16 score;
};

#define TC_MOVE_SCORE_ON_INSERT 1

/// @brief An automatically sorted, stack allocated move list which can 
/// be provided as a consumer for movegen functions.
/// @param _MoveOrderer Must be a class with a field `constexpr MoveOrderingType moveOrderingType` 
template<typename _MoveOrderer, u16 _Capacity>
struct MoveList {
    u16 count = 0;                                   // The amount of moves in the array.
    MoveScorePair moves[_Capacity];                  // The data array.

    forceinline void reset() {
        count = 0;
    }

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
    forceinline i16 score_move(Board* board, Move move) {
        return _MoveOrderer::template score_move<turn>(board, move);
    }

    template<bool turn>
    forceinline void sort_moves(Board* board) {
#ifndef TC_MOVE_INSERT_SORT
        if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING) {
#ifndef TC_MOVE_SCORE_ON_INSERT
            // score all moves
            for (int i = count; i >= 0; i--) {
                if (moves[i].move.null()) continue; 
                moves[i].score = score_move<turn>(board, moves[i].move);
            }
#endif
            // sort the array using custom comp
            std::sort((MoveScorePair*) &moves, (MoveScorePair*) &moves[count], [&](MoveScorePair a, MoveScorePair b) {
                return a.score < b.score; // ascending order
            });
        }
#endif
    }

    template<Color turn, PieceType pt, bool isCapture, u8 flags>
    forceinline void acceptx(Board* board, Move move) {
#ifndef TC_MOVE_INSERT_SORT
        i16 score = -32000;
#ifdef TC_MOVE_SCORE_ON_INSERT
        if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING)
            score = _MoveOrderer::template score_movex<turn, pt, isCapture, flags>(board, move);
#endif
        moves[count++] = { .move = move, .score = score };
#endif
    }

    template<Color turn>
    forceinline void accept(Board* board, Move move) {
#ifndef TC_MOVE_INSERT_SORT
        i16 score = -32000;
#ifdef TC_MOVE_SCORE_ON_INSERT
        if constexpr (_MoveOrderer::moveOrderingType == SCORE_MOVE_ORDERING)
            score = _MoveOrderer::template score_move<turn>(board, move);
#endif
        moves[count++] = { .move = move, .score = score };
#endif

#ifdef TC_MOVE_INSERT_SORT
        /* used to do sort on insertion, but its actually slower due to the necessity to move 
           data on insertion into the middle of the array */
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
    static forceinline i16 score_move(Board* board, Move move) {
        Piece moved = move.moved_piece(board);
        Piece captured = move.captured_piece(board);
        i16 captureScore = materialValuePerType[TYPE_OF_PIECE(captured)] * 500 - materialValuePerType[TYPE_OF_PIECE(moved)] * 50;
        i16 promotionScore = (move.flags == MOVE_PROMOTE_QUEEN) * 1500 + (move.flags == MOVE_PROMOTE_KNIGHT) * 500 /* + (move.flags == MOVE_PROMOTE_ROOK) * 300 + (move.flags == MOVE_PROMOTE_BISHOP) * 300*/;
        return (i16) ( 
                /* capture */ captureScore +
                /* promotion */ promotionScore
        );
    }

    template<Color turn, PieceType pt, bool isCapture, u8 flags>
    static forceinline i16 score_movex(Board* board, Move move) {
        i16 score = 0;
        constexpr Piece moved = PIECE_COLOR_FOR(turn) | pt;

        if constexpr (isCapture) {
            Piece captured = move.captured_piece(board);
            score += materialValuePerType[TYPE_OF_PIECE(captured)] * 500 - materialValuePerType[pt] * 50;
        }
        
        if constexpr (flags == MOVE_PROMOTE_QUEEN) score += 1500;
        else if constexpr (flags == MOVE_PROMOTE_ROOK || flags == MOVE_PROMOTE_BISHOP) score += 300;
        else if constexpr (flags == MOVE_PROMOTE_KNIGHT) score += 1000;
        else if constexpr (flags == MOVE_DOUBLE_PUSH) score += 50;
        
        return score;
    }
}; 

typedef u8 MoveSupplierStage;

/// @brief The stage of the move supplier.
enum {
    TT_MOVE  = 5,      // search the TT entry move, if available
    CAPTURES_INIT = 4, // init search all captures
    CAPTURES = 3,
    QUIETS_INIT = 2,   // init search quiet moves
    QUIETS = 1,

    STAGE_ENDED = 0
};

/// @brief Staged move picker/supplier. 
struct MoveSupplier {
    Board* board;
    MoveSupplierStage stage = CAPTURES_INIT;
    MoveList<BasicScoreMoveOrderer, MAX_MOVES> moveList;
    u8 index;

    Move ttMove;

    MoveSupplier(Board* board) {
        this->board = board;
    }

    inline void init_tt(TTEntry* entry) {
        if (entry->data.move.null()) {
            return;
        }

        ttMove = entry->data.move;
        stage = TT_MOVE;
    }

    forceinline bool has_next() {
        return stage > 0;
    }

    template<Color turn>
    forceinline Move next_move() {
        switch (stage) {
            /* tt move */
            case TT_MOVE:
                stage--;
                return ttMove;

            /* captures */
            case CAPTURES_INIT:
                gen_all_moves<decltype(moveList), movegenCapturesPL, turn>(board, &moveList);
                moveList.sort_moves<turn>(board);
                if (moveList.count == 0) {
                    stage -= 2;
                    return NULL_MOVE;
                }

                index = moveList.count;
                stage--;
            case CAPTURES:
                index--;
                if (!index) {
                    stage--;
                } else {
                    return moveList.moves[index].move;
                }
            
            /* quiets */
            case QUIETS_INIT:
                moveList.reset();
                gen_all_moves<decltype(moveList), movegenQuietsPL, turn>(board, &moveList);
                if (moveList.count == 0) {
                    stage -= 2;
                    return NULL_MOVE;
                }

                moveList.sort_moves<turn>(board);
                index = moveList.count;
                stage--;
            case QUIETS:
                index--;
                if (!index) {
                    stage--;
                } else {
                    return moveList.moves[index].move;
                }
        }
        
        return NULL_MOVE;
    }
};

}