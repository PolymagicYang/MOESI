//
// Created by yanghoo on 2/25/24.
//

#ifndef FRAMEWORK_CPU_H
#define FRAMEWORK_CPU_H

#include <iostream>
#include <systemc.h>

#include "cpu_cache_if.h"
#include "helpers.h"
#include "psa.h"
#include "types.h"
#include "cpu_if.h"
#include "Manager_if.h"

class CPU: public sc_module {
public:
    sc_in_clk clock;
    sc_in<bool> start;
    sc_port<cpu_cache_if> cache;
    sc_port<Manager_if> manager;

    CPU(sc_module_name name_, int id_) : sc_module(name_), id(id_) {
        SC_THREAD(execute);
        sensitive << clock.pos();
        log(name(), "constructed with id", id);
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(CPU); // Needed because we didn't use SC_TOR

private:
    int id;

    void execute() {
        wait(this->start.value_changed_event());
        if (!this->start.read()) return;

        TraceFile::Entry tr_data;
        // Loop until end of tracefile
        while (!tracefile_ptr->eof()) {
            // Get the next action for the processor in the trace
            if (!tracefile_ptr->next(this->id, tr_data)) {
                cerr << "Error reading trace for Manager" << endl;
                break;
            }
            switch (tr_data.type) {
                case TraceFile::ENTRY_TYPE_READ:
                    log_addr(name(), "[READ] ", tr_data.addr);
                    this->cache->cpu_read(tr_data.addr);
                    log_addr(name(), "[READ END] ", tr_data.addr);
                    break;
                case TraceFile::ENTRY_TYPE_WRITE:
                    log_addr(name(), "[WRITE]", tr_data.addr);
                    this->cache->cpu_write(tr_data.addr);
                    log_addr(name(), "[WRITE END]", tr_data.addr);
                    break;
                case TraceFile::ENTRY_TYPE_NOP:
                    // log(name(), "nop");
                    break;
                default:
                    cerr << "Error, got invalid data from Trace" << endl;
                    exit(0);
            }
            wait();
            // Finished the Tracefile, now stop the simulation
        }
        manager->finish();
    }
};

#endif //FRAMEWORK_CPU_H
