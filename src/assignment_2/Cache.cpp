#include <systemc.h>
#include "Cache.h"
#include "psa.h"

/* cpu_cache_if interface method
 * Called by Manager.
 */
int Cache::cpu_read(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;

    // cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
    // cout << *lru;

    lru_read(addr, (uint32_t) this->id, lru);
    cout << sc_time_stamp() << " [READ END]" << endl << endl;
    return 0;
}

int Cache::cpu_write(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    lru_write(addr, (uint32_t) this->id, lru);
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

    cout << "start invalidating." << to_string(addr) << endl;

    if (lru->get_status(tag, &curr_status)) {
        // Ignore the probe if there is no cache line locally.
        cout << "into the loop." << endl;
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

void Cache::lru_read(uint64_t addr, uint32_t cpuid, LRU* lru) {
    uint64_t tag = (addr >> 5) / NR_SETS;
    LRUnit *curr = lru->find(tag);

    cout << "start reading." << endl;

    if (curr != nullptr) {
        // cache hits.
        sc_core::wait();

        cout << sc_core::sc_time_stamp()
             << " [READ HIT] on " << to_string(curr->index) << "th cache line with tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        stats_readhit(cpuid);
        this->send_readhit(addr);
        wait_ack();

        lru->push2head(curr);
    } else {
        // cache miss.
        stats_readmiss(cpuid);

        cout << sc_core::sc_time_stamp()
             << " [READ MISS] tag: 0x"
             << setfill('0') << setw(13) << right << hex << tag << endl;

        this->send_readmiss(addr);
        wait_ack();

        if (lru->is_full()) {
            // Cache line eviction.
            curr = lru->tail;
            cout << sc_core::sc_time_stamp()
                 << " No enough space for new cache line, evict "
                 << to_string(curr->index) << "th cache line" << endl;

            curr->tag = tag;
            curr->status = cache_status::valid;
            cout << sc_core::sc_time_stamp()
                 << " update cache line with tag: 0x"
                 << setfill('0') << setw(13) << right << hex << curr->tag;

            lru->push2head(curr);

            wait_mem();
        } else {
            curr = lru->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return;
            }

            // append a new cache line.
            curr->tag = tag;
            curr->status = cache_status::valid;

            cout << sc_core::sc_time_stamp()
                 << " insert new cache line with "
                 << "no." << to_string(curr->index)
                 << " tag: 0x" << setfill('0') << setw(13) << right << hex
                 << curr->tag;

            lru->push2head(curr);
            lru->size += 1;

            wait_mem();
        }
    }
}

void Cache::lru_write(uint64_t addr, uint32_t cpuid, LRU* lru) {
    uint64_t tag = (addr >> 5) / NR_SETS;
    LRUnit *curr = lru->find(tag);

    if (curr != nullptr) {
        sc_core::wait();
        cout << "[WRITE HIT]" << endl;

        this->send_writehit(addr);
        this->wait_ack();

        stats_writehit(cpuid);
        curr->status = cache_status::valid;
        lru->push2head(curr);

        // The mem operating is in the last position, because the memory will execute the memory operations
        // in the service order, if this request comes firstly, then this cache will be invalid later, so we must
        // have good timing for setting the cache status.
        wait_mem();
    } else {
        // cache miss.
        stats_writemiss(cpuid);
        this->send_writemiss(addr);
        this->wait_ack();

        cout << sc_core::sc_time_stamp()
             << " [WRITE MISS] tag: 0x"
             << setfill('0')
             << setw(13) << right << hex << tag << endl;

        if (lru->is_full()) {
            // needs to evict least recent used one.
            curr = lru->tail;

            curr->tag = tag;
            curr->status = cache_status::valid;
            lru->push2head(curr);

            wait_mem(); // write through, the cache may be invalidated during the waiting, but it's ok.
        } else {
            // store data.
            curr = lru->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return;
            }
            curr->tag = tag;
            curr->status = cache_status::valid;

            lru->push2head(curr);
            lru->size += 1;

            wait_mem(); // write allocate.
            // during the write allocate waiting time, the memory location may already contain the older value,
            // it's ok to invalidate this cache before reading the data out.
            wait_mem(); // write through.
        }
    }
}
