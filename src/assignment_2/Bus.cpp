//
// Created by yanghoo on 2/22/24.
//
#include "Bus.h"

int Bus::try_request(request_id req) {
    this->requests.push_back(req);
    return 0;
}

location Bus::recent_data_location(uint64_t addr) {
    for (auto cache : this->caches) {
        cache_status status;
        cache->get_cacheline_status(addr, &status);
        if (status == cache_status::owned || status == cache_status::modified) {
            return location::cache;
        } else {
            return location::memory;
        }
    }
}

void Bus::send_request(request req) {
    auto data_location = this->recent_data_location(req.addr);

    location from = req.source;
    location to = req.destination;
    bool from_cache_to_mem = (from == location::cache && to == location::memory);
    bool from_cache_to_cache = (from == location::cache && to == location::cache);

    if (req.op == op_type::read_miss && from_cache_to_mem) {
        // read_miss needs to read the recent copy of the data, the write miss is replaced the read miss + write hit.
        switch (data_location) {
            case location::memory:
                this->send_to_mem(req);
                break;
            case location::cache:
                int receiver = this->find_most_recent_data_holder(req.addr);
                req.receiver_id = receiver;
                req.destination = location::cache;
                this->send_data_request_to_cpu(receiver, req);
                break;
        }

    } else if (from_cache_to_mem) {
        // cpu accesses memory.
        this->send_to_mem(req);
    } else if (req.has_data) {
        // get data from cpu or memory to cpu.
        this->send_data_to_cpu(req.receiver_id, req);
    } else if (from_cache_to_cache) {
        // broadcast events.
        this->send_to_cpus(req);
    }
}

request_id Bus::get_next_request_id() {
    uint32_t request_i = 0;

    for (uint32_t i = 0; i < this->requests.size(); i++) {
        if (this->requests[request_i].source == location::memory) {
            request_i = i;
            break;
        }
    }
    auto req = this->requests[request_i];
    this->requests.erase(this->requests.begin() + request_i);

    return req;
}

void Bus::send_to_cpus(request req) {
    // mark all the cpus are allowed to wake up.
    for (int i = 0; i < this->caches.size(); i++) {
        if (i == req.sender_id) {
            this->caches[i]->ack();
        } else {
            caches[i]->send_new_event();
        }
    }
    // wake up all the cpus.
    this->CachePort.write(req);
}

void Bus::send_to_mem(request req) {
    // write miss is replaced by read miss + write hit.
    switch (req.op) {
        case read_miss:
            this->memory->read(req);
            break;
        case write_hit:
            this->memory->write(req);
            break;
        default:
            break;
    }
    this->caches[req.sender_id]->ack();
}

void Bus::send_data_to_cpu(int cpu_id, request req) {
    // Only wake up the specific CPU.
    this->caches[cpu_id]->send_data(req);
}

int Bus::find_most_recent_data_holder(uint64_t addr) {
    for (uint32_t i = 0; i < this->caches.size(); i++) {
        cache_status status;
        this->caches[i]->get_cacheline_status(addr, &status);

        if (status == cache_status::owned || status == cache_status::modified) {
            return i;
        }
    }
    return 0;
}

void Bus::send_data_request_to_cpu(int cpu_id, request req) {
    // Only wake up the receiver cpu.
    for (int i = 0; i < this->caches.size(); i++) {
        if (i == req.receiver_id) {
            caches[i]->send_new_event();
        }
    }
    // wake up all the cpus.
    this->CachePort.write(req);
}
