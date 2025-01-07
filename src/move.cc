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
    this->data = (i32*) calloc(capacity, sizeof(i32));
}

void MoveEvalTable::free() {
    if (this->data != nullptr) {
        ::free(this->data);
        this->capacity = 0;
    }
}

}