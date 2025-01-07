#pragma once

namespace tc {

#include "types.hh"
#include <stdlib.h>

/// @brief The boolean color of a player
enum Color {
    BLACK = 0,
    WHITE = 1
};

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

#define SIDE_OF_COLOR(c) ((int)(-1 + (c > 0) * 2))                // Returns -1 for black, 1 for white
#define SIDE_OF_PIECE(p) ((int)(-1 + ((p & COLOR_MASK) > 0) * 2)) // Returns -1 for black, 1 for white
#define COLOR_OF_PIECE(p) (Color)(p & COLOR_MASK)
#define IS_WHITE_PIECE(p) (bool)((p & COLOR_MASK) > 0)
#define TYPE_OF_PIECE(p)  (PieceType)(p & TYPE_MASK)
#define PIECE_COLOR_FOR(b) (WHITE * (b))                            // Piece color value for boolean color

inline char pieceToChar(Piece p) {
    char typeChar = typeToCharLowercase[TYPE_OF_PIECE(p)];
    return IS_WHITE_PIECE(p) ? typeChar - ('a' - 'A') : typeChar;
}

inline const char* pieceToUnicode(Piece p) {
    return typeAndColorToIcon[TYPE_OF_PIECE(p) * (1 + IS_WHITE_PIECE(p))];
}

// Material Values
constexpr i32 materialValuePawn   = iEval(1.0);
constexpr i32 materialValueKnight = iEval(3.0);
constexpr i32 materialValueBishop = iEval(3.0);
constexpr i32 materialValueRook   = iEval(5.0);
constexpr i32 materialValueQueen  = iEval(9.0);

static u16 materialValuePerType[] = {
    100, // Pawn
    300, // Knight
    300, // Bishop
    500, // Rook
    800, // Queen
    0,   // King
    0,   // COUNT
    0,   // NULL
};

}