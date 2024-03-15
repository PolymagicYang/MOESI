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
    return 0;
}

int Cache::send_data(request req) {
    this->data_ok = true;
    this->data = req;

    return 0;
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
        if (!exists) continue;
        request message = event;
        auto curr = lru->find(tag);

        switch (event.op) {
            case data_transfer:
                // Other caches try to read this cache line will trigger this code
                // So the transition process is same as the probe_read, it doesn't have a break in the end.
                if (event.receiver_id == this->id) {
                    message.receiver_id = message.sender_id;
                    message.sender_id = this->id;
                    request_id rid;
                    rid.source = location::cache;
                    rid.cpu_id = this->id;

                    this->send_buffer.push_back(message);
                    this->bus_port->try_request(rid);
                }
            case probe_read:
                // probe read hit.
                switch (curr_status) {
                    case exclusive:
                        lru->find(tag)->status = cache_status::shared;
                        log(this->name(), "[TRANSITION] Exclusive -> Shared,");
                        break;
                    case modified:
                        lru->find(tag)->status = cache_status::owned;
                        log(this->name(), "[TRANSITION] Modified -> Owned,");
                        break;
                    default:
                        break;
                }
                break;

            case probe_write:
                // probe write hit.
                switch (curr->status) {
                    case invalid:
                        break;
                    case exclusive:
                        log(this->name(), "[TRANSITION] Exclusive -> Invalid,");
                        break;
                    case shared:
                        log(this->name(), "[TRANSITION] Shared -> Invalid,");
                        break;
                    case modified:
                        log(this->name(), "[TRANSITION] Modified -> Invalid,");
                        break;
                    case owned:
                        log(this->name(), "[TRANSITION] Owned -> Invalid,");
                        break;
                }
                lru->invalid(lru->find(tag));
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

std::vector<request> Cache::get_requests() {
    vector<request> result;
    for (const auto & i : this->send_buffer) {
        result.push_back(i);
    }
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
    } else {
        // cache miss.
        stats_readmiss(cpuid);
        log_addr(this->name(), "[READ MISS]", addr);

        if (lru->is_full()) {
            // Cache line eviction.
            curr = lru->tail;
            if (curr->status == cache_status::modified || curr->status == cache_status::owned) {
                // update the memory data.
                uint64_t cache_addr = (curr->tag << 12) + (set_i << 5);
                this->send_write_memory(cache_addr);
                // Wait until the data is written into the memory.
                this->wait_ack();
                this->wait_data();
            }

            log_addr(this->name(), "[TRANSITION] Invalidate data", addr);
            lru->invalid(curr);
            curr = lru->get_clean_node();
        } else {
            curr = lru->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return;
            }
        }


        while (curr->status == cache_status::invalid) {
            curr->tag = tag;
            curr->has_data = false;
            lru->push2head(curr);
            lru->size += 1;

            this->send_probe_read(addr);
            wait_ack();

            // Wait until the cache got the most recent copy of the data.
            switch (this->ack_from) {
                case memory:
                    // Memory holds the recent data.
                    curr->status = cache_status::exclusive;
                    log(this->name(), "[TRANSITION] Invalid -> Exclusive.");
                    break;
                case cache:
                    // Other caches hold the recent copy of the data.
                    log(this->name(), "[TRANSITION] Invalid -> Shared.");
                    curr->status = cache_status::shared;
                    break;
                default:
                    break;
            }
            // Start waiting.
            wait_data(); // The data in this cache may be invalidated by other caches later.
            curr->has_data = true;
        }
    }
}

