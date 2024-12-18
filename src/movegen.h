#include "board.h"
#include "move.h"

/// @brief The compile-time movegen options
struct StaticMovegenOptions {
    const bool verifyChecksOnOpponent;
    const bool verifyCheckLegal;
};

constexpr StaticMovegenOptions defaultMovegenOptions { .verifyChecksOnOpponent = true, .verifyCheckLegal = true };

/// @brief Generate all pseudo-legal moves on the board for the given color.
/// This adds the check flag to attacking moves which generate a check, but it is up
/// to the search function to verify whether moves don't put the king in check.
template<typename _Consumer, StaticMovegenOptions const& _Options = defaultMovegenOptions, StaticBoardOptions const& _BoardOptions>
u8 gen_pseudo_legal_moves(Board<_BoardOptions>* board, bool turn, _Consumer* consumer) {
    std::cout << "c" << std::endl;
    u8 count = 0;

    u64 slidingCheckingBB = 0;
    u64 nonSlidingCheckingBB = 0;
    u64 checkingBB = 0;
    if constexpr (!_Options.verifyCheckLegal) {
        // consumer and constraints
        u64 slidingCheckingBB = get_unobstructed_sliding_checking_bb(board, turn);
        u64 nonSlidingCheckingBB = get_non_sliding_checking_bb(board, turn);
        u64 checkingBB = slidingCheckingBB | nonSlidingCheckingBB;
    }

    FunctionMoveConsumer moveConsumer([&](Move move) {
        if (checkingBB) {
            u64 mutableCheckingBB = checkingBB;

            // must be a king move which avoids check, a capture which removes
            // the attacker or a move which blocks the attack with another piece

            // since king moves already cant move into check, just check if its a king piece
            if (TYPE_OF_PIECE(move.piece) == KING) {
                consumer->accept(move);
                return;
            }

            // check for capture removing the attacker
            if (move.captured != NULL_PIECE_TYPE) {
                u8 captureIndex = move.captureIndex();
                mutableCheckingBB &= ~(1ULL << captureIndex);
                if (mutableCheckingBB == 0) {
                    consumer->accept(move); // captured only attacker
                    return;
                }
            }

            // check for block of rooks using bit masks
            // if there are non sliding pieces checking, there is no point in performing this check
            // blocks by bishops or diagonally by queens have to be evaluated in the search function
            if (!nonSlidingCheckingBB) {
                u8 kingIndex = board->kingIndexPerColor[turn];
                u8 kingFile = FILE(kingIndex);
                u8 kingRank = RANK(kingIndex);
                u8 file = FILE(move.dst);
                u8 rank = RANK(move.dst);

                mutableCheckingBB &= ~bitMaskExcludeFiles(kingFile, file);
                mutableCheckingBB &= ~bitMaskExcludeRanks(kingRank, rank);
                if (!mutableCheckingBB) {
                    // we blocked the check
                    consumer->accept(move);
                }
            }

            return;
        }

        // no pin detection we do that in the search, similar to
        // diagonal check blocking detection

        consumer->accept(move);
    });

    // use pieces bitboard to find indices of all pieces
    u64 bbAll = board->allPiecesBBPerColor[turn];
    u8 index;
    BITBOARD_INDEX_LOOP_MUT_LE(bbAll, index) {
        Piece p = board->pieces[index];
        
        // generate moves for the piece
        switch (TYPE_OF_PIECE(p))
        {
        case PAWN: count += movegen_pawn<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); break;
        case KNIGHT: count += movegen_knight<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); break;
        case BISHOP: count += movegen_bishop<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); break;
        case ROOK: count += movegen_rook<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); break;
        case QUEEN: count += movegen_rook<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); movegen_bishop<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); break;
        case KING: count += movegen_king<decltype(moveConsumer), _Options, _BoardOptions>(board, &moveConsumer, index, p); break;
        default:  break;
        }
    } CLOSE

    return count;
}

