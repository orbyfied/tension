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

/// @brief Non-trivial or unrecoverable state of the board which may be restored from memory.
struct VolatileBoardState {
    /// @brief The amount of moves made without a capture or pawn move
    i16 rule50Ply;

    /// @brief The current en passant target
    u8 enPassantTarget = NULL_SQ;

    /// @brief Castling status per color
    u8 castlingStatus[2] = { CAN_CASTLE_L | CAN_CASTLE_R, CAN_CASTLE_L | CAN_CASTLE_R };

    /* Attacks, checkers, pinners, blockers, etc */
    /// @brief Bitboards of checking squares per mobility type per color
    Bitboard checkingSquares[2][MOBILITY_TYPE_COUNT] = { { 0 } }; 

    /// @brief All checkers on the king per color
    Bitboard checkers[2] = { 0, 0 };

    /// @brief All pinning attackers in the position
    Bitboard pinners = 0;

    /// @brief All pinned pieces in the position per color
    Bitboard pinned = 0;
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
    VolatileBoardState lastState;
};

/// @brief Representation of the board
struct Board {
public:
    Board();

    /* Piece Representation */

    /// @brief All pieces on the board stored in a 1 dimensional array
    /// From bottom-left to top-right (A1 to G8)
    Piece pieceArray[64];

    /// @brief The piece position bitboards per piece type per color,
    /// equivalent to being index by the piece value
    Bitboard pieceBBs[1 << (4 + 2)];

    /// @brief All pieces for each color
    Bitboard allPiecesPerColor[2];

    /// @brief All pieces on the board
    Bitboard allPieces = 0;

    /* General State */

    /// @brief Whether it is white's turn to move
    bool turn;

    /// @brief The amount of moves made.
    int ply;

    /* King State */

    /// @brief The index the king is currently on per color
    Sq kingIndexPerColor[2] = { NULL_SQ, NULL_SQ };

    /// @brief The non trivial board state
    VolatileBoardState volatileState;

public:
    inline VolatileBoardState* volatile_state() const { return (VolatileBoardState*) &volatileState; }

    /* Piece access */
    inline Piece piece_on(Sq index) const { return pieceArray[index]; }
    inline Bitboard all_pieces() const { return allPieces; }
    inline Bitboard pieces_for_side(Color color) const { return allPiecesPerColor[color]; }
    inline Bitboard piece_bb(Piece p) const { return pieceBBs[p]; }
    inline Bitboard pieces(PieceType pt) const { return pieceBBs[pt | WHITE_PIECE] | pieceBBs[pt | BLACK_PIECE]; }
    inline Bitboard pieces(Color c, PieceType pt) const { return pieceBBs[pt | PIECE_COLOR_FOR(c)]; }
    template<typename... PieceTypes>
    inline Bitboard pieces(PieceType pt, PieceTypes... pts) const;
    template<typename... PieceTypes>
    inline Bitboard pieces(Color color, PieceType pt, PieceTypes... pts) const;
    inline Bitboard pieces_except_king(Color color) const { return pieces(color, PAWN, KNIGHT, BISHOP, ROOK, QUEEN); }

    /* Attacks */
    inline Bitboard calculate_attacks_on(Color color, Sq sq, /* out */ Bitboard *pinned = nullptr, /* out */ Bitboard *pinners = nullptr);
    inline Bitboard calculate_attacks_by(PieceType pt, Sq sq);

    /* Checking */
    inline Bitboard checkers(Color color) const { return volatile_state()->checkers[color]; }
    inline Bitboard pinners() const { return volatile_state()->pinners; }
    inline Bitboard pinned() const { return volatile_state()->pinned; }

    inline bool has_king(Color color) const { return kingIndexPerColor[color] != NULL_SQ; }
    inline Sq king_index(Color color) const { return kingIndexPerColor[color]; }

    /// @brief Set the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to set the piece at.
    /// @param p The piece type to remove.
    template <bool updateState>
    inline void set_piece(Sq index, Piece p);

    /// @brief Remove the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to remove the piece from.
    /// @param p The piece type to remove.
    template<bool updateState>
    inline void unset_piece(Sq index, Piece p);

    /// @brief Make the given move on the board.
    /// This does not check whether the move is legal.
    /// @param extMove The move to make with extended information.
    template<Color color /* the side to make the mode */, bool useExtMove>
    void make_move_unchecked(ExtMove<useExtMove>* extMove);

    /// @brief Unmake the given move on the board, restoring all state to before the move.
    /// This does not check whether the move is legal.
    /// @param move The move to unmake.
    template<Color color /* the side which made the mode */, bool useExtMove>
    void unmake_move_unchecked(ExtMove<useExtMove>* extMove);

    /// @brief Check if the king of the given color is in check.
    template<Color color>
    inline bool is_in_check() const { return checkers(color) > 0; }

    /// @brief Find the first rook on the given rank from either side.
    template<Color color, bool right>
    inline u8 find_file_of_first_rook_on_rank(u8 rank) const;

