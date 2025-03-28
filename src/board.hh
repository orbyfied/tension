#pragma once

#include <memory.h>
#include <iostream>
#include <array>
#include <string>
#include <iterator>
#include <sstream>

#include "types.hh"
#include "bitboard.hh"
#include "piece.hh"
#include "move.hh"
#include "lookup.hh"

namespace tc {

/* Hashing */

/// The type used for the hash or hash key of a position.
typedef u64 PositionHash;

template<int size>
using PositionHashArray = std::array<PositionHash, size>;

extern const PositionHashArray<1 << 12> pieceSqHashes;
extern const PositionHashArray<256> enPassantSqHashes;

extern const PositionHash hashCanCastleL;
extern const PositionHash hashCanCastleR;

extern const PositionHashArray<2> sideToMoveHashes;

// The hash key for a piece on the given square
#define PIECE_HASH_KEY(piece, sq) ((i16)(piece | (sq << 5)))

static const char* startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/// @brief Full attack info on a specific square
struct AttackInfo {
    Bitboard attackers;
    Bitboard pinners;
    Bitboard pinned;
};

/// @brief Non-trivial or unrecoverable state of the board which may be restored from memory.
struct VolatileBoardState {
    /// @brief The amount of moves made without a capture or pawn move
    u8 rule50Ply = 0;

    /// @brief The current en passant target
    u8 enPassantTarget = NULL_SQ;

    /// @brief Castling status per color
    u8 castlingStatus[2] = { CAN_CASTLE_L | CAN_CASTLE_R, CAN_CASTLE_L | CAN_CASTLE_R };
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
    bool turn = WHITE;

    /// @brief The amount of moves made.
    int ply = 0;

    /* King State */

    /// @brief The index the king is currently on per color
    Sq kingIndexPerColor[2] = { NULL_SQ, NULL_SQ };

    /* Attacks, checkers, pinners, blockers, etc */
    /// @brief Bitboards of checking squares per mobility type per color
    Bitboard checkingSquares[2][MOBILITY_TYPE_COUNT] = { { 0 } }; 

    /// @brief All checkers on the king per color
    Bitboard kingCheckers[2] = { 0, 0 };

    /// @brief The non trivial board state
    VolatileBoardState volatileState;

    /* Hashing */

    PositionHash pieceZHash;

public:
    forceinline VolatileBoardState* volatile_state() const { return (VolatileBoardState*) &volatileState; }

    /* Piece access */
    forceinline Piece piece_on(Sq index) const { return pieceArray[index]; }
    forceinline Bitboard all_pieces() const { return allPieces; }
    forceinline Bitboard pieces_for_side(Color color) const { return allPiecesPerColor[color]; }
    forceinline Bitboard piece_bb(Piece p) const { return pieceBBs[p]; }
    forceinline Bitboard pieces(PieceType pt) const { return pieceBBs[pt | WHITE_PIECE] | pieceBBs[pt | BLACK_PIECE]; }
    forceinline Bitboard pieces(Color c, PieceType pt) const { return pieceBBs[pt | PIECE_COLOR_FOR(c)]; }
    template<typename... PieceTypes>
    forceinline Bitboard pieces(PieceType pt, PieceTypes... pts) const;
    template<typename... PieceTypes>
    forceinline Bitboard pieces(Color color, PieceType pt, PieceTypes... pts) const;
    forceinline Bitboard pieces_except_king(Color color) const { return pieces(color, PAWN, KNIGHT, BISHOP, ROOK, QUEEN); }

    /* Attacks */
    inline Bitboard calculate_attacks_on(Color color, Sq sq, /* out */ Bitboard *pinned = nullptr, /* out */ Bitboard *pinners = nullptr) const;
    inline Bitboard calculate_attacks_by(PieceType pt, Sq sq) const;
    inline Bitboard attackers(Sq sq, Color attackingColor) const;

    inline Bitboard attacks_by(Color attackingColor, PieceType pt) const;
    template<typename... PieceTypes>
    forceinline Bitboard attacks_by(Color attackingColor, PieceType pt, PieceTypes... pts) const;
    forceinline Bitboard attacks_by(Color attackingColor) const { return attacks_by(attackingColor, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING); }

    forceinline Bitboard pawn_attacks_by(Color color) const;

