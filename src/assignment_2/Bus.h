//
// Created by yanghoo on 2/22/24.
//
#ifndef FRAMEWORK_BUS_H
#define FRAMEWORK_BUS_H
#include "psa.h"
#include "types.h"
#include "bus_if.h"
#include "Memory_if.h"
#include <systemc.h>

class Bus : public bus_if, public sc_module {
public:
    sc_port<Memory_if> memory;
    sc_in_clk clock;
    sc_out<request> Port_Cache;

    int try_request(request) override;

    // Constructor without SC_ macro.
    Bus(sc_module_name name_) : sc_module(name_) {
        SC_THREAD(execute);
        sensitive << clock.neg();
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(Bus); // Needed because we didn't use SC_TOR

    ~Bus() {
    }

private:
    bus_requests requests;

    void execute() {
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

                if (req.destination == location::cache || req.destination == location::all) {
                    cout << "cache port" << endl;
                    this->Port_Cache.write(req);
                }
                if (req.destination == location::memory || req.destination == location::all) {
                    if (req.op == op_type::read_miss) {
                        this->memory->read(req);
                    }
                    if (req.op == op_type::write_miss || req.op == op_type::write_hit) {
                        this->memory->write(req);
                    }
                }
            }
            wait();
        }
    }
};

#endif //FRAMEWORK_BUS_H
