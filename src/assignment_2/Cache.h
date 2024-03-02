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

    std::vector<request> get_requests() override;

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

    void wait_ack() {
        while (true) {
            wait();
            // This state can be invalid.
            if (this->ack_ok) {
                this->ack_ok = false;
                return;
            }
        }
    }

    void send_readhit(uint64_t addr) {
        request req;
        req.source = location::cache;
        req.destination = location::cache;
        req.cpu_id = this->id;
        req.addr = addr;
        req.op = op_type::read_hit;

        request_id rid;
        rid.cpu_id = this->id;
        rid.source = location::cache;

        this->bus_port->try_request(rid);
        this->send_buffer.push_back(req);
    }

    void send_readmiss(uint64_t addr) {
        request broadcast;
        broadcast.source = location::cache;
        broadcast.destination = location::cache;
        broadcast.cpu_id = this->id;
        broadcast.addr = addr;
        broadcast.op = op_type::read_miss;

        // read allocate.
        request memory_request;
        memory_request.source = location::cache;
        memory_request.destination = location::memory;
        memory_request.cpu_id = this->id;
        memory_request.addr = addr;
        memory_request.op = op_type::read_miss;

        request_id rid;
        rid.cpu_id = this->id;
        rid.source = location::cache;

        this->bus_port->try_request(rid);
        this->send_buffer.push_back(broadcast);
        this->send_buffer.push_back(memory_request);
    }

    void send_writehit(uint64_t addr) {
        request broadcast;
        broadcast.source = location::cache;
        broadcast.destination = location::cache;
        broadcast.cpu_id = this->id;
        broadcast.addr = addr;
        broadcast.op = op_type::write_hit;

        // write through.
        request memory_request;
        memory_request.source = location::cache;
        memory_request.destination = location::memory;
        memory_request.cpu_id = this->id;
        memory_request.addr = addr;
        memory_request.op = op_type::write_hit;

        request_id rid;
        rid.cpu_id = this->id;
        rid.source = location::cache;

        this->bus_port->try_request(rid);
        this->send_buffer.push_back(broadcast);
        this->send_buffer.push_back(memory_request);
    }

    void send_writemiss(uint64_t addr) {
        request broadcast;
        broadcast.source = location::cache;
        broadcast.destination = location::cache;
        broadcast.cpu_id = this->id;
        broadcast.addr = addr;
        broadcast.op = op_type::write_miss;

        // write allocate.
        request memory_read_request;
        memory_read_request.source = location::cache;
        memory_read_request.destination = location::memory;
        memory_read_request.cpu_id = this->id;
        memory_read_request.addr = addr;
        memory_read_request.op = op_type::write_miss;

        // write through.
        request memory_write_request;
        memory_write_request.source = location::cache;
        memory_write_request.destination = location::memory;
        memory_write_request.cpu_id = this->id;
        memory_write_request.addr = addr;
        memory_write_request.op = op_type::write_miss;

        request_id rid;
        rid.cpu_id = this->id;
        rid.source = location::cache;

        this->bus_port->try_request(rid);
        this->send_buffer.push_back(broadcast);
        this->send_buffer.push_back(memory_read_request);
        this->send_buffer.push_back(memory_write_request);
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

private:
    int id;
    Set *sets;
    vector<request> send_buffer;
    bool ack_ok;
    bool mem_ok;

    void lru_write(uint64_t addr, uint32_t cpuid, LRU *lru);

    void lru_read(uint64_t addr, uint32_t cpuid, LRU *lru);
};

#endif
