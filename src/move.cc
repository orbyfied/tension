#include "move.hh"

namespace tc {

MoveEvalTable::~MoveEvalTable() {
    free();
}

void MoveEvalTable::alloc(i32 capacity) {
    if (this->data != nullptr) {
        ::free(this->data);
    }

    this->capacity = capacity;
    this->data = (i16*) calloc(capacity, sizeof(i16));
}

void MoveEvalTable::free() {
    if (this->data != nullptr) {
        ::free(this->data);
        this->capacity = 0;
    }
}

}