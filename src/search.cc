#include "search.hh"

namespace tc {

void TranspositionTable::alloc(u32 entryCount) {
    this->capacity = entryCount;
    this->data = (TTEntry*)calloc(capacity, sizeof(TTEntry));
}

}