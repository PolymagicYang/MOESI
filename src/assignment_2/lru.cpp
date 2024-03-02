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

op_type LRU::read(uint64_t tag, uint32_t id) {
    LRUnit *curr = this->find(tag);
    op_type event;

    if (curr != nullptr) {
        // cache hits.
        event = op_type::read_hit;
        sc_core::wait(1);

        cout << sc_core::sc_time_stamp()
             << " [READ HIT] on " << to_string(curr->index) << "th cache line with tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        stats_readhit(id);
        this->push2head(curr);
    } else {
        // cache miss.
        event = op_type::read_miss;
        stats_readmiss(id);
        cout << sc_time_stamp() << " read data from memory" << endl;

        cout << sc_core::sc_time_stamp()
             << " [READ MISS] tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        if (this->is_full()) {
            // Cache line eviction.
            curr = this->tail;
            cout << sc_core::sc_time_stamp()
                 << " No enough space for new cache line, evict "
                 << to_string(curr->index) << "th cache line" << endl;

            curr->tag = tag;
            curr->status = cache_status::valid;
            cout << sc_core::sc_time_stamp()
                 << " update cache line with tag: 0x"
                 << setfill('0') << setw(13) << right << hex << curr->tag;

            this->push2head(curr);
        } else {
            curr = this->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return op_type::read_miss;
            }

            // append a new cache line.
            curr->tag = tag;
            curr->status = cache_status::valid;

            cout << sc_core::sc_time_stamp()
                 << " insert new cache line with "
                 << "no." << to_string(curr->index)
                 << " tag: 0x" << setfill('0') << setw(13) << right << hex
                 << curr->tag;

            this->push2head(curr);
            this->size += 1;
        }
        cout << sc_core::sc_time_stamp() << " write cache back to the cache line" << endl;
    }
    cout << sc_core::sc_time_stamp() << " cache line status after reading: " << endl << *this;
    return event;
}

op_type LRU::write(uint64_t tag, uint32_t, uint32_t id) {
    LRUnit *curr = this->find(tag);
    op_type event;

    if (curr != nullptr) {
        sc_core::wait();

        event = op_type::write_hit;
        cout << sc_core::sc_time_stamp()
             << " [WRITE HIT] on " << to_string(curr->index) << "th cache line with tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        // cache hits.
        stats_writehit(id);
        cout << sc_core::sc_time_stamp() << " mark " << to_string(curr->index) << "th cache line as dirty" << endl;
        curr->status = cache_status::valid;
        this->push2head(curr);
    } else {
        // cache miss.
        event = op_type::write_miss;
        stats_writemiss(id);

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

            curr->tag = tag;
            curr->status = cache_status::valid;

            cout << sc_core::sc_time_stamp()
                 << " update cache line with tag: 0x"
                 << setfill('0') << setw(13)
                 << right << hex << curr->tag;

            this->push2head(curr);
        } else {
            // store data.
            curr = this->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return op_type::write_miss;
            }
            curr->tag = tag;
            curr->status = cache_status::valid;

            cout << sc_core::sc_time_stamp()
                 << " insert new cache line with "
                 << "no." << to_string(curr->index)
                 << " tag: 0x" << setfill('0')
                 << setw(13) << right << hex << curr->tag;

            this->push2head(curr);
            this->size += 1;
        }
    }

    cout << sc_core::sc_time_stamp() << " cache line status after writing: " << endl << *this;
    return event;
}

void LRU::transition(cache_status target, uint64_t tag) {
    LRUnit *curr = this->find(tag);

    if (curr) {
        curr->status = target;
        if (target == cache_status::invalid) {
            this->invalid(curr);
        }
    }
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