/// @brief Get a bitboard of all checking, non-sliding pieces.
template<StaticBoardOptions const& _BoardOptions>
inline u64 get_non_sliding_checking_bb(Board<_BoardOptions>* board, bool kingColor) {
    u8 colValue = !kingColor * WHITE;
    return 
        (board->checkingSquaresBBs[kingColor][PAWN_MOBILITY] & board->pieceBBs[colValue | PAWN]) |
        (board->checkingSquaresBBs[kingColor][KNIGHT_MOBILITY] & board->pieceBBs[colValue | KNIGHT]);
}

/// @brief Get a bitboard of all checking, sliding pieces, note that these will not be obstructed by eachother.
template<StaticBoardOptions const& _BoardOptions>
inline u64 get_unobstructed_sliding_checking_bb(Board<_BoardOptions>* board, bool kingColor) {
    if constexpr (_BoardOptions.calculateCheckingBitboards) {
        u8 colValue = !kingColor * WHITE;
        u64 straight = board->checkingSquaresBBs[kingColor][STRAIGHT];
        u64 diag = board->checkingSquaresBBs[kingColor][DIAGONAL];
        return 
            (straight & board->pieceBBs[colValue | ROOK]) |
            (diag & board->pieceBBs[colValue | BISHOP]) |
            ((straight | diag) & board->pieceBBs[colValue | QUEEN]);
    } else {
        return 0;
    }
} 

/// @brief Get a bitboard of all checking pieces, note that these will not be obstructed by eachother.
template<StaticBoardOptions const& _BoardOptions>
inline u64 get_unobstructed_checking_bb(Board<_BoardOptions>* board, bool kingColor) {
    if constexpr (_BoardOptions.calculateCheckingBitboards) {
        return get_non_sliding_checking_bb(board, kingColor) | get_unobstructed_checking_bb(board, kingColor);
    } else {
        return 0;
    }
}

/* Templated Piece Move Generation */

template<typename _Consumer, StaticMovegenOptions const& _Options, StaticBoardOptions const& _BoardOptions>
inline int movegen_pawn(Board<_BoardOptions>* board, _Consumer* consumer, u8 index, Piece p) {
    bool color = IS_WHITE_PIECE(p);
    Move move;
    u8 dst;
    u8 vdir = SIDE_OF_COLOR(color) * 8;
    u8 c = 0;

    // pawn move processing //
    auto acceptMove = [&](Move move) __attribute__((always_inline)) {
        // check for promotion 
        u8 rank = RANK(move.dst);
        if (rank == 0 || rank == 7) {
            move.promotionType = KNIGHT;
            consumer->accept(move);
            move.promotionType = BISHOP;
            consumer->accept(move);
            move.promotionType = ROOK;
            consumer->accept(move);
            move.promotionType = QUEEN;
            consumer->accept(move);
            c += 4;
            return;
        }

        consumer->accept(move);
        c++;
    };

    // pushing 1
    dst = index + vdir;
    if (dst >= 0 && dst < 64) {
        if (((board->allPiecesBB >> dst) & 0x1) == 0) { 
            move = { .piece = p, .src = index, .dst = dst, .isCheck = board->is_check_attack(1 - color, dst, PAWN_MOBILITY) };
            acceptMove(move);
        }
    }

    // pushing 2
    // pushing 2 squares can never be a promotion, so we dont
    // have to use acceptMove()
    u8 rank = RANK(index);
    if ((rank == 1 && IS_WHITE_PIECE(p) || rank == 6 && !IS_WHITE_PIECE(p)) && ((board->allPiecesBB >> dst) & 0x1) == 0) {
        dst += vdir;
        if (((board->allPiecesBB >> dst) & 0x1) == 0) { 
            move = { .piece = p,  .src = index, .dst = dst, .isCheck = board->is_check_attack(1 - color, dst, PAWN_MOBILITY), .isDoublePush = true };
            consumer->accept(move);
            c++;
        }
    }

    // capturing
    u8 file = FILE(index);
    u8 front = index + vdir;
    if (front >= 0 && front < 64) {
        u64 enemyBB = board->allPiecesBBPerColor[1 - IS_WHITE_PIECE(p)];
        if (file > 0 && ((enemyBB >> (dst = front - 1)) & 0x1) == 1) {
            move = { .piece = p,  .src = index, .dst = dst, .captured = board->pieces[dst], .isCheck = board->is_check_attack(1 - color, dst, PAWN_MOBILITY) };
            acceptMove(move);
        }

        if (file < 7 && ((enemyBB >> (dst = front + 1)) & 0x1) == 1) {
            move = { .piece = p,  .src = index, .dst = dst, .captured = board->pieces[dst], .isCheck = board->is_check_attack(1 - color, dst, PAWN_MOBILITY) };
            acceptMove(move);
        }

        u64 enPassant = board->enPassantTargetBBs[1 - IS_WHITE_PIECE(p)];
        if (enPassant > 0) {
            // en passant can never be a promotion, so we dont
            // have to use acceptMove()
            if (file > 0 && ((enPassant >> (dst = front - 1)) & 0x1) == 1) {
                move = { .piece = p,  .src = index, .dst = dst, .captured = board->pieces[index - 1], .isCheck = board->is_check_attack(1 - color, dst, PAWN_MOBILITY), .enPassant = true };
                consumer->accept(move);
                c++;
            }

            if (file < 7 && ((enPassant >> (dst = front + 1)) & 0x1) == 1) {
                move = { .piece = p,  .src = index, .dst = dst, .captured = board->pieces[index + 1], .isCheck = board->is_check_attack(1 - color, dst, PAWN_MOBILITY), .enPassant = true };
                consumer->accept(move);
                c++;
            }
        }
    }

    return c;
}

