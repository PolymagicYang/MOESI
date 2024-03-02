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

    op_type operation = lru->read(tag, (uint32_t) this->id, this);

    switch (operation) {
        case read_hit:
            this->send_readhit(addr);
            wait_ack();
            break;
        case read_miss:
            this->send_readmiss(addr);
            wait_ack();
            wait_mem();
            break;
        default:
            cout << "[ERROR]: detect write operations when read" << endl;
    }

    cout << sc_time_stamp() << " [READ END]" << endl << endl;
    return 0;
}

int Cache::cpu_write(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    op_type operation = lru->write(tag, 0, (uint32_t) this->id, this);

    switch (operation) {
        case write_hit:
            this->send_writehit(addr);
            wait_ack();
            wait_mem();
            break;
        case write_miss:
            this->send_writemiss(addr);
            wait_ack();
            wait_mem();
            break;
        default:
            cout << "[ERROR]: detect read operations when write" << endl;
    }

    cout << sc_time_stamp() << " [WRITE END]" << endl << endl;
    return 0;
}

int Cache::state_transition(request req) {
    // Cache states transition logic.
    uint64_t addr = req.addr;
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    cache_status curr_status;

    if (lru->get_status(tag, &curr_status)) {
        // Ignore the probe if there is no cache line locally.
        if ((req.op == op_type::write_hit || req.op == op_type::write_miss) && curr_status == cache_status::valid) {
            // Probe write hit.
            cout << "before invalid: " << lru << endl;
            lru->transition(cache_status::invalid, tag);
            cout << "after invalid: " << lru << endl;
        }
        if (req.op == op_type::read_hit && curr_status == cache_status::valid) {
            lru->transition(cache_status::valid, tag);
        }
    }
    return 0;
}

int Cache::ack() {
    this->ack_ok = true;
    return 0;
}

int Cache::finish_mem() {
    this->mem_ok = true;
    return 0;
}

std::vector<request> Cache::get_requests() {
    vector<request> result = this->send_buffer;
    this->send_buffer.clear();
    return result;
}
