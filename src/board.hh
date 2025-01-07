#pragma once

#include <memory.h>
#include <iostream>

#include "types.hh"
#include "bitboard.hh"
#include "piece.hh"
#include "move.hh"
#include "lookup.hh"

namespace tc {

static const char* startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/// @brief Non-trivial state of the board which may be restored from memory.
struct BoardState {
    /// @brief Bitboards of en passant target positions per color
    Bitboard enPassantTargets[2] = { 0, 0 };

    /// @brief Castling status per color
    u8 castlingStatus[2] = { CAN_CASTLE_L | CAN_CASTLE_R, CAN_CASTLE_L | CAN_CASTLE_R };

    /* Attacks, checkers, pinners, blockers, etc */
    /// @brief Bitboards of checking squares per mobility type per color
    Bitboard checkingSquares[2][MOBILITY_TYPE_COUNT]; 

    /// @brief All attacked squares by each color INCLUDING en passant.
    Bitboard attackBBsPerColor[2] = { 0, 0 };

    /// @brief All checkers on the king per color
    Bitboard checkers[2] = { 0, 0 };
};

/// @brief Extended move representation. This is not the format the moves are generated in,
/// instead the moves are cast into this format one move at a time and is used to store additional information
/// which might make undoing the move or evaluation of the move more accurate/performant.
template<bool active>
struct ExtMove {
    ExtMove(Move move): move(move) { }

    /// @brief The actual move.
    Move move;

    /* Extra information about the move */
    Piece piece;
    Piece captured;

    // The position (file) on the rank of the rook before castling
    u8 rookFile : 3 = 0;

    /* State */
    BoardState lastState;
};

/// @brief Representation of the board
struct Board {
public:
    Board();

    /* Piece Representation */

    /// @brief All pieces on the board stored in a 1 dimensional array
    /// From bottom-left to top-right (A1 to G8)
    Piece pieces[64];

    /// @brief The piece position bitboards per piece type per color,
    /// equivalent to being index by the piece value
    Bitboard pieceBBs[1 << (4 + 2)];

    /// @brief All pieces for each color
    Bitboard allPiecesPerColor[2];

    /// @brief All pieces on the board
    Bitboard allPieces = 0;

    /* General State */

    /// @brief Whether it is white's turn to move
    bool whiteTurn;

    /// @brief The amount of moves made.
    int ply;

    /* King State */

    /// @brief The index the king is currently on per color
    u8 kingIndexPerColor[2] = { 0, 0 };

    /// @brief The non trivial board state
    BoardState state;

public:
    inline BoardState* current_state() { return &state; }

    /// @brief Set the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to set the piece at.
    /// @param p The piece type to remove.
    template <bool updateState>
    inline void set_piece(u8 index, Piece p);

    /// @brief Remove the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to remove the piece from.
    /// @param p The piece type to remove.
    template<bool updateState>
    inline void unset_piece(u8 index, Piece p);

    /// @brief Make the given move on the board.
    /// This does not check whether the move is legal.
    /// @param extMove The move to make with extended information.
    template<bool color /* the side to make the mode */, bool useExtMove>
    void make_move_unchecked(ExtMove<useExtMove>* extMove);

    /// @brief Unmake the given move on the board, restoring all state to before the move.
    /// This does not check whether the move is legal.
    /// @param move The move to unmake.
    template<bool color /* the side which made the mode */, bool useExtMove>
    void unmake_move_unchecked(ExtMove<useExtMove>* extMove);

    /// @brief Check if the king of the given color is in check.
    template<bool color>
    inline bool is_in_check() const { return state.checkers[color] > 0; }

    /// @brief Find the first rook on the given rank from either side.
    template<bool color, bool right>
    inline u8 find_file_of_first_rook_on_rank(u8 rank) const;

    /// @brief Get the checking attack bitboard for the given mobility type.
    template<bool color, MobilityType mt>
    inline Bitboard checking_attack_bb() const;

