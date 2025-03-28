#include "bitboard.hh"

namespace tc {

Bitboard betweenBBsExcl[64][64];

static bool init_bitboards() {
    // for (int i = 0; i < 64; i++) {
    //     int ax = FILE(i); int ay = RANK(i);
    //     for (int j = 0; j < 64; j++) {
    //         if (i == j) continue;
    //         int bx = FILE(j); int by = RANK(j);
    //         int dx = bx - ax; int dy = by - ay;
    //         int xSign = (dx < 0) ? -1 : 1;
    //         float slope = (dy * 1.0 / dx);

    //         Bitboard bb = 0;
    //         float y = ay;
    //         for (int x = ax + xSign; abs(x - bx) > 1; x += xSign) {
    //             y += slope;
    //             int iy = (int)y;

    //             bb |= sqbb(INDEX(x, iy));
    //         }

    //         betweenBBsExcl[i][j] = bb;
    //     }
    // }

    log<DEBUG>(P, "Initialized precomputed auxiliary bitboards");
    return true;
}

bool _bitboardsInitialized = init_bitboards();

}