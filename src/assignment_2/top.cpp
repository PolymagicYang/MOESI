/*
// File: top.cpp
//
*/

#include <iostream>
#include <systemc.h>

#include "CPU.h"
#include "Cache.h"
#include "psa.h"
#include "Bus.h"

using namespace std;

int sc_main(int argc, char *argv[]) {
    try {
        // Get the tracefile argument and create Tracefile object
        // This function sets tracefile_ptr and num_cpus
        init_tracefile(&argc, &argv);

        // init_tracefile changed argc and argv so we cannot use
        // getopt anymore.
        // The "-q" flag must be specified _after_ the tracefile.
        if (argc == 2 && !strcmp(argv[0], "-q")) {
            sc_report_handler::set_verbosity_level(SC_LOW);
        }

        sc_set_time_resolution(1, SC_PS);

        // Initialize statistics counters
        stats_init();

        // Create instances with id 0
        auto cpu_slots = new vector<CPU *>;
        auto cache_slots = new vector<Cache *>;

        // The clock that will drive the CPU
        sc_clock clk;

        auto memory = new Memory("memory");
        auto bus = new Bus("Bus");
        bus->clock(clk);

        sc_buffer<request> request_buffer;
        bus->Port_Cache(request_buffer);

        for (uint32_t i; i < num_cpus; i++) {
            auto cpu = new CPU(sc_gen_unique_name("cpu"), 0);
            auto cache = new Cache(sc_gen_unique_name("cache"), 0);

            cache->Port_Cache(request_buffer);
            cpu->cache(*cache);
            cpu->clock(clk);
            cache->bus_port(*bus);
            cpu_slots->push_back(cpu);
            cache_slots->push_back(cache);
        }

        // Start Simulation
        sc_start();

        // Print statistics after simulation finished
        stats_print();
        cout << sc_time_stamp() << endl;

        // Cleanup components
        for (auto cpu : *cpu_slots) {
            delete cpu;
        }
        for (auto cache : *cache_slots) {
            delete cache;
        }
        delete bus;
        delete memory;
    } catch (exception &e) {
        cerr << e.what() << endl;
    }

    return 0;
}
