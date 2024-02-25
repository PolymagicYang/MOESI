/*
 * File: assignment1.cpp
 *
 * Framework to implement Task 1 of the Advances in Computer Architecture lab
 * session. This uses the framework library to interface with tracefiles which
 * will drive the read/write requests
 *
 * Author(s): Michiel W. van Tol, Mike Lankamp, Jony Zhang,
 *            Konstantinos Bousias, Simon Polstra
 *
 */

#include <string>
#include "psa.h"
#include "lru.h"

#define S_CACHE_SIZE 32 << 10 // 32KBytes can be stored in the cache.

using namespace std;
using namespace sc_core; // This pollutes namespace, better: only import what you need.

static const size_t CACHE_SIZE = S_CACHE_SIZE;
static const size_t NR_SETS = CACHE_SIZE / (SET_SIZE * BLOCK_SIZE);

typedef struct Set {
    LRU *lru;
} Set;

class CacheLines {
public:
    CacheLines() {
        this->sets = new Set[NR_SETS];
        for (uint8_t i = 0; i < NR_SETS; i++) {
            this->sets[i].lru = new LRU(SET_SIZE, i);
        }
    }

    uint8_t read(uint64_t addr) const {
        // Return true if there is a cache.
        // 32kB / (32 * 8) Byte = 128 sets.
        // address is 64 bits.
        // The cache line is 32 bytes lone, so we need 5 bits to represent it.
        cout << endl << sc_time_stamp()
             << " [READ]: Manager sends writes to: 0x"
             << setfill('0') << setw(16) << right << std::hex << addr << endl;

        uint64_t set_i = (addr >> 5) % NR_SETS;
        uint64_t tag = (addr >> 5) / NR_SETS;

        cout << "set index: "
             << to_string(set_i)
             << " tag: 0x" << setfill('0') << setw(13) << right << hex << tag << endl;

        Set *set = &this->sets[set_i];
        LRU *lru = set->lru;

        cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
        cout << *lru;

        lru->read(tag);
        cout << sc_time_stamp() << " [READ END]" << endl;
        return 0;
    }

    void store(uint64_t addr, uint32_t data) const {
        // default 4 bytes data.
        cout << endl << sc_time_stamp()
             << " [WRITE]: Manager sends writes to: 0x"
             << setfill('0') << setw(16) << right << std::hex << addr << endl;

        uint64_t set_i = (addr >> 5) % NR_SETS;
        uint64_t tag = (addr >> 5) / NR_SETS;

        cout << sc_time_stamp() << " Write data: " << to_string(data) << endl;
        // size_t offset = (addr & 0b11111);
        cout << "set index: " << to_string(set_i)
             << " tag: 0x" << setfill('0') << setw(13) << right << hex << tag << endl;

        // Get the set based on the addr.
        Set *set = &this->sets[set_i];
        LRU *lru = set->lru;

        cout << "Init state for " << to_string(set_i) << "th cache line:" << endl;
        cout << *lru;

        lru->write(tag, 0);
        cout << sc_time_stamp() << " [WRITE END]" << endl;
    }

    Set *sets;
};

SC_MODULE(Cache) {
public:
    enum Function {
        FUNC_READ, FUNC_WRITE
    };

    enum RetCode {
        RET_READ_DONE, RET_WRITE_DONE
    };

    sc_in<bool> Port_CLK;
    sc_in<Function> Port_Func;
    sc_in<uint64_t> Port_Addr;
    sc_out<RetCode> Port_Done;
    sc_inout_rv<64> Port_Data;

    SC_CTOR(Cache) {
        SC_THREAD(execute);
        sensitive << Port_CLK.pos();
        dont_initialize();

        this->cache = new CacheLines();
    }

    ~Cache() override {
        delete cache;
    }

private:
    CacheLines *cache;

    void execute() {
        while (true) {
            wait(Port_Func.value_changed_event());

            Function f = Port_Func.read();
            uint64_t addr = Port_Addr.read();
            uint64_t data = 0;
            if (f == FUNC_WRITE) {
                data = Port_Data.read().to_uint64();
            }

            if (f == FUNC_READ) {
                cache->read(addr);
                // get the whole cache line out, use the offset to get the 4 bytes.
                Port_Data.write(100);
                Port_Done.write(RET_READ_DONE);
                wait();
                Port_Data.write(float_64_bit_wire); // string with 64 "Z"'s
            } else {
                cache->store(addr, data);
                Port_Done.write(RET_WRITE_DONE);
            }
        }
    }
};

