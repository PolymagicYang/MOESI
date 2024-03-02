//
// Created by yanghoo on 2/22/24.
//
#ifndef FRAMEWORK_BUS_H
#define FRAMEWORK_BUS_H
#include "psa.h"
#include "types.h"
#include "bus_if.h"
#include "Memory_if.h"
#include "cpu_cache_if.h"
#include <systemc.h>

class Bus : public bus_if, public sc_module {
public:
    sc_port<Memory_if> memory;
    sc_in_clk clock;
    std::vector<sc_port<cpu_cache_if>> caches;

    int try_request(request_id) override;

    // Constructor without SC_ macro.
    Bus(sc_module_name name_) : sc_module(name_) {
        SC_THREAD(execute);
        this->caches = std::vector<sc_port<cpu_cache_if>>(num_cpus);
        sensitive << clock.neg();
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(Bus); // Needed because we didn't use SC_TOR

    ~Bus() {
    }

private:
    bus_requests requests;

    void send_request(request req) {
        if (req.destination == location::cache && req.source == location::memory) {
            // From memory to cache.
            this->caches[req.cpu_id]->finish_mem();
        }

        if (req.destination == location::cache && req.source == location::cache) {
            cout << "broadcast" << endl;
            // This is the cache broadcast, should ignore the sender when changing the states.
            for (uint32_t i = 0; i < this->caches.size(); i++) {
                if (i == req.cpu_id) {
                    continue;
                } else {
                    this->caches[i]->state_transition(req);
                }
            }
        }

        if (req.destination == location::memory) {
            // This should be equal to the filter in cache or memory.
            if (req.op == op_type::read_miss) {
                this->memory->read(req);
            }

            if (req.op == op_type::write_miss || req.op == op_type::write_hit) {
                cout << "serve write miss" << endl;
                this->memory->write(req);
            }
        }
    };

    void execute() {
        while (true) {
            if (this->requests.empty()) {
                wait();
            } else {
                // there are requests in the queue, fetching the requests.
                uint32_t request_i = 0;

                for (uint32_t i = 0; i < this->requests.size(); i++) {
                    request_i = i;
                    if (this->requests[request_i].source == location::memory) {
                        // memory requests should have the higher priority.
                        break;
                    }
                }

                auto req_id = this->requests[request_i];
                this->requests.erase(this->requests.begin() + request_i);
                std::vector<request> buffer;

                if (req_id.source == location::cache) {
                    int cpu_id = req_id.cpu_id;
                    // This is used to implement the burst request.
                    buffer = this->caches[cpu_id]->get_requests(); // get all requests in the buffer.
                    for (auto req : buffer) {
                        cout << sc_time_stamp() << endl;
                        this->send_request(req);
                        wait();
                    }
                    this->caches[cpu_id]->ack();

                } else if (req_id.source == location::memory) {
                    buffer = this->memory->get_requests();
                    for (auto req : buffer) {
                        this->send_request(req);
                        wait();
                    }
                    this->memory->ack();
                }
                wait(); // Wait the cache acks the requests, then the bus can process the next request.
            }
        }
    }
};

#endif //FRAMEWORK_BUS_H
