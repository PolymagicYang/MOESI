//
// Created by yanghoo on 2/14/24.
//
#include "lru.h"
#include <iostream>
#include "psa.h"

using namespace std;

LRU::LRU(uint8_t capacity, uint8_t lru_index) {
    this->lines = new LRUnit[capacity];

    for (uint8_t i = 0; i < capacity; i++) {
        this->lines[i].next = nullptr;
        this->lines[i].prev = nullptr;
        this->lines[i].status = cache_status::invalid;
        this->lines[i].tag = 0;
        this->lines[i].index = i;
        this->lines[i].has_data = false;
    }

    this->lru_index = lru_index;
    this->size = 0;
    this->capacity = capacity;
    this->head = nullptr;
    this->tail = nullptr;
}

std::ostream &operator<<(std::ostream &out, LRU &data) {
    LRUnit *curr = data.head;
    out << "v==============================================v" << endl;
    out << "Set" << to_string(data.lru_index) << ": Cache lines status" << endl;
    out << "(Eviction priority increases from top to bottom)" << endl;

    // [ | no.0 | dirty: 0/1 | tag: 000...000 | ] [ | no.1 | ] ...
    while (curr != nullptr) {
        uint8_t index = curr->index;
        out << "[ | no." << to_string(index);

        out << "| valid: " << to_string(curr->status) << " | tag: ";
        out << "0x" << setfill('0') << setw(13) << right << hex
            << curr->tag; // I need 13 hex to represent (64 - (7 + 5)) bits.
        out << " | ] ";
        if (curr->next == nullptr) {
            out << endl << "Least recently used one: no." << to_string(curr->index);
        }
        curr = curr->next;
        out << endl;
    }
    out << "^==============================================^" << endl;
    return out;
}

bool LRU::get_status(uint64_t tag, cache_status* status) const {
    // return true if find the cache line else false.
    LRUnit *curr = this->find(tag);

    if (curr) {
        *status = curr->status;
        return true;
    } else {
        return false;
    }
}