template<typename _Consumer, StaticMovegenOptions const& _Options, StaticBoardOptions const& _BoardOptions>
inline int movegen_knight(Board<_BoardOptions>* board, _Consumer* consumer, u8 index, Piece p) {
    int count = 0;
    u64 friendlyBB = board->allPiecesBBPerColor[IS_WHITE_PIECE(p)];
    u64 enemyBB = board->allPiecesBBPerColor[!IS_WHITE_PIECE(p)];

    // knight move processing //
    auto addMoveTo = [&](u8 dst) __attribute__((always_inline)) {
        if (((friendlyBB >> dst) & 0x1) > 0) return; // discard

        Move move = { .piece = p, .src = index, .dst = dst, .isCheck = board->is_check_attack(!IS_WHITE_PIECE(p), dst, KNIGHT_MOBILITY) };
        if (((enemyBB >> dst) & 0x1) > 0) {
            move.captured = board->pieces[dst];
        }

        consumer->accept(move);
        count++;
    };

    u8 file = FILE(index);
    u8 rank = RANK(index);
    if (file > 0 && rank < 6) addMoveTo(index + 8 * 2 - 1);
    if (file > 1 && rank < 7) addMoveTo(index + 8 * 1 - 2);
    if (file > 0 && rank > 1) addMoveTo(index - 8 * 2 - 1);
    if (file > 1 && rank > 0) addMoveTo(index - 8 * 1 - 2);
    if (file < 7 && rank < 6) addMoveTo(index + 8 * 2 + 1);
    if (file < 6 && rank < 7) addMoveTo(index + 8 * 1 + 2);
    if (file < 7 && rank > 1) addMoveTo(index - 8 * 2 + 1);
    if (file < 6 && rank > 0) addMoveTo(index - 8 * 1 + 2);

    return count;
}

