#include "search.hh"

namespace tc {

void TranspositionTable::alloc(u32 powerOf2) {
    u64 cap = 1 << (powerOf2 + 1);
    this->capacity = cap;
    this->indexMask = (0xFFFFFFFF << (32 - powerOf2)) >> (32 - powerOf2);
    this->data = (TTEntry*)calloc(capacity, sizeof(TTEntry));
}

}