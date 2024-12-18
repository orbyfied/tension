#pragma once

#include <memory.h>
#include <iostream>

#include "types.h"
#include "bitboard.h"
#include "piece.h"
#include "move.h"

struct StaticBoardOptions {
    constexpr StaticBoardOptions(bool ccb): calculateCheckingBitboards(ccb) { }

    const bool calculateCheckingBitboards;
};

static const char* startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/// @brief Extended move representation. This is not the format the moves are generated in,
/// instead the moves are cast into this format one move at a time and is used to store additional information
/// which might make undoing the move or evaluation of the move more accurate/performant.
template<bool active>
struct ExtMove {
    ExtMove(Move move): move(move) { }

    /// @brief The actual move.
    Move move;

    /// @brief The attack bitboards before the move, done to avoid recalculation.
    Bitboard lastAttackBBs[2];
};

/// @brief Representation of the board
template<StaticBoardOptions const& _BoardOptions>
struct Board {
    Board() {
        memset(&pieces, NULL_PIECE, 64);
        memset(&pieceBBs, 0, 32 * sizeof(u64));
        memset(&allPiecesBBPerColor, 0, 2 * sizeof(u64));
        memset(&checkingSquaresBBs, 0, MOBILITY_TYPE_COUNT * 2 * sizeof(u64));
        // memset(&castlingRookIndices, 0, 2 * 2 * sizeof(u8));
        allPiecesBB = 0;
    }

    ~Board() {

    }

    /// @brief All pieces on the board stored in a 1 dimensional array
    /// From bottom-left to top-right (A1 to G8)
    Piece pieces[64];

    /// @brief The piece position bitboards per piece type per color,
    /// equivalent to being index by the piece value
    u64 pieceBBs[1 << (4 + 2)];

    /// @brief All pieces for each color
    u64 allPiecesBBPerColor[2];

    /// @brief All pieces on the board
    u64 allPiecesBB = 0;

    /// @brief Whether it is white's turn to move
    bool whiteTurn;

    /// @brief The index the king is currently on per color
    u8 kingIndexPerColor[2] = { 0, 0 };

    /// @brief Bitboards of checking squares per mobility type per color
    u64 checkingSquaresBBs[2][MOBILITY_TYPE_COUNT]; 

    /// @brief Bitboars of en passant positions per color
    u64 enPassantTargetBBs[2] = { 0, 0 };

    /// @brief Castling status per color
    u8 castlingStatus[2] = { CAN_CASTLE_L | CAN_CASTLE_R, CAN_CASTLE_L | CAN_CASTLE_R };

    /// @brief All attacked squares by each color EXCLUDING en passant.
    u64 attackBBsPerColor[2] = { 0, 0 };

    /// @brief Set the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to set the piece at.
    /// @param p The piece type to remove.
    template <bool updateAdvancedBitboards>
    inline void set_piece(u8 index, Piece p) {
        bool color = IS_WHITE_PIECE(p);
        pieces[index] = p;
        pieceBBs[p] |= 1ULL << index;
        allPiecesBBPerColor[color] |= 1ULL << index;    
        allPiecesBB |= 1ULL << index;

        if constexpr (updateAdvancedBitboards) {
            update_attack_bb_pieceadd(index, p);
        }

        if constexpr (_BoardOptions.calculateCheckingBitboards) {
            // check for king update
            if (TYPE_OF_PIECE(p) == KING) {
                kingIndexPerColor[color] = index;
                recalculate_checking_bb_kingmove(color, index, (u64*)&checkingSquaresBBs[color]);
            } else {
                update_checking_bb_piecemove<false>(color, index);
            }
        }
    }