    inline Bitboard attacks_on(Sq sq, Bitboard blockers, Color attackingColor, PieceType pt) const;
    template<typename... PieceTypes>
    forceinline Bitboard attacks_on(Sq sq, Bitboard blockers, Color attackingColor, PieceType pt, PieceTypes... pts) const;
    forceinline Bitboard attacks_on(Sq sq, Bitboard blockers, Color attackingColor) const { return attacks_on(sq, blockers, attackingColor, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING); }

    /* Checking */
    forceinline Bitboard checkers(Color color) const { return kingCheckers[color]; }

    forceinline bool has_king(Color color) const { return kingIndexPerColor[color] != NULL_SQ; }
    forceinline Sq king_index(Color color) const { return kingIndexPerColor[color]; }

    /// @brief Set the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to set the piece at.
    /// @param p The piece type to remove.
    template <bool updateState>
    forceinline void set_piece(Sq index, Piece p);
    template <bool updateState>
    forceinline void set_piece(Sq index, Piece p, Color color);

    /// @brief Remove the given piece from the board, inlined to allow optimization in move making
    /// @param index The index to remove the piece from.
    /// @param p The piece type to remove.
    template<bool updateState>
    forceinline void unset_piece(Sq index, Piece p);
    template<bool updateState>
    forceinline void unset_piece(Sq index, Piece p, Color color);

    /// @brief Make the given move on the board.
    /// This does not check whether the move is legal.  
    /// Warning: Usage without active ExtMove will not produce sufficient information
    /// to properly undo the move, state may be lost.
    /// @param extMove The move to make with extended information.
    template<Color color /* the side to make the mode */, bool useExtMove, bool updateAttackState = true>
    void make_move_unchecked(ExtMove<useExtMove>* extMove);

    /// @brief Unmake the given move on the board, restoring all state to before the move.
    /// This does not check whether the move is legal.
    /// Warning: When used without active ExtMove, the procedure may have insufficient
    /// information to fully restore the state.
    /// @param move The move to unmake.
    template<Color color /* the side which made the mode */, bool useExtMove>
    void unmake_move_unchecked(ExtMove<useExtMove>* extMove);

    /// @brief Check if the king of the given color is in check.
    template<Color color>
    forceinline bool is_in_check() const { return checkers(color) > 0; }

    /// @brief Find the first rook on the given rank from either side.
    template<Color color, bool right>
    forceinline u8 find_file_of_first_rook_on_rank(u8 rank) const;

    /// @brief Get the checking attack bitboard for the given mobility type.
    template<Color color, MobilityType mt>
    forceinline Bitboard checking_attack_bb() const;

    /// @brief Check whether a move of a piece with the given MT to the given index gives a check on the king of the given color.
    template<Color color, MobilityType mt>
    forceinline bool is_check_attack(Sq index) const;

    /// @brief Creates a bitboard set for all squares controlled by the given color piece with the given 'trivial' type 
    /// at the given position, assuming it is able to move, without masking out friendly pieces.
    template<PieceType pieceType>
    forceinline Bitboard trivial_attack_bb(Sq index) const;
    forceinline Bitboard trivial_attack_bb(Sq index, PieceType pt) const;
    
    /// @brief Recalculate all attacking, pinning, checking, etc bitboards.
    void recalculate_state();

    forceinline void clear_state_for_recalculation();

    template<Color color>
    forceinline void recalculate_state_sided();

    /// @brief Load the current board status from the given FEN string
    /// @param str The FEN string or 'startpos'.
    void load_fen(const char* str);

    /// @brief Load the current board status from the given FEN string
    /// @param it The character iterator.
    void load_fen(std::istream_iterator<char>& it, const std::istream_iterator<char>& end);

    /// @brief Whether the current position has insufficient material to deliver checkmate
    inline bool is_insufficient_material();

    /// @brief Perform some basic checks on the given move to ensure it isn't completely absurd 
    template<bool turn>
    inline bool check_pseudo_legal(Move move) const;
    
    /// @brief Check whether the given move may be legal
    template<bool turn>
    forceinline bool check_maybe_legal(Move move) const;

