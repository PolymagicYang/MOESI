//
// Created by yanghoo on 2/22/24.
//
#ifndef FRAMEWORK_BUS_H
#define FRAMEWORK_BUS_H
#include "psa.h"
#include "types.h"
#include "bus_if.h"
#include "Memory_if.h"
#include "cache_if.h"
#include <systemc.h>
#include "helpers.h"

class Bus : public bus_if, public sc_module {
public:
    sc_port<Memory_if> memory;
    sc_in_clk clock;
    std::vector<sc_port<cache_if>> caches;
    sc_signal<request> CachePort;

    int try_request(request_id) override;

    location recent_data_location(uint64_t addr);

    void send_data_to_cpu(int cpu_id, request req);

    void send_to_mem(request req);

    void send_to_cpus(request req);

    void send_data_request_to_cpu(int cpu_id, request req);

    int find_most_recent_data_holder(uint64_t addr);

    request_id get_next_request_id();

    void send_request(request);

    // Constructor without SC_ macro.
    Bus(sc_module_name name_) : sc_module(name_) {
        SC_THREAD(execute);
        this->caches = std::vector<sc_port<cache_if>>(num_cpus);
        sensitive << clock.neg();
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(Bus); // Needed because we didn't use SC_TOR

    ~Bus() {
    }

    void execute() {
        while (true) {
            if (this->requests.empty()) {
                wait();
            } else {
                // there are requests in the queue, fetching the requests.
                auto req = this->get_next_request_id();
                vector<request> buffer;
                switch (req.source) {
                    case location::memory:
                        buffer = this->memory->get_requests();
                        break;
                    case location::cache:
                        buffer = this->caches[req.cpu_id]->get_requests();
                        break;
                }

                for (auto req_in_buffer : buffer) {
                    this->send_request(req_in_buffer);
                }
            }
        }
    }

private:
    bus_requests requests;
};

#endif //FRAMEWORK_BUS_H