    /// @brief Remove the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to remove the piece from.
    /// @param p The piece type to remove.
    template<bool updateAdvancedBitboards>
    inline void unset_piece(u8 index, Piece p) {
        pieces[index] = NULL_PIECE;
        pieceBBs[p] &= ~(1ULL << index);
        allPiecesBBPerColor[IS_WHITE_PIECE(p)] &= ~(1ULL << index);   
        allPiecesBB &= ~(1ULL << index); 

        bool color = IS_WHITE_PIECE(p);

        if constexpr (updateAdvancedBitboards) {
            // todo: update attack bitboard with piece removal (prob recalc fully)
        }

        if constexpr (_BoardOptions.calculateCheckingBitboards) {
            // king removal lol
            if (TYPE_OF_PIECE(p) == KING) {
                kingIndexPerColor[color] = 0;
                memset(&checkingSquaresBBs[color], 0, MOBILITY_TYPE_COUNT * sizeof(u64));
            } else if (TYPE_OF_PIECE(p) > KNIGHT) {
                update_checking_bb_piecemove<true>(color, index);
            }
        }
    }

    /// @brief Make the given move on the board.
    /// This does not check whether the move is legal.
    /// @param move The move to make.
    template<bool useExtMove>
    void make_move_unchecked(ExtMove<useExtMove>* extMove) {
        Move move = extMove->move;
        Piece p = move.piece;
        bool color = IS_WHITE_PIECE(p);

        if constexpr (useExtMove) {
            // store info for undo
            extMove->lastAttackBBs[0] = attackBBsPerColor[0];
            extMove->lastAttackBBs[1] = attackBBsPerColor[1];
        }

        // handle en passant and captures
        enPassantTargetBBs[!color] = 0;
        if (move.enPassant) {
            unset_piece<false>(move.dst - SIDE_OF_COLOR(color) * 8, move.captured);
            // // remove en passant target
            // enPassantTargetBBs[!color] &= ~(1ULL << move.dst);
        } else if (move.captured != NULL_PIECE) {
            unset_piece<false>(move.dst, move.captured);
        }

        if (move.isDoublePush) {
            // create en passant target
            enPassantTargetBBs[color] |= (1ULL << INDEX(FILE(move.dst), RANK(move.dst - SIDE_OF_PIECE(p) * 8)));
        }
        
        // remove from source position
        unset_piece<false>(move.src, p);

        // promotions
        if (move.promotionType != NULL_PIECE_TYPE) {
            p = COLOR_OF_PIECE(p) | move.promotionType;
        }
        
        // set piece at destination
        set_piece<true>(move.dst, p);

        // castling
        if ((move.castleOperations & MOVE_CASTLE_L) > 0) {
            castlingStatus[color] |= CASTLED_L;
            u8 rookIndex = INDEX(move.rookFile, RANK(move.dst));
            Piece rook = COLOR_OF_PIECE(p) | ROOK;
            unset_piece<false>(rookIndex, rook);
            set_piece<true>(/* move behind king on the right */ move.dst + 1, rook);
        } else if ((move.castleOperations & MOVE_CASTLE_R) > 0) {
            castlingStatus[color] |= CASTLED_R;
            u8 rookIndex = INDEX(move.rookFile, RANK(move.dst));
            Piece rook = COLOR_OF_PIECE(p) | ROOK;
            unset_piece<false>(rookIndex, rook);
            set_piece<true>(/* move behind king on the left */ move.dst - 1, rook);
        } else /* castling rights */ {
            castlingStatus[color] &= ~move.castleOperations;
        }
    }