void Cache::lru_write(uint64_t addr, uint32_t cpuid, LRU* lru) {
    uint64_t tag = (addr >> 5) / NR_SETS;
    uint64_t set_i = (addr >> 5) % NR_SETS;
    LRUnit *curr = lru->find(tag);

    if (curr != nullptr) {
        sc_core::wait();
        log_addr(this->name(), "[WRITE HIT]", addr);


        switch (curr->status) {
            case invalid:
                log(this->name(), "[ERROR] Write hit on invalid cache line.");
                break;
            case exclusive:
                // Exclusive doesn't need a write probe broadcast.
                log(this->name(), "[TRANSITION] Exclusive -> Modified.");
                break;
            case modified:
                break;
            case shared:
                log(this->name(), "[TRANSITION] Shared -> Modified");
            case owned:
                log(this->name(), "[TRANSITION] Owned -> Modified");
                this->send_probe_write(addr);
                this->wait_ack();
                break;
        }

        curr->status = modified; // After invalidating all the caches, we can mark it as modified.
        stats_writehit(cpuid);
        lru->push2head(curr);
    } else {
        // cache miss.
        log_addr(this->name(), "[WRITE MISS]", addr);

        stats_writemiss(cpuid);

        if (lru->is_full()) {
            // Cache line eviction.
            curr = lru->tail;
            if (curr->status == cache_status::modified || curr->status == cache_status::owned) {
                // update the memory data.
                uint64_t cache_addr = (curr->tag << 12) + (set_i << 5);
                this->send_write_memory(cache_addr);
                // Wait until the data is written into the memory.
                this->wait_ack();
                this->wait_data();

                // After it writes the data back to the memory, it can't provide data anymore.
                log(this->name(), "[TRANSITION] Write back, mark the cache line as invalid.");
                // We don't change the status immediately,
                // because other caches that are read miss needs to read the data
                // from this cache line.
            }
            lru->invalid(curr);
            curr = lru->get_clean_node();

        } else {
            curr = lru->get_clean_node();
            if (curr == nullptr) {
                cout << "[ERROR]: find nullptr when get clean node." << endl;
                return;
            }
        }

        while (curr->status == cache_status::invalid) {
            curr->tag = tag;
            curr->has_data = false;
            lru->push2head(curr);
            lru->size += 1;

            this->send_probe_read(addr);
            wait_ack();

            // Wait until the cache got the most recent copy of the data.
            switch (this->ack_from) {
                case memory:
                    // Memory holds the recent data.
                    log(this->name(), "[TRANSITION] Invalid -> Exclusive.");
                    curr->status = cache_status::exclusive;
                    break;
                case cache:
                    // Other caches hold the recent copy of the data.
                    log(this->name(), "[TRANSITION] Invalid to shared.");
                    curr->status = cache_status::shared;
                    break;
                default:
                    break;
            }
            wait_data(); // The data in this cache may be invalidated by other caches later.
            curr->has_data = true;
        }
        this->send_probe_write(addr);
        this->wait_ack();
    }

    curr->status = cache_status::modified;
}

void Cache::send_probe_read(uint64_t addr) {
    request req = req_template(addr, op_type::probe_read, location::all);
    this->send_buffer.push_back(req);

    request_id rid;
    rid.source = location::cache;
    rid.cpu_id = this->id;

    this->bus_port->try_request(rid);
}

void Cache::send_probe_write(uint64_t addr) {
    request req = req_template(addr, op_type::probe_write, location::all);
    this->send_buffer.push_back(req);

    request_id rid;
    rid.source = location::cache;
    rid.cpu_id = this->id;
    this->bus_port->try_request(rid);
}

void Cache::send_write_memory(uint64_t addr) {
    request req = req_template(addr, op_type::probe_write, location::memory);
    this->send_buffer.push_back(req);

    request_id rid;
    rid.source = location::cache;
    rid.cpu_id = this->id;
    this->bus_port->try_request(rid);
}

int Cache::put_ack_from(location l) {
    this->ack_from = l;
    return 0;
}

bool Cache::has_data(uint64_t addr) {
    uint64_t set_i = (addr >> 5) % NR_SETS;
    uint64_t tag = (addr >> 5) / NR_SETS;

    Set *set = &this->sets[set_i];
    LRU *lru = set->lru;
    auto curr = lru->find(tag);

    if (curr) {
        return lru->find(tag)->has_data;
    }
    return false;
}
