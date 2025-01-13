#pragma once

#include "types.hh"
#include "evaldef.h"
#include <stdlib.h>

namespace tc {

/// @brief The player color part of the piece union
enum PieceColor {
    BLACK_PIECE = 0 << 4, 
    WHITE_PIECE = 1 << 4,
};

/// @brief The type of piece, 
enum PieceType {
    PAWN   = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK   = 3,
    QUEEN  = 4,
    KING   = 5,

    NULL_PIECE_TYPE = 6,
};

#define PIECE_TYPE_COUNT (NULL_PIECE_TYPE)

static const char* typeToName[] = {
    "Pawn",
    "Knight",
    "Bishop",
    "Rook",
    "Queen",
    "King"
};

static const char* typeAndColorToIcon[] = {
    "♙", "♘", "♗", "♖", "♕", "♔",
    "♟", "♞", "♝", "♜", "♛", "♚", 
};

static const char typeToCharLowercase[] = {
    'p',
    'n',
    'b',
    'r',
    'q',
    'k',
    '0',
};

inline static PieceType charToPieceType(char c) {
    char lc = tolower(c);
    switch (lc)
    {
    case 'p': return PAWN;
    case 'n': return KNIGHT;
    case 'b': return BISHOP;
    case 'r': return ROOK;
    case 'q': return QUEEN;
    case 'k': return KING;
    default: return NULL_PIECE_TYPE;
    }
}

#define NULL_PIECE ((Piece)(NULL_PIECE_TYPE | BLACK))
#define TYPE_MASK (0xF << 0)
#define COLOR_MASK (0xF << 4)

/// Pieces are simply encoded as a byte
typedef u8 Piece;

#define SIGN_OF_COLOR(c) ((int)(-1 + (c > 0) * 2))                // Returns -1 for black, 1 for white
#define SIGN_OF_PIECE(p) ((int)(-1 + ((p & COLOR_MASK) > 0) * 2)) // Returns -1 for black, 1 for white
#define COLOR_OF_PIECE(p) (PieceColor)(p & COLOR_MASK)
#define IS_WHITE_PIECE(p) (Color)((p & COLOR_MASK) > 0)
#define TYPE_OF_PIECE(p)  (PieceType)(p & TYPE_MASK)
#define PIECE_COLOR_FOR(b) (WHITE_PIECE * (b))                            // Piece color value for boolean color

inline char pieceToChar(Piece p) {
    char typeChar = typeToCharLowercase[TYPE_OF_PIECE(p)];
    return IS_WHITE_PIECE(p) ? typeChar - ('a' - 'A') : typeChar;
}

inline const char* pieceToUnicode(Piece p) {
    return typeAndColorToIcon[TYPE_OF_PIECE(p) * (1 + IS_WHITE_PIECE(p))];
}

// Material Values
constexpr i32 evalValuePawn   = iEval(1.0);
constexpr i32 evalValueKnight = iEval(3.0);
constexpr i32 evalValueBishop = iEval(3.0);
constexpr i32 evalValueRook   = iEval(5.0);
constexpr i32 evalValueQueen  = iEval(9.0);

static i16 materialValuePerType[] = {
    1, // Pawn
    3, // Knight
    3, // Bishop
    5, // Rook
    9, // Queen
    0, // King
    0, // NULL aka COUNT,
};

}