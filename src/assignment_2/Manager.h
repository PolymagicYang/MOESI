#ifndef CPU_H
#define CPU_H

#include <iostream>
#include <systemc.h>

#include "cpu_cache_if.h"
#include "helpers.h"
#include "psa.h"
#include "types.h"
#include "CPU.h"
#include "Manager_if.h"

class Manager : public Manager_if, public sc_module {
    public:
    sc_in_clk clock;
    sc_out<bool> start;

    Manager(sc_module_name name_) : sc_module(name_) {
        SC_THREAD(execute);
        sensitive << clock.pos();
        log(name(), "constructed with dispatcher");
        this->finished = 0;
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(Manager); // Needed because we didn't use SC_TOR

    int finish() override {
        this->finished += 1;
        return 0;
    }

    private:
    int finished;

    void execute() {
        while (true) {
            // Get the next action for the processor in the trace
            this->start->write(true);

            if ((uint32_t) this->finished == num_cpus) break;
            wait();
        }
        sc_stop();
    }
};

#endif
