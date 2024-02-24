#include <systemc.h>
#include "Cache.h"
#include "psa.h"

/* cpu_cache_if interface method
 * Called by CPU.
 */
int Cache::cpu_read(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;

    // cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
    // cout << *lru;

    // detect the requests from last falling edge.
    request ret;
    this->probe(&ret);

    op_type operation = lru->read(tag);

    request req;
    req.source = location::cache;
    req.cpu_id = this->id;
    req.addr = addr;

    bool memory_access = false;
    switch (operation) {
        case read_hit:
            req.op = read_hit;
            memory_access = false;
            break;
        case read_miss:
            req.op = read_miss;
            memory_access = true;
            break;
        default:
            cout << "[ERROR]: detect write operations when read" << endl;
    }

    this->broadcast(req);
    if (memory_access) {
        this->wait_mem();
    }

    cout << sc_time_stamp() << " [READ END]" << endl;
}

int Cache::cpu_write(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;

    // cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
    // cout << *lru;

    // detect the requests from last falling edge.
    request ret;
    this->probe(&ret);
    op_type operation = lru->read(tag);

    request req;
    req.source = location::cache;
    req.cpu_id = this->id;
    req.addr = addr;

    switch (operation) {
        case write_hit:
            req.op = write_hit;
            break;
        case write_miss:
            req.op = write_miss;
            break;
        default:
            cout << "[ERROR]: detect read operations when write" << endl;
    }

    this->broadcast(req);
    this->wait_mem();
    cout << sc_time_stamp() << " [WRITE END]" << endl;
}