    /// @brief Unmake the given move on the board, restoring all state to before the move.
    /// This does not check whether the move is legal.
    /// @param move The move to unmake.
    template<bool useExtMove>
    void unmake_move_unchecked(ExtMove<useExtMove>* extMove) {
        Move move = extMove->move;
        Piece p = pieces[move.dst];
        bool color = IS_WHITE_PIECE(p);

        if constexpr (useExtMove) {
            // restore info
            attackBBsPerColor[0] = extMove->lastAttackBBs[0];
            attackBBsPerColor[1] = extMove->lastAttackBBs[1];
        }

        if (move.promotionType != NULL_PIECE_TYPE) {
            // unset promoted piece, need to handle seperately bc
            // the bitboards for the promoted to type need to be updated before
            Piece p2 = p;
            if (move.promotionType != NULL_PIECE_TYPE) {
                p2 = COLOR_OF_PIECE(p) | move.promotionType;
            }

            unset_piece<false>(move.dst, p2);
        } else {
            unset_piece<false>(move.dst, p);
        }

        if (move.enPassant) {
            // return the en passanted pawn and remove from dst position
            set_piece<false>(move.dst - (IS_WHITE_PIECE(p) ? 8 : -8), move.captured);
            unset_piece<false>(move.dst, p);
            // restore en passant target
            enPassantTargetBBs[!color] |= (1ULL << move.dst);
        } else if (move.captured != NULL_PIECE) {
            // return captured piece to dst
            set_piece<false>(move.dst, move.captured);
        }

        if (move.isDoublePush) {
            // destroy en passant target
            enPassantTargetBBs[color] &= ~(1ULL << INDEX(FILE(move.dst), RANK(move.dst - SIDE_OF_PIECE(p) * 8)));
        }

        // return piece to source pos
        set_piece<false>(move.src, p);

        // castling
        if ((move.castleOperations & MOVE_CASTLE_L) > 0) {
            castlingStatus[color] &= ~CASTLED_L;
            u8 rookIndex = INDEX(move.rookFile, RANK(move.dst));
            Piece rook = COLOR_OF_PIECE(p) | ROOK;
            set_piece<false>(rookIndex, rook);
            unset_piece<false>(/* moved behind king on the right */ move.dst + 1, rook);
        } else if ((move.castleOperations & MOVE_CASTLE_R) > 0) {
            castlingStatus[color] &= ~CASTLED_R;
            u8 rookIndex = INDEX(move.rookFile, RANK(move.dst));
            Piece rook = COLOR_OF_PIECE(p) | ROOK;
            set_piece<false>(rookIndex, rook);
            unset_piece<false>(/* moved behind king on the left */ move.dst - 1, rook);
        } else /* castling rights */ {
            castlingStatus[color] |= move.castleOperations;
        }
    }

    /// @brief Check if the king of the given color is in check.
    inline bool is_in_check(bool color) {
        return (pieceBBs[WHITE * color | KING] & attackBBsPerColor[!color]) > 0;
    }

    inline void recalculate_checking_bb_kingmove(bool color, u8 kingIndex, u64* bbs) {
        u8 file = FILE(kingIndex);
        u8 rank = RANK(kingIndex);

        // pawn checks
        u8 vdir = SIDE_OF_COLOR(color) * 8;
        u64 pawnBB = 0;
        if (kingIndex + vdir < 64) {
            if (file > 0) pawnBB |= 1ULL << (kingIndex + vdir - 1);
            if (file < 7) pawnBB |= 1ULL << (kingIndex + vdir + 1);
        }

        bbs[PAWN_MOBILITY] = pawnBB;

        // knight checks
        u64 knightBB = 0;
        if (file > 1 && rank < 6) knightBB |= 1ULL << (kingIndex + 8 * 2 - 1);
        if (file > 2 && rank < 7) knightBB |= 1ULL << (kingIndex + 8 * 1 - 2);
        if (file > 1 && rank > 1) knightBB |= 1ULL << (kingIndex - 8 * 2 - 1);
        if (file > 2 && rank > 0) knightBB |= 1ULL << (kingIndex - 8 * 1 - 2);
        if (file < 7 && rank < 6) knightBB |= 1ULL << (kingIndex + 8 * 2 + 1);
        if (file < 6 && rank < 7) knightBB |= 1ULL << (kingIndex + 8 * 1 + 2);
        if (file < 7 && rank > 1) knightBB |= 1ULL << (kingIndex - 8 * 2 + 1);
        if (file < 6 && rank > 0) knightBB |= 1ULL << (kingIndex - 8 * 1 + 2);
        bbs[KNIGHT_MOBILITY] = knightBB;

        // todo: slider ...
    }

    template<bool remove>
    inline void update_checking_bb_piecemove(bool color, u8 pieceIndex) {
        u8 kingIndex = kingIndexPerColor[color];
        u64 diagBB = checkingSquaresBBs[color][DIAGONAL];
        u64 straightBB = checkingSquaresBBs[color][STRAIGHT];

        if ((diagBB & (1ULL << pieceIndex)) > 0) {
            // recalculate diagonal bb
        } else if ((straightBB & (1ULL << pieceIndex)) > 0) {
            // recalculate straight bb
        }
    }

