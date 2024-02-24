#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <systemc.h>

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
    enum RetCode {
        PROBE_DONE
    };

    sc_port<bus_if> bus_port;
    sc_in<request> Port_Cache; // single producer multiple consumer (bus <-> many caches).
    sc_out<RetCode> output;

    // cpu_cache interface methods.
    int cpu_read(uint64_t addr) override;

    int cpu_write(uint64_t addr) override;

    // Constructor without SC_ macro.
    Cache(sc_module_name name_, int id_) : sc_module(name_), id(id_) {
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
    op_type event;
    Set *sets;

    /* cpu_cache_if interface method
     * Called by CPU.
     */
    op_type cpu_write(uint64_t addr) const {
        uint64_t set_i = (addr >> 5) % NR_SETS;
        uint64_t tag = (addr >> 5) / NR_SETS;

        Set *set = &this->sets[set_i];
        LRU *lru = set->lru;

        // cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
        // cout << *lru;
        return lru->write(tag, 0);
    }

    bool probe(request *result) const {
        // return true if there is new request comes in.
        bool req_exists = this->Port_Cache.event();
        if (req_exists) {
            auto req = this->Port_Cache.read();
            if (req.cpu_id != this->id) {
                // Probe the actions from the other caches.
                uint64_t addr = req.addr;
                uint64_t set_i = (addr >> 5) % NR_SETS;
                uint64_t tag = (addr >> 5) / NR_SETS;

                Set *set = &this->sets[set_i];
                LRU *lru = set->lru;
                cache_status curr_status;

                if (lru->get_status(tag, &curr_status)) {
                    // Ignore the probe if there is no cache line locally.
                    if (req.op == op_type::write_hit && curr_status == cache_status::valid) {
                        // Probe write hit.
                        lru->transition(cache_status::invalid, tag);
                    }
                    if (req.op == op_type::read_hit && curr_status == cache_status::valid) {
                        lru->transition(cache_status::valid, tag);
                    }
                }
            }
            *result = req;
            return true;
        }
        return false;
    }

    void broadcast(request req) {
        this->bus_port->try_request(req);
        while (true) {
            // wait until receive the same request.
            wait(this->Port_Cache.value_changed_event());

            request ret;
            this->probe(&ret);
            // req can't be the nullptr because of the wait fn.
            if (ret.cpu_id == this->id) {
                if (ret.source == memory) {
                    cout << endl << "[ERROR]: Receive unprocessed memory response" << endl;
                }
                break;
            }
        }
    }

    void wait_mem() {
        while (true) {
            wait(this->Port_Cache.value_changed_event());

            request ret;
            this->probe(&ret);
            // req can't be the nullptr because of the wait fn.
            if (ret.cpu_id == this->id) {
                if (ret.source == cache) {
                    cout << endl << "[ERROR]: Receive unprocessed cache response" << endl;
                }
                break;
            }
        }
    }
};

#endif
