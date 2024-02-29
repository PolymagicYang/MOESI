#include <systemc.h>
#include "Cache.h"
#include "psa.h"

/* cpu_cache_if interface method
 * Called by Manager.
 */
int Cache::cpu_read(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;

    // cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
    // cout << *lru;

    // request ret = {}; It never be called, becuase the bus only send data on the fallen edge.
    // this->probe(&ret);

    op_type operation = lru->read(tag, (uint32_t) this->id);

    request req;
    req.destination = location::all;
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

            cout << "read miss" << endl;
            this->broadcast(req);
            this->wait_mem();
            break;
        default:
            cout << "[ERROR]: detect write operations when read" << endl;
    }

    this->broadcast(req);
    if (memory_access) {
        req.destination = location::memory; // avoid broadcast the read miss twice.
        this->wait_mem();
    }

    cout << sc_time_stamp() << " [READ END]" << endl << endl;
    return 0;
}

int Cache::cpu_write(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;

    // cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
    // cout << *lru;

    // detect the requests from last falling edge.
    // request ret = {}; It never be called, becuase the bus only send data on the fallen edge.
    // this->probe(&ret);
    op_type operation = lru->write(tag, 0, (uint32_t) this->id);

    request req;
    req.source = location::cache;
    req.destination = location::all;
    req.cpu_id = this->id;
    req.addr = addr;

    switch (operation) {
        case write_hit:
            req.op = write_hit;
            break;
        case write_miss:
            req.op = write_miss;

            cout << "broadcast" << endl;
            this->broadcast(req);
            cout << "broadcast end" << endl;

            this->wait_mem();
            break;
        default:
            cout << "[ERROR]: detect read operations when write" << endl;
    }

    req.destination = location::memory; // avoid broadcast write op twice.
    this->broadcast(req);

    cout << "finish broadcast" << endl;
    this->wait_mem();

    cout << sc_time_stamp() << " [WRITE END]" << endl << endl;
    return 0;
}
