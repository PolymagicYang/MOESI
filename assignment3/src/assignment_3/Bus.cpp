//
// Created by yanghoo on 2/22/24.
//
#include "Bus.h"

int Bus::try_request(request_id req) {
    this->requests.push_back(req);
    return 0;
}

location Bus::recent_data_location(uint64_t addr) {
    for (uint32_t i = 0; i < this->caches.size(); i++) {
        cache_status status;
        if (this->caches[i]->get_cacheline_status(addr, &status)) {
            if (status != cache_status::invalid) {
                return location::cache;
            }
        }
    }
    return location::memory;
}

void Bus::send_request(request req) {
    auto data_location = this->recent_data_location(req.addr);
    int cpu_id = req.sender_id;
    if (data_location == location::cache) {
        cpu_id = find_most_recent_data_holder(req.addr);
    }
    if (cpu_id == req.sender_id) {
        data_location = location::memory;
    }

    switch (req.op) {
        case probe_read:
            this->caches[req.sender_id]->put_ack_from(data_location);

            switch (data_location) {
                case location::memory:
                    this->send_to_cpus(req);
                    this->send_to_mem(req);
                    break;
                default:
                    if (!this->caches[cpu_id]->has_data(req.addr)) {
                        this->send_to_cpus(req);
                        this->send_to_mem(req);
                    } else {
                        log(this->name(), "Cache", to_string(cpu_id), " has the most recent copy of the data","");
                        req.op = op_type::data_transfer;
                        req.receiver_id = cpu_id;
                        this->send_to_cpus(req);
                    }

                    break;
            }
            break;

        case probe_write:
            if (req.destination == location::all) {
                this->send_to_cpus(req); // Invalidate all the coherent cpus.
            }

            if (req.destination == location::memory) {
                this->send_to_mem(req);
            };

            break;

        case data_transfer:
            // read data from memory or cache.
            this->send_data_to_cpu(req.receiver_id, req);
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
    for (uint32_t i = 0; i < this->caches.size(); i++) {
        if (i == req.sender_id) {
            continue;
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
        case probe_read:
            this->memory->read(req);
            break;
        case probe_write:
            this->memory->write(req);
            break;
        default:
            break;
    }
}

void Bus::send_data_to_cpu(int cpu_id, request req) {
    // Only wake up the specific CPU.
    this->caches[cpu_id]->send_data(req);
}

int Bus::find_most_recent_data_holder(uint64_t addr) {
    // Any caches that are not invalid will hold the most recent data.
    for (uint32_t i = 0; i < this->caches.size(); i++) {
        cache_status status;
        if (this->caches[i]->get_cacheline_status(addr, &status)) {
            if (status != cache_status::invalid) {
                return (int) i;
            }
        }
    }
    return 0;
}

void Bus::send_data_request_to_cpu(int cpu_id, request req) {
    // Only wake up the receiver cpu.
    for (uint32_t i = 0; i < this->caches.size(); i++) {
        if ((int) i == cpu_id) {
            caches[i]->send_new_event();
        }
    }
    // wake up all the cpus.
    this->CachePort.write(req);
}