    /// @brief The main Zobrist hash of the board.
    forceinline u64 zhash();
};

// Basically copied from Stockfish lol https://github.com/official-stockfish/Stockfish/blob/master/src/position.h
template<typename... PieceTypes>
forceinline Bitboard Board::pieces(PieceType pt, PieceTypes... pts) const {
    return pieces(pt) | pieces(pts...);
}

template<typename... PieceTypes>
forceinline Bitboard Board::pieces(Color color, PieceType pt, PieceTypes... pts) const {
    return pieces(color, pt) | pieces(color, pts...);
}

template <bool updateState>
forceinline void Board::set_piece(Sq index, Piece p, Color color) {
    pieceArray[index] = p;
    pieceBBs[p] |= 1ULL << index;
    allPiecesPerColor[color] |= 1ULL << index;    
    allPieces |= 1ULL << index;
    pieceZHash ^= pieceSqHashes[PIECE_HASH_KEY(p, index)];

    // check for king update
    if (TYPE_OF_PIECE(p) == KING) {
        kingIndexPerColor[color] = index;
    }

    if constexpr (updateState) {
        recalculate_state();
    }
}

template <bool updateState>
forceinline void Board::set_piece(Sq index, Piece p) {
    set_piece<updateState>(index, p, IS_WHITE_PIECE(p));
}

template<bool updateState>
forceinline void Board::unset_piece(Sq index, Piece p, Color color) {
    pieceArray[index] = NULL_PIECE;
    pieceBBs[p] &= ~(1ULL << index);
    allPiecesPerColor[color] &= ~(1ULL << index);   
    allPieces &= ~(1ULL << index); 
    pieceZHash ^= pieceSqHashes[PIECE_HASH_KEY(p, index)];

    if constexpr (updateState) {
        recalculate_state();
    }
}

template<bool updateState>
forceinline void Board::unset_piece(Sq index, Piece p) {
    unset_piece<updateState>(index, p, IS_WHITE_PIECE(p));
}

// only updates the bitboards because the piece is replaced in the arrays
// or all pieces bb regardless
forceinline void remove_piece_replaced(Board* b, Sq index, Piece p, Color color) {
    b->pieceBBs[p] &= ~(1ULL << index);
    b->allPiecesPerColor[color] &= ~(1ULL << index); 
    b->pieceZHash ^= pieceSqHashes[PIECE_HASH_KEY(p, index)];  
}

template<Color color, bool useExtMove, bool updateAttackState>
forceinline void Board::make_move_unchecked(ExtMove<useExtMove>* extMove) {
    const Move move = extMove->move;

    // extract all necessary information
    Piece piece = extMove->piece = pieceArray[move.src];
    const Sq captureSq = move.capture_index<color>();
    const Piece captured = extMove->captured = pieceArray[captureSq]; 
    VolatileBoardState* state = volatile_state();

    constexpr Piece rook = PIECE_COLOR_FOR(color) | ROOK;
    const u8 rank = RANK(move.dst);

    if constexpr (useExtMove) {
        // store old state
        extMove->lastState = *state;
    }

    // 50 move rule and other board state
    state->rule50Ply++;
    if (TYPE_OF_PIECE(piece) == PAWN || captured != NULL_PIECE) {
        state->rule50Ply = 0;
    }

    state->enPassantTarget = NULL_SQ;

    // remove from source position
    unset_piece<false>(move.src, piece, color);

    // handle captures, en passant is handled by the capture sq
    if (captured != NULL_PIECE) {
        if (move.dst != captureSq) {
            remove_piece_replaced(this, captureSq, captured, !color);
        } else {
            unset_piece<false>(captureSq, captured, !color);
        }
        
        if (move.is_en_passant()) {
            goto finalize; // en passant cant be a double push, promotion, castle or king move
        }
    }

    // create en passant target for double push
    // else if because a capture is never a double push
    else if (move.is_double_push()) {
        // create en passant target
        u8 targetIndex = move.dst - SIGN_OF_COLOR(color) * 8;
        state->enPassantTarget = targetIndex;
        goto finalize; // a double push can not be a promotion, castle or king move
    }

    // check for promotions
    if (move.is_promotion()) {
        piece = PIECE_COLOR_FOR(color) | move.promotion_piece();
        goto finalize; // a promotion can not be a castle or king move
    }

    // castling
    if (move.is_castle_left()) {
        u8 rookFile = extMove->rookFile = find_file_of_first_rook_on_rank<color, false>(rank);
        u8 rookIndex = INDEX(rookFile, rank);
        unset_piece<false>(rookIndex, rook, color);
        set_piece<false>(/* move behind king on the right */ move.dst + 1, rook, color);
        state->castlingStatus[color] &= ~(CAN_CASTLE_L | CAN_CASTLE_R);
        goto finalize;
    } else if (move.is_castle_right()) {
        u8 rookFile = extMove->rookFile = find_file_of_first_rook_on_rank<color, true>(rank);
        u8 rookIndex = INDEX(rookFile, rank);
        unset_piece<false>(rookIndex, rook, color);
        set_piece<false>(/* move behind king on the left */ move.dst - 1, rook, color);
        state->castlingStatus[color] &= ~(CAN_CASTLE_L | CAN_CASTLE_R);
        goto finalize;
    }
    
    if (TYPE_OF_PIECE(piece) == KING) {
        // remove castling rights on king move
        state->castlingStatus[color] &= ~(CAN_CASTLE_L | CAN_CASTLE_R);
        goto finalize;
    }

    /* Label is called when a mutually exclusive special case for the move has been considered and we want
       to avoid checking the remaining cases. */
finalize:
    // set piece at destination
    set_piece<false>(move.dst, piece, color);

    if constexpr (updateAttackState) {
        // update state
        recalculate_state();
    }

    // incr ply played
    ply++;
    turn = !turn;
}

template<Color color, bool useExtMove>
forceinline void Board::unmake_move_unchecked(ExtMove<useExtMove>* extMove) {
    const Move move = extMove->move;

    // extract necessary information
    const Piece piece = extMove->piece;
    Piece dstPiece = piece;
    const Sq captureSq = move.capture_index<color>();
    const Piece captured = extMove->captured;
    VolatileBoardState* state = volatile_state();

    constexpr Piece rook = PIECE_COLOR_FOR(color) | ROOK;

    // unset the current destination piece
    if (move.is_promotion()) {
        // unset promoted piece, need to handle seperately bc
        // the bitboards for the promoted to type need to be updated before
        dstPiece = PIECE_COLOR_FOR(color) | move.promotion_piece();
    }

    // handle captures and en passant
    if (captured != NULL_PIECE) {
        if (captureSq == move.dst) {
            remove_piece_replaced(this, captureSq, dstPiece, color);
            set_piece<false>(captureSq, captured, !color);
        } else {
            unset_piece<false>(move.dst, dstPiece, color); 
            set_piece<false>(captureSq, captured, !color);
        }

        goto finalize; // captures can never be castle
    } else {
        unset_piece<false>(move.dst, dstPiece, color);
    }

    // undo castling
    if (move.is_castle_left()) {
        u8 rookIndex = INDEX(extMove->rookFile, RANK(move.dst));
        set_piece<false>(rookIndex, rook, color);
        unset_piece<false>(/* moved behind king on the right */ move.dst + 1, rook, color);
        goto finalize;
    } else if (move.is_castle_right()) {
        u8 rookIndex = INDEX(extMove->rookFile, RANK(move.dst));
        set_piece<false>(rookIndex, rook, color);
        unset_piece<false>(/* moved behind king on the left */ move.dst - 1, rook, color);
        goto finalize;
    }

    /* Label is called when a mutually exclusive special case for the move has been considered and we want
       to avoid checking the remaining cases. */
finalize:
    // return piece to source pos
    set_piece<false>(move.src, piece, color);

    if constexpr (useExtMove) {
        // restore state
        this->volatileState = extMove->lastState;
    }

    // decr ply played
    ply--;
    turn = !turn;
}

template<Color color, bool right>
forceinline u8 Board::find_file_of_first_rook_on_rank(u8 rank) const {
    u8 bitline = ((pieceBBs[color * WHITE_PIECE | ROOK] >> rank * 8) & 0xFF);
    if (!bitline) return NULL_SQ;

    if constexpr (!right) {
        return __builtin_ctz(bitline);
    } else {
        return 7 - __builtin_clz(bitline << 24);
    }
}

template<Color color, MobilityType mt>
forceinline Bitboard Board::checking_attack_bb() const {
    if constexpr (mt == QUEEN_MOBILITY) {
        return checking_attack_bb<color, STRAIGHT>() | checking_attack_bb<color, DIAGONAL>();
    }

    return checkingSquares[color][mt];
}

template<Color color, MobilityType mt>
forceinline bool Board::is_check_attack(Sq index) const {
    return (checking_attack_bb<color, mt>() & (1ULL << index)) > 0;
}

template<>
forceinline Bitboard Board::trivial_attack_bb<KNIGHT>(Sq index) const {
    return lookup::knightAttackBBs.values[index];
}

template<>
forceinline Bitboard Board::trivial_attack_bb<ROOK>(Sq index) const {
    return lookup::magic::rook_attack_bb(index, allPieces);
}

template<>
forceinline Bitboard Board::trivial_attack_bb<BISHOP>(Sq index) const {
    return lookup::magic::bishop_attack_bb(index, allPieces);
}

template<>
forceinline Bitboard Board::trivial_attack_bb<QUEEN>(Sq index) const {
    return trivial_attack_bb<ROOK>(index) | trivial_attack_bb<BISHOP>(index);
}

template<>
forceinline Bitboard Board::trivial_attack_bb<KING>(Sq index) const {
    return lookup::kingMovementBBs.values[index];
}

forceinline Bitboard Board::trivial_attack_bb(Sq index, PieceType pt) const {
    if (pt == KNIGHT) return trivial_attack_bb<KNIGHT>(index);
    else if (pt == BISHOP) return trivial_attack_bb<BISHOP>(index);
    else if (pt == ROOK) return trivial_attack_bb<ROOK>(index);
    else if (pt == QUEEN) return trivial_attack_bb<QUEEN>(index);
    else if (pt == KING) return trivial_attack_bb<KING>(index);
    return 0;
}

forceinline Bitboard Board::attackers(Sq index, Color attackingColor) const {
    return (lookup::pawnAttackBBs.values[!attackingColor][index] & pieces(attackingColor, PAWN)) | 
            (lookup::knightAttackBBs.values[index] & pieces(attackingColor, KNIGHT)) |
            (trivial_attack_bb<ROOK>(index) & pieces(attackingColor, ROOK, QUEEN)) |
            (trivial_attack_bb<BISHOP>(index) & pieces(attackingColor, BISHOP, QUEEN));
}

forceinline Bitboard Board::attacks_by(Color attackingColor, PieceType pt) const {
    if (pt == PAWN) {
        return pawn_attacks_by(attackingColor);
    }

    Bitboard all = 0;
    Bitboard bb = pieces(attackingColor, pt);
    while (bb) {
        all |= trivial_attack_bb(_pop_lsb(bb), pt);
    }

    return all;
}

template<typename... PieceTypes>
forceinline Bitboard Board::attacks_by(Color attackingColor, PieceType pt, PieceTypes... pts) const {
    return attacks_by(attackingColor, pt) | attacks_by(attackingColor, pts...);
}

forceinline Bitboard Board::pawn_attacks_by(Color color) const {
    const DirectionOffset UpOffset = (color ? OFF_NORTH : OFF_SOUTH);
    const Bitboard pawns = pieces(color, PAWN);
    Bitboard attacksEast = shift(pawns & BB_FILES_17_MASK, UpOffset + OFF_EAST);
    Bitboard attacksWest = shift(pawns & BB_FILES_28_MASK, UpOffset + OFF_WEST);
    return attacksEast | attacksWest;
}

forceinline Bitboard Board::attacks_on(Sq sq, Bitboard blockers, Color attackingColor, PieceType pt) const {
    if (pt == PAWN) return lookup::pawnAttackBBs.values[attackingColor][sq] & pieces(attackingColor, PAWN); 
    if (pt == KNIGHT) return lookup::knightAttackBBs.values[sq] & pieces(attackingColor, KNIGHT);
    if (pt == KING) return lookup::kingMovementBBs.values[sq] & pieces(attackingColor, KING);
    if (pt == BISHOP) return lookup::magic::bishop_attack_bb(sq, blockers) & pieces(attackingColor, BISHOP);
    if (pt == ROOK) return lookup::magic::rook_attack_bb(sq, blockers) & pieces(attackingColor, ROOK);
    if (pt == QUEEN) return (lookup::magic::rook_attack_bb(sq, blockers) | lookup::magic::bishop_attack_bb(sq, blockers)) & pieces(attackingColor, QUEEN);
    return 0;
}

template<typename... PieceTypes>
forceinline Bitboard Board::attacks_on(Sq sq, Bitboard blockers, Color attackingColor, PieceType pt, PieceTypes... pts) const {
    return attacks_on(sq, blockers, attackingColor, pt) | attacks_on(sq, blockers, attackingColor, pts...);
}

/* 
    Volatile Board States
*/

forceinline void Board::recalculate_state() {
    clear_state_for_recalculation();
    recalculate_state_sided<WHITE>();
    recalculate_state_sided<BLACK>();
}

forceinline void Board::clear_state_for_recalculation() {
    VolatileBoardState* state = volatile_state();
}

template<Color color>
forceinline void Board::recalculate_state_sided() {
    VolatileBoardState* state = this->volatile_state();

    const Bitboard allPieces = all_pieces();
    const Bitboard ourPieces = pieces_for_side(color);

    // king-related updates
    if (has_king(color)) {
        // calculate king checking squares
        const u8 kingIndex = king_index(color);
        const Bitboard pawnCBB = this->checkingSquares[color][PAWN] = lookup::pawnAttackBBs.values[!color][kingIndex];
        const Bitboard knightCBB = this->checkingSquares[color][KNIGHT] = lookup::knightAttackBBs.values[kingIndex];
        const u64 rookKey = lookup::magic::rook_attack_key(kingIndex, allPieces);
        const u64 bishopKey = lookup::magic::bishop_attack_key(kingIndex, allPieces);
        const Bitboard rookCBB = this->checkingSquares[color][ROOK] = lookup::magic::get_rook_attack_bb(kingIndex, rookKey);
        const Bitboard bishopCBB = this->checkingSquares[color][BISHOP] = lookup::magic::get_bishop_attack_bb(kingIndex, bishopKey);
        const Bitboard queenCBB = this->checkingSquares[color][QUEEN] = rookCBB | bishopCBB;
        
        // calculate checkers
        Bitboard checkers = (pieces(!color, PAWN) & pawnCBB) | (pieces(!color, KNIGHT) & knightCBB) |
                            (pieces(!color, ROOK, QUEEN) & rookCBB | (pieces(!color, BISHOP, QUEEN) & bishopCBB));
        this->kingCheckers[color] = checkers;
    }
}

forceinline bool Board::is_insufficient_material() {
    // king and king no pieces
    return _popcount64(allPieces) <= 2;
}

template<bool turn>
forceinline bool Board::check_pseudo_legal(Move move) const {
    Piece p = piece_on(move.src);
    if (p == NULL_PIECE || COLOR_OF_PIECE(p) != turn) return false;

    // check en passant validity
    if (move.is_en_passant()) {
        if (TYPE_OF_PIECE(p) != PAWN) return false;
        if (move.dst != volatile_state()->enPassantTarget) return false;
        return true;
    }

    // check castling validity
    if (move.is_castle_left()) return (TYPE_OF_PIECE(p) == KING) && (volatile_state()->castlingStatus[turn] & CAN_CASTLE_L) > 0;
    if (move.is_castle_right()) return (TYPE_OF_PIECE(p) == KING) && (volatile_state()->castlingStatus[turn] & CAN_CASTLE_R) > 0;

    return true;
}

template<bool turn>
forceinline bool Board::check_maybe_legal(Move move) const {
    Bitboard blockers = allPieces & ~(1ULL << move.src) | (1ULL << move.dst) & ~(move.special_capture() * (1ULL << move.capture_index<turn>()));
    Bitboard attacksOnKing = attacks_on(king_index(turn), blockers, !turn, /* only sliders can be pinning pieces */ BISHOP, ROOK, QUEEN);
    return attacksOnKing == 0;
}

forceinline u64 Board::zhash() {
    return pieceZHash ^ enPassantSqHashes[volatile_state()->enPassantTarget] ^ sideToMoveHashes[turn];
}

/* Impl for the Move methods which take the board */
forceinline Piece Move::moved_piece(Board const* b) const { return b->pieceArray[src]; };
forceinline Piece Move::captured_piece(Board const* b) const { return b->pieceArray[dst]; }
forceinline bool Move::is_capture(Board const* b) const { return (b->allPieces & (1ULL << dst)) > 1; }
forceinline bool Move::is_check_estimated(Board const* b) const { return (b->checkingSquares[!IS_WHITE_PIECE(moved_piece(b))][TYPE_OF_PIECE(moved_piece(b))] & (1ULL << dst)) > 1; }

}