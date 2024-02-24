#ifndef MEMORY_H
#define MEMORY_H

#include <iostream>
#include <systemc.h>

#include "Memory_if.h"
#include "bus_if.h"
#include "helpers.h"
#include "psa.h"

class Memory : public Memory_if, public sc_module {
    public:
    sc_port<bus_if> bus;
    sc_in<request*> Port_Mem;
    sc_in_clk clk;

    Memory(sc_module_name name_) : sc_module(name_) {
        SC_THREAD(execute);
        sensitive << clk.pos(); // It's positive because the bus is falling edge triggered.
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(Memory); // Needed because we didn't use SC_TOR

    ~Memory() override {
        // nothing to do here right now.
        delete this->requests;
    }

    int read(request req) override {
        this->requests->push_back(req);
        return 0;
    }

    int write(request req) override {
        this->requests->push_back(req);
        return 0;
    }

    void execute() {
        while (true) {
            // Pop the first request.
            for (auto req : *this->requests) {
                request response;
                response.cpu_id = req.cpu_id;
                response.addr = req.addr;
                response.source = location::memory;
                response.op = req.op;

                wait(100); // One memory access consumes 100 cycles.
                this->bus->try_request(response);
            }
            wait();
        }
    }

private:
    vector<request>* requests = new vector<request>;
};
#endif