    /// @brief Get the checking attack bitboard for the given mobility type.
    template<Color color, MobilityType mt>
    inline Bitboard checking_attack_bb() const;

    /// @brief Check whether a move of a piece with the given MT to the given index gives a check on the king of the given color.
    template<Color color, MobilityType mt>
    inline bool is_check_attack(Sq index) const;

    /// @brief Creates a bitboard set for all squares controlled by the given color piece with the given 'trivial' type 
    /// at the given position, assuming it is able to move, without masking out friendly pieces.
    template<PieceType pieceType>
    Bitboard trivial_attack_bb(Sq index) const;
    
    /// @brief Recalculate all attacking, pinning, checking, etc bitboards.
    void recalculate_state();

    inline void clear_state_for_recalculation();

    template<Color color>
    inline void recalculate_state_sided();

    /// @brief Load the current board status from the given FEN string
    /// @param str The FEN string or 'startpos'.
    void load_fen(const char* str);

    /// @brief Whether the current position has insufficient material to deliver checkmate
    inline bool is_insufficient_material();
};

// Basically copied from Stockfish lol https://github.com/official-stockfish/Stockfish/blob/master/src/position.h
template<typename... PieceTypes>
inline Bitboard Board::pieces(PieceType pt, PieceTypes... pts) const {
    return pieces(pt) | pieces(pts...);
}

template<typename... PieceTypes>
inline Bitboard Board::pieces(Color color, PieceType pt, PieceTypes... pts) const {
    return pieces(color, pt) | pieces(color, pts...);
}

template <bool updateState>
inline void Board::set_piece(Sq index, Piece p) {
    Color color = IS_WHITE_PIECE(p);
    pieceArray[index] = p;
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
inline void Board::unset_piece(Sq index, Piece p) {
    pieceArray[index] = NULL_PIECE;
    pieceBBs[p] &= ~(1ULL << index);
    allPiecesPerColor[IS_WHITE_PIECE(p)] &= ~(1ULL << index);   
    allPieces &= ~(1ULL << index); 

    Color color = IS_WHITE_PIECE(p);

    if constexpr (updateState) {
        recalculate_state();
    }
}

template<Color color, bool useExtMove>
void Board::make_move_unchecked(ExtMove<useExtMove>* extMove) {
    Move move = extMove->move;

    Piece piece = extMove->piece = pieceArray[move.src];
    Sq captureSq = move.capture_index<color>();
    Piece captured = extMove->captured = pieceArray[captureSq]; 
    VolatileBoardState* state = volatile_state();

    if constexpr (useExtMove) {
        // store old state
        extMove->lastState = *state;
    }

    // 50 move rule
    state->rule50Ply++;
    if (TYPE_OF_PIECE(piece) == PAWN || captured != NULL_PIECE) {
        state->rule50Ply = 0;
    }

    // handle en passant and captures
    state->enPassantTarget = NULL_SQ;
    if (captured != NULL_PIECE) {
        unset_piece<false>(captureSq, captured);
    }

    if (move.is_double_push()) {
        // create en passant target
        u8 targetIndex = move.dst - SIDE_OF_COLOR(color) * 8;
        state->enPassantTarget = targetIndex;
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
        state->castlingStatus[color] |= CASTLED_L;
        u8 rookIndex = INDEX(/* todo: rook file */ 0, RANK(move.dst));
        Piece rook = color | ROOK;
        unset_piece<false>(rookIndex, rook);
        set_piece<false>(/* move behind king on the right */ move.dst + 1, rook);
    } else if (move.is_castle_right()) {
        state->castlingStatus[color] |= CASTLED_R;
        u8 rookIndex = INDEX(/* todo: rook file */ 0, RANK(move.dst));
        Piece rook = color | ROOK;
        unset_piece<false>(rookIndex, rook);
        set_piece<false>(/* move behind king on the left */ move.dst - 1, rook);
    } else if (TYPE_OF_PIECE(piece) == KING) {
        // remove castling rights on king move
        state->castlingStatus[color] &= ~(CAN_CASTLE_L | CAN_CASTLE_R);
    }

    // update state
    recalculate_state();

    // incr ply played
    ply++;
    turn = !turn;
}

template<Color color, bool useExtMove>
void Board::unmake_move_unchecked(ExtMove<useExtMove>* extMove) {
    Move move = extMove->move;
    Piece piece = extMove->piece;
    Piece captured = extMove->captured;

    VolatileBoardState* state = volatile_state();

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
        if constexpr (!useExtMove) {
            // restore en passant target
            state->enPassantTarget = move.dst;
        }
    } else if (captured != NULL_PIECE) {
        // return captured piece to dst
        set_piece<false>(move.dst, captured);
    }

    if constexpr (!useExtMove) {
        if (move.is_double_push()) {
            // destroy en passant target
            state->enPassantTarget = NULL_SQ;
        }
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
        this->volatileState = extMove->lastState;
    }

    // decr ply played
    ply--;
    turn = !turn;
}

