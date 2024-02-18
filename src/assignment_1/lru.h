//
// Created by yanghoo on 2/13/24.
//
#ifndef FRAMEWORK_LRU_H
#define FRAMEWORK_LRU_H
#include "systemc"
#include <string>
#include <iomanip>
#include <iostream>
using namespace std;

static const size_t BLOCK_SIZE = 32; // 32 Bytes.
static const size_t SET_SIZE = 8; // 8-Set associative cache.

using namespace sc_core;

struct LRUnit {
    bool dirty;
    uint8_t index;
    uint64_t tag;
    uint8_t data[BLOCK_SIZE];
    LRUnit* next;
    LRUnit* prev;
};

class LRU {
    /*
     * LRU maintains an array to represent a d-linked list
     */
public:
    explicit LRU(uint8_t capacity, uint8_t lru_index);

    ~LRU() { delete this->lines; };

    void write(uint64_t tag, uint32_t _data);

    uint32_t read(uint64_t tag);

    LRUnit* head;
    LRUnit* tail;
    LRUnit* lines;
    uint8_t lru_index;

private:
    void push2head(LRUnit* curr) {
        cout << sc_core::sc_time_stamp() << " write cache back to the cache line" << endl;
        sc_core::wait(1); // write cache back to the cache line.
        if (this->head == nullptr && this->tail == nullptr) {
            // Initialization.
            this->head = curr;
            this->tail = curr;
            return;
        }

        if (this->head == curr) {
            // no need to change the first.
            return;
        }
        if (curr->prev != nullptr) {
            // disconnect this node.
            curr->prev->next = curr->next;
        }
        if (curr->next != nullptr) {
            // disconnect this node.
            curr->next->prev = curr->prev;
        }

        if (this->tail == curr) {
            // update if this is the tail.
            this->tail = curr->prev;
        }

        // push to the first place.
        curr->next = this->head;
        curr->prev = nullptr;
        if (curr->next != nullptr) {
            // if cold start, size = 1, don't need to change the head.
            curr->next->prev = curr;
        }
        this->head = curr;
    };

    LRUnit* find(uint64_t tag) const {
        LRUnit* curr = this->head;
        while (curr != nullptr) {
            if (curr->tag == tag) {
                return curr;
            } else {
                curr = curr->next;
            }
        }
        return curr;
    }

    bool is_empty() const {
        return this->size == 0;
    };

    bool is_full() const {
        return this->size == this->capacity;
    };

    uint8_t size;
    uint8_t capacity;
};

std::ostream& operator<<(std::ostream &out, LRU & data);

#endif //FRAMEWORK_LRU_H