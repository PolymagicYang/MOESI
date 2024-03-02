#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <systemc.h>
#include <stdexcept>

#include "Memory.h"
#include "cpu_cache_if.h"
#include "helpers.h"
#include "types.h"
#include "lru.h"

#define S_CACHE_SIZE 32 << 10 // 32KBytes can be stored in the cache.

using namespace std;
using namespace sc_core; // This pollutes namespace, better: only import what you need.

static const size_t CACHE_SIZE = S_CACHE_SIZE;
static const size_t NR_SETS = CACHE_SIZE / (SET_SIZE * BLOCK_SIZE);

typedef struct Set {
    LRU *lru;
} Set;


// Class definition without the SC_ macro because we implement the
// cpu_cache_if interface.
class Cache : public cpu_cache_if, public sc_module {
public:
    sc_port<bus_if> bus_port;
    sc_in<request> Port_Cache; // single producer multiple consumer (bus <-> many caches).
    sc_in_clk clk;

    // cpu_cache interface methods.
    int cpu_read(uint64_t addr) override;

    int cpu_write(uint64_t addr) override;

    int state_transition(request req) override;

    int ack() override;

    int finish_mem() override;

    // Constructor without SC_ macro.
    Cache(sc_module_name name_, int id_) : sc_module(name_), id(id_) {
        sensitive << clk.pos();
        this->sets = new Set[NR_SETS];
        for (uint8_t i = 0; i < NR_SETS; i++) {
            this->sets[i].lru = new LRU(SET_SIZE, i);
        }
    }

    ~Cache() override {
        delete this->sets;
    }

private:
    int id;
    Set *sets;
    vector<request> pending_requests;
    bool ack_ok;
    bool mem_ok;

    void broadcast(uint64_t addr, op_type op) {
        request req;
        req.source = location::cache;
        req.destination = location::cache;
        req.cpu_id = this->id;
        req.addr = addr;
        req.op = op;
        this->bus_port->try_request(req);

        while (true) {
            wait();
            // This state can be invalid.
            if (this->ack_ok) {
                this->ack_ok = false;
                return;
            }
        }
    }

    void wait_mem() {
        while (true) {
            wait();
            if (this->mem_ok) {
                this->mem_ok = false;
                return;
            }
        }
    }

    void access_mem(uint64_t addr, op_type op) {
        request req;
        req.source = location::cache;
        req.destination = location::memory;
        req.cpu_id = this->id;
        req.addr = addr;
        req.op = op;
        this->bus_port->try_request(req);

        this->wait_mem();
    }
};

#endif