template<Color color, bool right>
inline u8 Board::find_file_of_first_rook_on_rank(u8 rank) const {
    u8 bitline = ((pieceBBs[color * WHITE_PIECE | ROOK] >> rank * 8) & 0xFF);
    if constexpr (!right) {
        return __builtin_ctz(bitline);
    } else {
        return 7 - __builtin_clz(bitline << 24);
    }
}

template<Color color, MobilityType mt>
inline Bitboard Board::checking_attack_bb() const {
    if constexpr (mt == QUEEN_MOBILITY) {
        return checking_attack_bb<color, STRAIGHT>() | checking_attack_bb<color, DIAGONAL>();
    }

    return volatile_state()->checkingSquares[color][mt];
}

template<Color color, MobilityType mt>
inline bool Board::is_check_attack(Sq index) const {
    return ((checking_attack_bb<color, mt>() >> index) & 0x1) > 0;
}

template<>
inline Bitboard Board::trivial_attack_bb<KNIGHT>(Sq index) const {
    return lookup::knightAttackBBs.values[index];
}

template<>
inline Bitboard Board::trivial_attack_bb<ROOK>(Sq index) const {
    return lookup::magic::rook_attack_bb(index, allPieces);
}

template<>
inline Bitboard Board::trivial_attack_bb<BISHOP>(Sq index) const {
    return lookup::magic::bishop_attack_bb(index, allPieces);
}

template<>
inline Bitboard Board::trivial_attack_bb<QUEEN>(Sq index) const {
    return trivial_attack_bb<ROOK>(index) | trivial_attack_bb<BISHOP>(index);
}

/* 
    Volatile Board States
*/

inline void Board::clear_state_for_recalculation() {
    VolatileBoardState* state = volatile_state();
    state->pinned = 0;
    state->pinners = 0;
}

template<Color color>
inline void Board::recalculate_state_sided() {
    VolatileBoardState* state = this->volatile_state();

    const Bitboard allPieces = all_pieces();

    // king-related updates
    if (has_king(color)) {
        // calculate king checking squares
        u8 kingIndex = king_index(color);
        Bitboard pawnCBB = state->checkingSquares[color][PAWN] = lookup::pawnAttackBBs.values[color][kingIndex];
        Bitboard knightCBB = state->checkingSquares[color][KNIGHT] = lookup::knightAttackBBs.values[kingIndex];
        u64 rookKey = lookup::magic::rook_attack_key(kingIndex, allPieces);
        u64 bishopKey = lookup::magic::bishop_attack_key(kingIndex, allPieces);
        Bitboard rookCBB = state->checkingSquares[color][ROOK] = lookup::magic::get_rook_attack_bb(kingIndex, rookKey);
        Bitboard bishopCBB = state->checkingSquares[color][BISHOP] = lookup::magic::get_bishop_attack_bb(kingIndex, bishopKey);
        Bitboard queenCBB = state->checkingSquares[color][QUEEN] = rookCBB | bishopCBB;
        
        // calculate checkers
        Bitboard checkers = (pieces(!color, PAWN) & pawnCBB) | (pieces(!color, KNIGHT) & knightCBB) |
                            (pieces(!color, ROOK, QUEEN) & rookCBB | (pieces(!color, BISHOP, QUEEN) & bishopCBB));
        state->checkers[color] = checkers;

        // calculate xray attacks on the king
        std::ostringstream oss;
        std::cout << oss.str();

        Bitboard xrayRook = lookup::magic::rook_xray_bb(kingIndex, rookKey); 
        Bitboard xrayBishop = lookup::magic::bishop_xray_bb(kingIndex, bishopKey);

        // calculate pinners
        Bitboard pinnerRooks = pieces(!color, ROOK) & xrayRook;
        Bitboard pinnerBishops = pieces(!color, BISHOP) & xrayBishop;
        Bitboard pinnerQueens = pieces(!color, QUEEN) & (xrayRook | xrayBishop);
        state->pinners |= pinnerRooks | pinnerBishops | pinnerQueens;

        // calculate pinned pieces
        Bitboard pinned = pieces_except_king(color) & xrayRook;
    }
}

inline bool Board::is_insufficient_material() {
    return false;
}

/* Impl for the Move methods which take the board */
inline Piece Move::moved_piece(Board const* b) const { return b->pieceArray[src]; };
inline Piece Move::captured_piece(Board const* b) const { return b->pieceArray[dst]; }
inline bool Move::is_capture(Board const* b) const { return (b->allPieces & (1ULL << dst)) > 1; }
inline bool Move::is_check_estimated(Board const* b) const { return (b->volatile_state()->checkingSquares[!IS_WHITE_PIECE(moved_piece(b))][TYPE_OF_PIECE(moved_piece(b))] & (1ULL << dst)) > 1; }

}