template<typename _Consumer, StaticMovegenOptions const& _Options, StaticBoardOptions const& _BoardOptions>
inline int movegen_king(Board<_BoardOptions>* board, _Consumer* consumer, u8 index, Piece p) {
    int count = 0;
    bool color = IS_WHITE_PIECE(p);
    u64 friendlyBB = board->allPiecesBBPerColor[color];
    u64 enemyBB = board->allPiecesBBPerColor[!color];
    u8 file = FILE(index);
    u8 rank = RANK(index);

    // king move processing //
    auto addMoveTo = [&](u8 dst) __attribute__((always_inline)) {
        if (((friendlyBB >> dst) & 0x1) > 0) return; // discard

        Move move = { .piece = p, .src = index, .dst = dst, .isCheck = false };
        if (!kingmove_check_legal(board, color, move)) return; // discard

        if (((enemyBB >> dst) & 0x1) > 0) {
            move.captured = board->pieces[dst];
        }

        consumer->accept(move);
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
        u8 bitline = (board->allPiecesBB >> (rank * 8)) & 0xFF;
        u8 shiftedBitline = (right ? bitline >> (file + 1) : bitline << file);
        if (right && __builtin_ctz(shiftedBitline) < rookFile - file - 1 || 
                        __builtin_clz(shiftedBitline << 24) < file - rookFile - 1) {
            return; // discard
        }

        // check if castled rook is check for opponent
        bool isCheck = board->is_check_attack(!color, INDEX(newRookFile, rank), STRAIGHT);

        Move move = { .piece = p, .src = index, .dst = kingDstIndex, .isCheck = isCheck, .castleOperations = 
                    (u8)(right ? CASTLED_R : CASTLED_L), .rookFile = rookFile,  };
        if (!kingmove_check_legal(board, color, move)) return; // discard

        consumer->accept(move);
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
    u8 flags = board->castlingStatus[color];
    if ((flags & CAN_CASTLE_R) > 0) { addCastlingMove(file + 2, board->template find_file_of_first_rook_on_rank<true>((color), rank), true); }
    if ((flags & CAN_CASTLE_L) > 0) { addCastlingMove(file - 2, board->template find_file_of_first_rook_on_rank<false>((color), rank), false); }

    // todo: castling
    return count;
}

template<StaticBoardOptions const& _BoardOptions>
inline bool kingmove_check_legal(Board<_BoardOptions>* board, bool color, Move move) {
    // check moving into attach from other king
    u8 otherKingIndex = board->kingIndexPerColor[!color];
    u8 kingIndex = move.dst;
    if (FILE(kingIndex) >= FILE(otherKingIndex) - 1 && FILE(kingIndex) <= FILE(otherKingIndex) + 1 &&
        RANK(kingIndex) >= RANK(otherKingIndex) - 1 && RANK(kingIndex) <= RANK(otherKingIndex) + 1) return false; 
    u8 otherKingRank = RANK(otherKingRank);

    // check moving into attack
    u64 bbs[MOBILITY_TYPE_COUNT];
    board->recalculate_checking_bb_kingmove(color, move.dst, (u64*)&bbs);
    if ((board->pieceBBs[PAWN | ((!color) << 4)] & bbs[PAWN_MOBILITY]) > 0) return false;
    if ((board->pieceBBs[KNIGHT | ((!color) << 4)] & bbs[KNIGHT_MOBILITY]) > 0) return false;
    u64 straight = bbs[STRAIGHT];
    u64 diag = bbs[DIAGONAL];
    if ((board->pieceBBs[((1 - color) << 4) | BISHOP] & diag) > 0) return false;
    if ((board->pieceBBs[((1 - color) << 4) | ROOK] & straight) > 0) return false;
    if ((board->pieceBBs[((1 - color) << 4) | QUEEN] & (diag | straight)) > 0) return false;
    
    return true;
}

template<typename _Consumer, StaticMovegenOptions const& _Options, StaticBoardOptions const& _BoardOptions>
inline int movegen_rook(Board<_BoardOptions>* board, _Consumer* consumer, u8 index, Piece p) {
    int count = 0;
    bool color = IS_WHITE_PIECE(p);
    u8 file = FILE(index);
    u8 rank = RANK(index);

    u64 enemyBB = board->allPiecesBBPerColor[!color];
    u64 friendlyBB = board->allPiecesBBPerColor[color];

    // rook move verification //
    auto addMoveTo = [&](u8 dst) __attribute__((always_inline)) {
        if (((friendlyBB >> dst) & 0x1) > 0) return; // discard

        Move move = { .piece = p, .src = index, .dst = dst, .isCheck = board->is_check_attack(!color, dst, STRAIGHT) };
        if (((enemyBB >> dst) & 0x1) > 0) {
            move.captured = board->pieces[dst];
        }

        consumer->accept(move);
        count++;
    };

    // horizontal movement //
    // Setup of rank: 1 1 1 0 1 1 0 1
    //                H G F E D C B A
    
    // to test left movement, test
    // 0 0 1 0
    // D C B A
    // so clz_u8(~bitLine << file)

    // to test right movement, test
    // 0 0 0
    // so ctz_u8(~bitLine >> file + 1)

    u8 hObstructedBitLine = 0xFF & ~((board->allPiecesBB >> rank * 8) & 0xFF);
    int leftMovement = /* edge */ (file > 0) * (/* cz */ __builtin_clz(~(hObstructedBitLine << 24 << file)) + /* account for capture */ 1);
    if (leftMovement > file) leftMovement = file;
    int rightMovement = /* edge */ (file < 7) * (/* cz */ __builtin_ctz(~((u32)hObstructedBitLine >> (file + 1))) + /* account for capture */ 1);
    if (rightMovement > 7 - file) rightMovement = 7 - file;

    // directly generate horizontal moves from the movement values
    for (u32 i = 1; i <= leftMovement; i++) addMoveTo(index - i);
    for (u32 i = 1; i <= rightMovement; i++) addMoveTo(index + i);

    // vertical movement //
    // TODO: optimize, cba rn

    for (int r = rank - 1; r >= 0; r--) {
        u8 index = INDEX(file, r);
        addMoveTo(index);
        if (((board->allPiecesBB >> index) & 0x1) > 0) {
            break;
        }
    }

    for (int r = rank + 1; r < 8; r++) {
        u8 index = INDEX(file, r);
        addMoveTo(index);
        if (((board->allPiecesBB >> index) & 0x1) > 0) {
            break;
        }
    }

    return count;
}

template<typename _Consumer, StaticMovegenOptions const& _Options, StaticBoardOptions const& _BoardOptions>
inline int movegen_bishop(Board<_BoardOptions>* board, _Consumer* consumer, u8 index, Piece p) {
    int count = 0;
    bool color = IS_WHITE_PIECE(p);
    u8 file = FILE(index);
    u8 rank = RANK(index);

    u64 enemyBB = board->allPiecesBBPerColor[!color];
    u64 friendlyBB = board->allPiecesBBPerColor[color];

    // bishop move verification //
    auto addMoveTo = [&](u8 dst) __attribute__((always_inline)) {
        if (((friendlyBB >> dst) & 0x1) > 0) return; // discard

        Move move = { .piece = p, .src = index, .dst = dst, .isCheck = board->is_check_attack(!color, dst, STRAIGHT) };
        if (((enemyBB >> dst) & 0x1) > 0) {
            move.captured = board->pieces[dst];
        }

        consumer->accept(move);
        count++;
    };

    // TODO: optimize
    int x, y;

    x = file + 1; y = rank + 1;
    while (x < 8 && y < 8)
    {
        u8 index = INDEX(x, y);
        addMoveTo(index);
        if ((board->allPiecesBB >> index) & 0x1) break;
        x++; y++;
    }

    x = file + 1; y = rank - 1;
    while (x < 8 && y >= 0)
    {
        u8 index = INDEX(x, y);
        addMoveTo(index);
        if ((board->allPiecesBB >> index) & 0x1) break;
        x++; y--;
    }

    x = file - 1; y = rank + 1;
    while (x >= 0 && y < 8)
    {
        u8 index = INDEX(x, y);
        addMoveTo(index);
        if ((board->allPiecesBB >> index) & 0x1) break;
        x--; y++;
    }

    x = file - 1; y = rank - 1;
    while (x >= 0 && y >= 0)
    {
        u8 index = INDEX(x, y);
        addMoveTo(index);
        if ((board->allPiecesBB >> index) & 0x1) break;
        x--; y--;
    }
    
    return count;
}