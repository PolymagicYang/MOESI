#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <systemc.h>
#include <stdexcept>

#include "Memory.h"
#include "cache_if.h"
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
// cache_if interface.
class Cache : public cache_if, public sc_module {
public:
    sc_port<bus_if> bus_port;
    sc_in<request> Port_Cache; // single producer multiple consumer (bus <-> many caches).
    sc_in_clk clk;

    // cpu_cache interface methods.
    int cpu_read(uint64_t addr) override;

    int cpu_write(uint64_t addr) override;

    int ack() override;

    int finish_mem() override;

    std::vector<request> get_requests() override;

    // Constructor without SC_ macro.
    Cache(sc_module_name name_, int id_) : sc_module(name_), id(id_) {
        this->has_new_event = false;
        this->data_ok = false;
        this->ack_ok = false;
        sensitive << clk.pos();
        SC_THREAD(probe);
        this->sets = new Set[NR_SETS];
        for (uint8_t i = 0; i < NR_SETS; i++) {
            this->sets[i].lru = new LRU(SET_SIZE, i);
        }
    }

    SC_HAS_PROCESS(Cache);

    void probe();

    ~Cache() override {
        delete this->sets;
    }

    int send_data(request req) override;

    int send_new_event() override;

    bool get_cacheline_status(uint64_t addr, cache_status* curr_status) override;

    void wait_ack() {
        auto start = sc_time_stamp().to_default_time_units();
        while (true) {
            wait();
            // This state can be invalid.
            if (this->ack_ok) {
                this->ack_ok = false;
                stats_waitbus(this->id, sc_time_stamp().to_default_time_units() - start);
                cout << "timestamp: " << sc_time_stamp().to_default_time_units() << " start: " << start << endl;
                return;
            }
        }
    }

    request req_template(uint64_t addr, op_type op, location dest) {
        request req;
        req.source = location::cache;
        req.sender_id = this->id;
        req.has_data = false;
        req.addr = addr;
        req.op = op;
        req.destination = dest;

        return req;
    }

    void send_readhit(uint64_t addr) {
        request req = req_template(addr, op_type::read_hit, location::cache);
        this->send_buffer.push_back(req);
    }

    void send_readmiss(uint64_t addr) {
        request req = req_template(addr, op_type::read_miss, location::memory);
        this->send_buffer.push_back(req);
    }

    void send_writehit(uint64_t addr) {
        request req = req_template(addr, op_type::write_hit, location::memory);
        this->send_buffer.push_back(req);
    }

    void send_writemiss(uint64_t addr) {
        request req = req_template(addr, op_type::write_miss, location::memory);
        this->send_buffer.push_back(req);
    }

    void wait_data() {
        while (true) {
            wait();
            if (this->data_ok) {
                this->data_ok = false;
                return;
            }
        }
    }

private:
    int id;
    Set *sets;
    vector<request> send_buffer;
    bool ack_ok;
    bool data_ok;
    bool has_new_event;
    request data;

    void lru_write(uint64_t addr, uint32_t cpuid, LRU *lru);

    void lru_read(uint64_t addr, uint32_t cpuid, LRU *lru);
};

#endif