    template<bool right>
    inline u8 find_file_of_first_rook_on_rank(bool color, u8 rank) {
        u8 bitline = ((pieceBBs[color * WHITE | ROOK] >> rank * 8) & 0xFF);
        if constexpr (!right) {
            return __builtin_ctz(bitline);
        } else {
            return 7 - __builtin_clz(bitline << 24);
        }
    }

    /// @brief Check whether a move of a piece with the given MT to the given index gives a check on the king of the given color.
    inline bool is_check_attack(bool color, u8 index, MobilityType mt) {
        if constexpr (_BoardOptions.calculateCheckingBitboards) {
            return ((checkingSquaresBBs[color][mt] >> index) & 0x1) > 0;
        } else {
            return 0; // todo: alt method
        }
    }

    /// @brief Load the current board status from the given FEN string
    /// @param str The FEN string or 'startpos'.
    void load_fen(const char* str) {
        int len = strlen(str);
        if (strcmp(str, "startpos") == 0) {
            load_fen(startFEN);
            return;
        }

        // parse piece placement
        u8 rank = 7;
        u8 file = 0;
        int i = 0;
        while (i < len && str[i] != ' ')
        {
            if (str[i] == '/') {
                rank--;
                file = 0;
                i++;
                continue;
            }

            // parse skip
            if (isdigit(str[i])) {
                file += str[i] - '0';
                i++;
                continue;
            }
            

            // parse pieces
            char pieceChar = str[i];
            u8 color = isupper(pieceChar) * WHITE;
            PieceType type = charToPieceType(pieceChar);
            this->set_piece<true>(INDEX(file, rank), type | color);
            file += 1;
            i++;
        }
    }

    /* Evaluation and debug helpers */
    int count_material(bool side) {
        u8 colorVal = WHITE * side;
        int count = 0;
        count += __builtin_popcountll(pieceBBs[colorVal | PAWN]) * materialValuePerType[0];
        count += __builtin_popcountll(pieceBBs[colorVal | KNIGHT]) * materialValuePerType[1];
        count += __builtin_popcountll(pieceBBs[colorVal | BISHOP]) * materialValuePerType[2];
        count += __builtin_popcountll(pieceBBs[colorVal | ROOK]) * materialValuePerType[3];
        count += __builtin_popcountll(pieceBBs[colorVal | QUEEN]) * materialValuePerType[4];
        count += __builtin_popcountll(pieceBBs[colorVal | KING]) * materialValuePerType[5];
        return count;
    }

    /* Bitboard Attack Generation */

    /// @brief Update the attack bitboard excluding en passant for the adding of the given piece in the current state of the board.
    /// This includes the attacks of the piece but also the blocking and unblocking of other attacks.
    inline void update_attack_bb_pieceadd(u8 index, Piece p) {
        u8 file = FILE(index);
        u8 rank = RANK(index);
        bool color = IS_WHITE_PIECE(p);

        // check for blocks/unblocks on other attacks

        // set own attack bitboard
        u64 ourAttackBB = attackBBsPerColor[color];
        switch (TYPE_OF_PIECE(p))
        {
        case PAWN: {
            constexpr PrecalcPawnAttackBBs pawnBBs { }; // lookup table
            ourAttackBB |= pawnBBs.values[color][index];
            break; }
        case KNIGHT: {
            constexpr PrecalcKnightAttackBBs knightBBs { }; // lookup table
            ourAttackBB |= knightBBs.values[index];
            break; }
        case KING: { 
            constexpr PrecalcKingAttackBBs kingBBs { }; // lookup table
            ourAttackBB |= kingBBs.values[index];
            break; }

        /* slider pieces */
        case BISHOP: break;
        case ROOK: break;
        case QUEEN: break;
        
        default: break;
        }

        attackBBsPerColor[color] = ourAttackBB;
    }
};