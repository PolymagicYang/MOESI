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
        this->lines[i].dirty = false;
        this->lines[i].tag = 0;
    }

    this->lru_index = lru_index;
    this->size = 0;
    this->capacity = capacity;
    this->head = nullptr;
    this->tail = nullptr;
};

std::ostream &operator<<(std::ostream &out, LRU &data) {
    LRUnit *curr = data.head;
    out << "v==============================================v" << endl;
    out << "Set" << to_string(data.lru_index) << ": Cache lines status" << endl;
    out << "(Eviction priority increases from top to bottom)" << endl;

    // [ | no.0 | dirty: 0/1 | tag: 000...000 | ] [ | no.1 | ] ...
    while (curr != nullptr) {
        uint8_t index = curr->index;
        out << "[ | no." << to_string(index) << " | dirty: ";

        out << to_string(curr->dirty) << " | tag: ";
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

uint32_t LRU::read(uint64_t tag) {
    LRUnit *curr = this->find(tag);

    if (curr != nullptr) {
        // cache hits.
        sc_core::wait(1); // read the cache set out.

        cout << sc_core::sc_time_stamp()
             << " [READ HIT] on " << to_string(curr->index) << "th cache line with tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        stats_readhit(0);
        this->push2head(curr);
    } else {
        // cache miss.
        stats_readmiss(0);
        cout << sc_time_stamp() << " read data from memory" << endl;
        sc_core::wait(100); // read data from memory.

        cout << sc_core::sc_time_stamp()
             << " [READ MISS] tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        if (this->is_full()) {
            // Cache line eviction.
            curr = this->tail;
            cout << sc_core::sc_time_stamp()
                 << " No enough space for new cache line, evict "
                 << to_string(curr->index) << "th cache line" << endl;

            if (curr->dirty) {
                cout << sc_time_stamp() << " " << to_string(curr->index) << " th cache line is dirty, write back"
                     << endl;
                sc_core::wait(100);
            }

            curr->tag = tag;
            curr->dirty = false;
            cout << sc_core::sc_time_stamp()
                 << " update cache line with tag: 0x"
                 << setfill('0') << setw(13) << right << hex << curr->tag
                 << " dirty: " << to_string(curr->dirty) << endl;

            this->push2head(curr);
        } else {
            // append a new cache line.
            this->lines[this->size].tag = tag;
            this->lines[this->size].dirty = false;
            this->lines[this->size].index = this->size;
            curr = &this->lines[this->size];

            cout << sc_core::sc_time_stamp()
                 << " insert new cache line with "
                 << "no." << to_string(curr->index)
                 << " tag: 0x" << setfill('0') << setw(13) << right << hex
                 << curr->tag << " dirty: " << to_string(curr->dirty) << endl;

            this->push2head(curr);
            this->size += 1;
        }
        cout << sc_core::sc_time_stamp() << " write cache back to the cache line" << endl;
    }
    cout << sc_core::sc_time_stamp() << " cache line status after reading: " << endl << *this;
    return 0;
}

void LRU::write(uint64_t tag, uint32_t _data) {
    LRUnit *curr = this->find(tag);

    if (curr != nullptr) {
        sc_core::wait(1); // read the cache set out.

        cout << sc_core::sc_time_stamp()
             << " [WRITE HIT] on " << to_string(curr->index) << "th cache line with tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        // cache hits.
        stats_writehit(0);
        cout << sc_core::sc_time_stamp() << " mark " << to_string(curr->index) << "th cache line as dirty" << endl;
        curr->dirty = true;
        this->push2head(curr);
    } else {
        // cache miss.
        stats_writemiss(0);
        // allocate on write.
        sc_core::wait(100);

        cout << sc_core::sc_time_stamp()
             << " [WRITE MISS] tag: 0x"
             << setfill('0')
             << setw(13) << right << hex << tag << endl;

        if (this->is_full()) {
            // needs to evict least recent used one.
            curr = this->tail;

            cout << sc_core::sc_time_stamp()
                 << " No enough space for new cache line, evict "
                 << to_string(curr->index)
                 << "th cache line" << endl;

            if (curr->dirty) {
                // write back.
                cout << sc_time_stamp() << " "
                     << to_string(curr->index) << " th cache line is dirty, write back" << endl;
                sc_core::wait(100);
            }
            curr->tag = tag;
            curr->dirty = true;

            cout << sc_core::sc_time_stamp()
                 << " update cache line with tag: 0x"
                 << setfill('0') << setw(13)
                 << right << hex << curr->tag
                 << " dirty: " << to_string(curr->dirty) << endl;

            this->push2head(curr);
        } else {
            this->lines[this->size].tag = tag;
            // store data.
            this->lines[this->size].dirty = true;
            this->lines[this->size].index = this->size;

            curr = &this->lines[this->size];

            cout << sc_core::sc_time_stamp()
                 << " insert new cache line with "
                 << "no." << to_string(curr->index)
                 << " tag: 0x" << setfill('0')
                 << setw(13) << right << hex << curr->tag
                 << " dirty: " << to_string(curr->dirty) << endl;

            this->push2head(curr);
            this->size += 1;
        }
    }
    cout << sc_core::sc_time_stamp() << " cache line status after writing: " << endl << *this;
}