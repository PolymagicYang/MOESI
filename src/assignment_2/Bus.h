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

    int try_request(request) override;

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

    void execute() {
        bool burst_mode = false;
        while (true) {
            if (!this->requests.empty()) {
                // there are requests in the queue, fetching the requests.
                uint32_t request_i = 0;

                for (uint32_t i = 0; i < this->requests.size(); i++) {
                    request_i = i;
                    if (this->requests[request_i].source == location::memory) {
                        // memory requests should have the higher priority.
                        break;
                    }
                }

                auto req = this->requests[request_i];
                this->requests.erase(this->requests.begin() + request_i);

                if (req.destination == location::cache && req.source == location::memory) {
                    // From memory to cache.
                    this->caches[req.cpu_id]->finish_mem();
                }

                if (req.destination == location::cache && req.source == location::cache) {
                    cout << "broadcast" << endl;
                    // This is the cache broadcast, should ignore the sender when changing the states.
                    for (uint32_t i = 0; i < this->caches.size(); i++) {
                        if (i == req.cpu_id) {
                            this->caches[i]->ack();
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
            }
            wait();
        }
    }
};

#endif //FRAMEWORK_BUS_H