SC_MODULE(Manager) {
public:
    sc_in<bool> Port_CLK;
    sc_in<Cache::RetCode> Port_MemDone;
    sc_out<Cache::Function> Port_MemFunc;
    sc_out<uint64_t> Port_MemAddr;
    sc_inout_rv<64> Port_MemData;

    SC_CTOR(Manager) {
        SC_THREAD(execute);
        sensitive << Port_CLK.pos();
        dont_initialize();
    }

private:
    void execute() {
        TraceFile::Entry tr_data;
        Cache::Function f;
        // uint64_t addr = 0;
        // Loop until end of tracefile
        while (!tracefile_ptr->eof()) {
            // Get the next action for the processor in the trace
            if (!tracefile_ptr->next(0, tr_data)) {
                cerr << "Error reading trace for Manager" << endl;
                break;
            }

            switch (tr_data.type) {
                case TraceFile::ENTRY_TYPE_READ:
                    f = Cache::FUNC_READ;
                    break;

                case TraceFile::ENTRY_TYPE_WRITE:
                    f = Cache::FUNC_WRITE;
                    break;

                case TraceFile::ENTRY_TYPE_NOP:
                    break;

                default:
                    cerr << "Error, got invalid data from Trace" << endl;
                    exit(0);
            }

            if (tr_data.type != TraceFile::ENTRY_TYPE_NOP) {
                Port_MemAddr.write(tr_data.addr);
                Port_MemFunc.write(f);

                if (f == Cache::FUNC_WRITE) {
                    // We write the address as the data value.
                    Port_MemData.write(tr_data.addr);
                    wait();
                    Port_MemData.write(float_64_bit_wire); // 64 "Z"'s

                }

                wait(Port_MemDone.value_changed_event());

                if (f == Cache::FUNC_READ) {
                    cout << sc_time_stamp()
                         << ": Manager reads: " << Port_MemData.read() << endl << endl;
                }
            } else {
                cout << sc_time_stamp() << ": Manager executes NOP" << endl;
            }
            // Advance one cycle in simulated time
            wait();
        }

        // Finished the Tracefile, now stop the simulation
        sc_stop();
    }
};

int sc_main(int argc, char *argv[]) {
    try {
        // Get the tracefile argument and create Tracefile object
        // This function sets tracefile_ptr and num_cpus
        init_tracefile(&argc, &argv);

        // Initialize statistics counters
        stats_init();

        // Instantiate Modules
        Cache mem("main_memory");
        Manager cpu("cpu");

        // Signals
        sc_buffer<Cache::Function> sigMemFunc;
        sc_buffer<Cache::RetCode> sigMemDone;
        sc_signal<uint64_t> sigMemAddr;
        sc_signal_rv<64> sigMemData;

        // The clock that will drive the Manager and Cache
        sc_clock clk;

        // Connecting module ports with signals
        mem.Port_Func(sigMemFunc);
        mem.Port_Addr(sigMemAddr);
        mem.Port_Data(sigMemData);
        mem.Port_Done(sigMemDone);

        cpu.Port_MemFunc(sigMemFunc);
        cpu.Port_MemAddr(sigMemAddr);
        cpu.Port_MemData(sigMemData);
        cpu.Port_MemDone(sigMemDone);

        mem.Port_CLK(clk);
        cpu.Port_CLK(clk);

        cout << "Running (press CTRL+C to interrupt)... " << endl;

        // Start Simulation
        sc_start();

        // Print statistics after simulation finished
        stats_print();
        // mem.dump(); // Uncomment to dump memory to stdout.
    }

    catch (exception &e) {
        cerr << e.what() << endl;
    }

    return 0;
}
