#include <systemc.h>
#include "Cache.h"
#include "psa.h"

/* cache_if interface method
 * Called by Manager.
 */
int Cache::cpu_read(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    lru_read(addr, (uint32_t) this->id, lru);
    return 0;
}

int Cache::cpu_write(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    lru_write(addr, (uint32_t) this->id, lru);
    return 0;
}

int Cache::send_new_event() {
    this->has_new_event = true;
}

int Cache::send_data(request req) {
    this->data_ok = true;
    this->data = req;
}

bool Cache::get_cacheline_status(uint64_t addr, cache_status *curr_status) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    bool exists = lru->get_status(tag, curr_status);
    return exists;
}

void Cache::probe() {
    while (true) {
        wait(this->Port_Cache->value_changed_event());
        auto event = Port_Cache.read();
        if (!this->has_new_event) continue;
        this->has_new_event = false;

        uint64_t addr = event.addr;
        uint64_t set_i = (addr >> 5) % NR_SETS;
        uint64_t tag = (addr >> 5) / NR_SETS;

        Set *set = &this->sets[set_i];
        LRU *lru = set->lru;
        cache_status curr_status;
        bool exists = lru->get_status(tag, &curr_status);
        if (!exists) return;

        request message = event;
        switch (event.op) {
            case read_miss:
                // The bus will filter out the incorrect read miss.
                // Only owned and modified will provide the data.
                message.has_data = true;
                message.receiver_id = message.sender_id;
                message.sender_id = this->id;
                this->bus_port->try_request(message);
                break;
            case write_hit:
                lru->invalid(lru->find(tag));
                break;
            case read_hit:
                switch (curr_status) {
                    case exclusive:
                        lru->find(tag)->status = cache_status::shared;
                        break;
                    case modified:
                        lru->find(tag)->status = cache_status::owned;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
}

int Cache::ack() {
    this->ack_ok = true;
    return 0;
}

int Cache::finish_mem() {
    this->data_ok = true;
    return 0;
}

std::vector<request> Cache::get_requests() {
    vector<request> result = this->send_buffer;
    this->send_buffer.clear();
    return result;
}

void Cache::lru_read(uint64_t addr, uint32_t cpuid, LRU* lru) {
    uint64_t tag = (addr >> 5) / NR_SETS;
    uint64_t set_i = (addr >> 5) % NR_SETS;
    LRUnit *curr = lru->find(tag);

    if (curr != nullptr) {
        // cache hits.
        sc_core::wait();
        log_addr(this->name(), "[READ HIT]", addr);

        stats_readhit(cpuid);
        lru->push2head(curr);

        this->send_readhit(addr);
        wait_ack();
    } else {
        // cache miss.
        stats_readmiss(cpuid);
        log_addr(this->name(), "[READ MISS]", addr);

        this->send_readmiss(addr);
        wait_data();


        if (lru->is_full()) {
            // Cache line eviction.
            curr = lru->tail;
            if (curr->status == cache_status::modified || curr->status == cache_status::owned) {
                // update the memory data.
                this->send_mem(tag + (set_i << 5), op_type::read_miss);
            }

            switch (this->data.source) {
                case memory:
                    // Memory holds the recent data.
                    curr->status = cache_status::exclusive;
                    break;
                case cache:
                    // Other caches hold the recent copy of the data.
                    curr->status = cache_status::shared;
                    break;
            }
            curr->tag = tag;
            lru->push2head(curr);
        } else {
            curr = lru->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return;
            }

            // append a new cache line.
            curr->tag = tag;
            lru->push2head(curr);
            lru->size += 1;
        }
    }
}

void Cache::lru_write(uint64_t addr, uint32_t cpuid, LRU* lru) {
    uint64_t tag = (addr >> 5) / NR_SETS;
    LRUnit *curr = lru->find(tag);

    if (curr != nullptr) {
        sc_core::wait();

        log_addr(this->name(), "[WRITE HIT]", addr);
        this->send_writehit(addr);
        this->wait_ack();

        stats_writehit(cpuid);
        curr->status = cache_status::valid;
        lru->push2head(curr);
        log_addr(this->name(), "[TRANSITION] validate the cache line", addr);

        // The mem operating is in the last position, because the memory will execute the memory operations
        // in the service order, if this request comes firstly, then this cache will be invalid later, so we must
        // have good timing for setting the cache status.
        wait_data();
    } else {
        // cache miss.
        log_addr(this->name(), "[WRITE MISS]", addr);
        stats_writemiss(cpuid);
        this->send_writemiss(addr);
        this->wait_ack();

        if (lru->is_full()) {
            // needs to evict least recent used one.
            curr = lru->tail;

            curr->tag = tag;
            curr->status = cache_status::valid;
            log_addr(this->name(), "[TRANSITION] validate the cache line", addr);
            lru->push2head(curr);

            wait_data(); // write-allocate.
            wait_data(); // write through, the cache may be invalidated during the waiting, but it's ok.
        } else {
            // store data.
            curr = lru->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return;
            }
            curr->tag = tag;
            curr->status = cache_status::valid;
            log_addr(this->name(), "[TRANSITION] validate the cache line", addr);

            lru->push2head(curr);
            lru->size += 1;

            wait_data(); // write allocate.
            // during the write allocate waiting time, the memory location may already contain the older value,
            // it's ok to invalidate this cache before reading the data out.
            wait_data(); // write through.
        }
    }
}
