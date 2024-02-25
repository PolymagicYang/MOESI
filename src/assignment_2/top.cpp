/*
// File: top.cpp
//
*/

#include <iostream>
#include <systemc.h>

#include "Manager.h"
#include "Cache.h"
#include "CPU.h"
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
        // The clock that will drive the Manager and bus.
        sc_clock clk;

        auto memory = new Memory("memory");
        auto bus = new Bus("Bus");
        auto dispatcher = new Manager(sc_gen_unique_name("manager"));

        bus->clock(clk);
        memory->clk(clk);
        dispatcher->clock(clk);

        memory->bus(*bus);
        bus->memory(*memory);

        sc_buffer<request> request_buffer;
        sc_signal<bool> start_signal;
        bus->Port_Cache(request_buffer);

        dispatcher->start(start_signal);
        /*
        * bus and cache should connects to the Manager.
        * list: Manager <-> Cache <-> bus <-> Memory
        * Every cache also has a signal port.
        */
        for (uint32_t i = 0; i < num_cpus; i++) {
            auto cache = new Cache(sc_gen_unique_name("cache"), (int) i);

            cache->bus_port(*bus);
            cache->Port_Cache(request_buffer);

            auto cpu = new CPU(sc_gen_unique_name("cpu"), (int) i);
            cpu->start(start_signal);
            cpu->clock(clk);
            cpu->manager(*dispatcher);
            cpu->cache(*cache);
        }

        // Start Simulation
        sc_start();

        // Print statistics after simulation finished
        stats_print();
        cout << sc_time_stamp() << endl;

        // Cleanup components
        delete bus;
        delete memory;
        delete dispatcher;
    } catch (exception &e) {
        cerr << e.what() << endl;
    }

    return 0;
}
