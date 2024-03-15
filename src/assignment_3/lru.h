//
// Created by yanghoo on 2/13/24.
//
#ifndef FRAMEWORK_LRU_H
#define FRAMEWORK_LRU_H

#include <systemc.h>
#include <string>
#include <iomanip>
#include <iostream>
#include "types.h"

using namespace std;

static const size_t BLOCK_SIZE = 32; // 32 Bytes.
static const size_t SET_SIZE = 8; // 8-Set associative cache.

using namespace sc_core;

struct LRUnit {
    uint8_t index;
    uint64_t tag;
    uint8_t data[BLOCK_SIZE];
    LRUnit *next;
    LRUnit *prev;
    cache_status status;
    bool has_data;
};

class LRU {
    /*
     * LRU maintains an array to represent a d-linked list
     */
public:
    explicit LRU(uint8_t capacity, uint8_t lru_index);

    ~LRU() { delete this->lines; };

    bool get_status(uint64_t, cache_status*) const;

    LRUnit *head;
    LRUnit *tail;
    LRUnit *lines;
    uint8_t lru_index;

    void push2head(LRUnit *curr) {
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

    void invalid(LRUnit* curr) {
        if (curr == nullptr) return;
        cout << "[invalid_size_start]: " << to_string(this->size);
        cout << " " << curr->tag;

        curr->status = cache_status::invalid;
        curr->has_data = false;
        this->size -= 1;

        if (this->head == this->tail && this->tail == curr && this->head == curr) {
            // len is 1.
            this->head = nullptr;
            this->tail = nullptr;

        } else if (this->head == curr && curr->next != nullptr) {
            // curr is the first one.
            this->head = curr->next;
            this->head->prev = nullptr;
        } else if (this->tail == curr && curr->prev != nullptr) {
            // curr is the last one.
            this->tail = curr->prev;
            this->tail->next = nullptr;
        } else {
            if (curr->prev != nullptr) {
                // disconnect this node.
                curr->prev->next = curr->next;
            }
            if (curr->next != nullptr) {
                // disconnect this node.
                curr->next->prev = curr->prev;
            }
        }

        cout << " [invalid_size_end]: " << to_string(this->size) << endl;
        // disconnect the adjacent nodes.
        curr->prev = nullptr;
        curr->next = nullptr;
    };

    LRUnit *find(uint64_t tag) const {
        LRUnit *curr = this->head;

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

    LRUnit* get_clean_node() const {
        for (uint8_t i = 0; i < this->capacity; i++) {
            if (this->lines[i].status == cache_status::invalid) {
                return &this->lines[i];
            }
        }
        return nullptr;
    };

    uint8_t size;
    uint8_t capacity;
};

std::ostream &operator<<(std::ostream &out, LRU &data);

#endif //FRAMEWORK_LRU_H