    /// @brief Check whether a move of a piece with the given MT to the given index gives a check on the king of the given color.
    template<bool color, MobilityType mt>
    inline bool is_check_attack(u8 index) const;

    /// @brief Creates a bitboard set for all squares controlled by the given color piece with the given 'trivial' type 
    /// at the given position, assuming it is able to move, without masking out friendly pieces.
    template<PieceType pieceType>
    Bitboard trivial_attack_bb(u8 index) const;
    
    /// @brief Recalculate all attacking, pinning, checking, etc bitboards.
    inline void recalculate_state();

    /// @brief Load the current board status from the given FEN string
    /// @param str The FEN string or 'startpos'.
    void load_fen(const char* str);
};

template <bool updateState>
inline void Board::set_piece(u8 index, Piece p) {
    bool color = IS_WHITE_PIECE(p);
    pieces[index] = p;
    pieceBBs[p] |= 1ULL << index;
    allPiecesPerColor[color] |= 1ULL << index;    
    allPieces |= 1ULL << index;

    // check for king update
    if (TYPE_OF_PIECE(p) == KING) {
        kingIndexPerColor[color] = index;
    }

    if constexpr (updateState) {
        recalculate_state();
    }
}

template<bool updateState>
inline void Board::unset_piece(u8 index, Piece p) {
    pieces[index] = NULL_PIECE;
    pieceBBs[p] &= ~(1ULL << index);
    allPiecesPerColor[IS_WHITE_PIECE(p)] &= ~(1ULL << index);   
    allPieces &= ~(1ULL << index); 

    bool color = IS_WHITE_PIECE(p);

    if constexpr (updateState) {
        recalculate_state();
    }
}

template<bool color, bool useExtMove>
void Board::make_move_unchecked(ExtMove<useExtMove>* extMove) {
    Move move = extMove->move;

    // moved piece
    Piece piece = extMove->piece = pieces[move.src];
    Piece captured = extMove->captured = pieces[move.dst]; 

    if constexpr (useExtMove) {
        // store old state
        extMove->lastState = this->state;
    }

    // handle en passant and captures
    state.enPassantTargets[!color] = 0;
    if (move.is_en_passant()) {
        unset_piece<false>(move.dst - SIDE_OF_COLOR(color) * 8, captured);
    } else if (captured != NULL_PIECE) {
        unset_piece<false>(move.dst, captured);
    }

    if (move.is_double_push()) {
        // create en passant target
        u8 targetIndex = INDEX(FILE(move.dst), RANK(move.dst - SIDE_OF_PIECE(piece) * 8));
        state.enPassantTargets[color] |= (1ULL << targetIndex);
    }
    
    // remove from source position
    unset_piece<false>(move.src, piece);

    // promotions
    if (move.is_promotion()) {
        piece = color | move.promotion_piece();
    }
    
    // set piece at destination
    set_piece<false>(move.dst, piece);

    // castling
    if (move.is_castle()) {
        state.castlingStatus[color] |= CASTLED_L;
        u8 rookIndex = INDEX(/* todo: rook file */ 0, RANK(move.dst));
        Piece rook = color | ROOK;
        unset_piece<false>(rookIndex, rook);
        set_piece<false>(/* move behind king on the right */ move.dst + 1, rook);
    } else if (move.is_castle_right()) {
        state.castlingStatus[color] |= CASTLED_R;
        u8 rookIndex = INDEX(/* todo: rook file */ 0, RANK(move.dst));
        Piece rook = color | ROOK;
        unset_piece<false>(rookIndex, rook);
        set_piece<false>(/* move behind king on the left */ move.dst - 1, rook);
    } else if (TYPE_OF_PIECE(piece) == KING) {
        // remove castling rights on king move
        state.castlingStatus[color] &= ~(CAN_CASTLE_L | CAN_CASTLE_R);
    }

    // store old bitboards in ext move

    // update state
    recalculate_state();

    ply += 1;
}

template<bool color, bool useExtMove>
void Board::unmake_move_unchecked(ExtMove<useExtMove>* extMove) {
    Move move = extMove->move;
    Piece piece = extMove->piece;
    Piece captured = extMove->captured;

    if (move.is_promotion()) {
        // unset promoted piece, need to handle seperately bc
        // the bitboards for the promoted to type need to be updated before
        Piece p2 = color | move.promotion_piece();
        unset_piece<false>(move.dst, p2);
    } else {
        unset_piece<false>(move.dst, piece);
    }

    if (move.is_en_passant()) {
        // return the en passanted pawn and remove from dst position
        set_piece<false>(move.dst - (IS_WHITE_PIECE(piece) ? 8 : -8), captured);
        unset_piece<false>(move.dst, piece);
        // restore en passant target
        state.enPassantTargets[!color] |= (1ULL << move.dst);
    } else if (captured != NULL_PIECE) {
        // return captured piece to dst
        set_piece<false>(move.dst, captured);
    }

    if (move.is_double_push()) {
        // destroy en passant target
        state.enPassantTargets[color] &= ~(1ULL << INDEX(FILE(move.dst), RANK(move.dst - SIDE_OF_PIECE(piece) * 8)));
    }

    // return piece to source pos
    set_piece<false>(move.src, piece);

    // castling
    if (move.is_castle_left()) {
        u8 rookIndex = INDEX(extMove->rookFile, RANK(move.dst));
        Piece rook = color | ROOK;
        set_piece<false>(rookIndex, rook);
        unset_piece<false>(/* moved behind king on the right */ move.dst + 1, rook);
    } else if (move.is_castle_right()) {
        u8 rookIndex = INDEX(extMove->rookFile, RANK(move.dst));
        Piece rook = color | ROOK;
        set_piece<false>(rookIndex, rook);
        unset_piece<false>(/* moved behind king on the left */ move.dst - 1, rook);
    }

    if constexpr (useExtMove) {
        // restore state
        this->state = extMove->lastState;
    }

    ply -= 1;
}

template<bool color, bool right>
inline u8 Board::find_file_of_first_rook_on_rank(u8 rank) const {
    u8 bitline = ((pieceBBs[color * WHITE_PIECE | ROOK] >> rank * 8) & 0xFF);
    if constexpr (!right) {
        return __builtin_ctz(bitline);
    } else {
        return 7 - __builtin_clz(bitline << 24);
    }
}

template<bool color, MobilityType mt>
inline Bitboard Board::checking_attack_bb() const {
    if constexpr (mt == QUEEN_MOBILITY) {
        return checking_attack_bb<color, STRAIGHT>() | checking_attack_bb<color, DIAGONAL>();
    }

    return state.checkingSquares[color][mt];
}

template<bool color, MobilityType mt>
inline bool Board::is_check_attack(u8 index) const {
    return ((checking_attack_bb<color, mt>() >> index) & 0x1) > 0;
}

template<>
inline Bitboard Board::trivial_attack_bb<KNIGHT>(u8 index) const {
    return lookup::precalcKnightAttackBBs.values[index];
}

template<>
inline Bitboard Board::trivial_attack_bb<ROOK>(u8 index) const {
    return lookup::magic::rook_attack_bb(index, allPieces);
}

template<>
inline Bitboard Board::trivial_attack_bb<BISHOP>(u8 index) const {
    return lookup::magic::bishop_attack_bb(index, allPieces);
}

template<>
inline Bitboard Board::trivial_attack_bb<QUEEN>(u8 index) const {
    return trivial_attack_bb<ROOK>(index) | trivial_attack_bb<BISHOP>(index);
}

void Board::recalculate_state() {

}

inline Piece Move::moved_piece(Board const* b) const { return b->pieces[src]; };
inline Piece Move::captured_piece(Board const* b) const { return b->pieces[dst]; }
inline bool Move::is_capture(Board const* b) const { return (b->allPieces & (1ULL << dst)) > 1; }

}