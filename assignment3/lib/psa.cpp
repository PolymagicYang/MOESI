/*
// Source file for the Parallel System Architectures Lab Session helper
// functions.
// Contains the TraceFile class. This class represents a file where
// traces of memory requests from a program's execution are stored. The class
// can be used to read such files and drive a simulator. Traces can be read
// independently for each processor. After a trace has finished, NOP operations
// will be read.
// Other functions included in here are to keep track of and to print statistics
//
// -- STUDENTS SHOULD NOT NEED TO MODIFY THIS FILE --
//
// Author(s): Michiel W. van Tol, Mike Lankamp, Simon Polstra

// 64 bit address version
*/

#include "psa.h"
#include <arpa/inet.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <cstdint>

#if defined(OS_MACOSX)
#include <machine/endian.h>
#else
#include <endian.h>
#endif

using namespace std;

#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
uint64_t ntohll(uint64_t net) {
    uint64_t host = 0;
    for (int i = 0; i < 8; ++i) {
        host = (host << 8) | ((net >> i * 8) & 0xFF);
    }
    return host;
}
#else
uint64_t ntohll(uint64_t net) {
    return net;
}
#endif

// Internal structure to keep track of statistics per Manager
struct stats {
    int writehit;
    int writemiss;
    int readhit;
    int readmiss;
    int memory_access;
    vector<double> * buswaitcycle;
};

// Constant to put a 64 bit wire in high impedance mode.
const char *float_64_bit_wire = "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ";

static stats *stats_percpu = NULL;
TraceFile *tracefile_ptr = NULL;
uint32_t num_cpus = 0;

// Initializes the tracefile from the 1st argv argument then takes it out of
// argc/argv for argument parsing elsewhere
void init_tracefile(int *argc, char **argv[]) {
    // Check if we got at least one argument, otherwise throw an error
    if (*argc < 2) {
        throw runtime_error(string("Error, usage: ") + (*argv)[0] + string(" <tracefile>"));
    } else {
        // Open the tracefile and create TraceFile object
        tracefile_ptr = new TraceFile((*argv)[1]);

        // Get the number of Manager's from the tracefile
        num_cpus = tracefile_ptr->get_proc_count();

        // Reset arguments to the next set
        *argv = &((*argv)[2]);
        (*argc)--;
    }
}

// Allocates and sets up stats datastructure
void stats_init() {
    stats_percpu = (stats *)malloc(sizeof(stats) * num_cpus);
    if (stats_percpu == NULL) {
        throw runtime_error(
        string("Error, unable to allocate statistics memory"));
    }

    for (unsigned int i = 0; i < num_cpus; i++) {
        stats_percpu[i].writehit = 0;
        stats_percpu[i].writemiss = 0;
        stats_percpu[i].readhit = 0;
        stats_percpu[i].readmiss = 0;
        stats_percpu[i].memory_access = 0;

        stats_percpu[i].buswaitcycle = new vector<double>();
    }
}

void stats_cleanup() {
    free(stats_percpu);
}

void stats_print() {
    if (stats_percpu == NULL) {
        throw runtime_error(
        string("Error, unable to open statistics. Did you run stats_init()?"));
    }
    printf("Manager\tReads\tRHit\tRMiss\tWrites\tWHit\tWMiss\tHitrate\t\tMAccessTime\tWaitBus\n");

    double total_avg_wait = 0;
    for (unsigned int i = 0; i < num_cpus; i++) {
        int writes = stats_percpu[i].writehit + stats_percpu[i].writemiss;
        int reads = stats_percpu[i].readhit + stats_percpu[i].readmiss;

        // Ratio of hits to the number of total accesses
        double hitrate = (stats_percpu[i].writehit + stats_percpu[i].readhit) /
                         (double)(writes + reads);

        double total_cycles = 0;
        for (auto cycles : *stats_percpu[i].buswaitcycle) {
            total_cycles += cycles;
        }

        double avg_wait = total_cycles / (double) stats_percpu[i].buswaitcycle->size();
        // To make it a percentage
        total_avg_wait += avg_wait;

        hitrate = hitrate * 100;

        printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%f\t%d\t\t%f\n", i, reads,
               stats_percpu[i].readhit, stats_percpu[i].readmiss, writes,
               stats_percpu[i].writehit, stats_percpu[i].writemiss, hitrate, stats_percpu[i].memory_access,
               avg_wait);
    }
}

void stats_memory_access(uint32_t cpuid, int cycles) {
    if (cpuid < num_cpus && stats_percpu != NULL) {
        stats_percpu[cpuid].memory_access += cycles;
    }
}
void stats_waitbus(uint32_t cpuid, double cycles) {
    if (cpuid < num_cpus && stats_percpu != NULL) {
        stats_percpu[cpuid].buswaitcycle->push_back(cycles);
    }
}

void stats_writehit(uint32_t cpuid) {
    if (cpuid < num_cpus && stats_percpu != NULL) {
        stats_percpu[cpuid].writehit++;
    }
}

void stats_writemiss(uint32_t cpuid) {
    if (cpuid < num_cpus && stats_percpu != NULL) {
        stats_percpu[cpuid].writemiss++;
    }
}

void stats_readhit(uint32_t cpuid) {
    if (cpuid < num_cpus && stats_percpu != NULL) {
        stats_percpu[cpuid].readhit++;
    }
}

void stats_readmiss(uint32_t cpuid) {
    if (cpuid < num_cpus && stats_percpu != NULL) {
        stats_percpu[cpuid].readmiss++;
    }
}

TraceFile::TraceFile(const char *filename)
: m_input(filename, ios::in | ios::binary), m_num_finished(0) {
    // Check if the file properly opened
    if (!m_input.is_open() || !m_input.good()) {
        throw runtime_error(string("Unable to open file: ") + filename);
    }

    // Check file signature
    char signature[4];
    m_input.read((char *)&signature, 4);
    if (m_input.fail() || strncmp(signature, "4TRF", 4)) {
        throw runtime_error(string("Invalid file signature in file: ") + filename);
    }

    // Read number of processors the file was created for
    uint32_t procs_count;
    m_input.read((char *)&procs_count, sizeof(uint32_t));
    if (m_input.fail()) {
        throw runtime_error("Unable to read file");
    }

    // Transform result into host-order
    procs_count = ntohl(procs_count);

    // Set the start positions of the processor traces
    m_positions.resize(procs_count);
    streampos start = m_input.tellg();

    // And in the meanwhile store the end position of the file
    m_input.seekg(0, ios::end);
    m_endstream = m_input.tellg();

    if ((start + (streamoff)((procs_count * entry_size) + (entry_size - 1))) >= m_endstream) {
        throw runtime_error(string("Unexpected end of tracefile: ") + filename);
    }

    for (uint32_t i = 0; i < procs_count; i++) {
        m_positions[i] = start + (streamoff)(i * entry_size);
    }
}

TraceFile::~TraceFile() {}

void TraceFile::close() {
    m_input.close();
    m_positions.resize(0);
}

uint32_t TraceFile::get_proc_count() const {
    return m_positions.size();
}

bool TraceFile::next(uint32_t pid, Entry &e) {
    uint32_t cpucount = get_proc_count();

    if (pid >= cpucount) {
        // Invalid processor ID
        return false;
    }

    uint64_t data;
    assert(sizeof(data) == entry_size);

    // Test if there is a valid position in the trace registered for this Manager
    if (m_positions[pid] != (streampos)0) {
        if (m_positions[pid] > (m_endstream - (streampos)sizeof(data))) {
            // We didnt encounter an end tag but we can no longer read a whole
            // entry from the file, so we stop reading this trace from now on
            e.type = ENTRY_TYPE_NOP;
            m_positions[pid] = 0;
            m_num_finished++;
        } else {
            m_input.seekg(m_positions[pid]);
            m_input.read((char *)&data, sizeof(data));

            // Transform data into correct order
            data = ntohll(data);

            // Seek to next value
            m_positions[pid] += cpucount * sizeof(data);

            // Separate Address and Type-Tag information
            // Two most significant bits are used for the entry type
            e.addr = data & ~(0x3ULL << 62);
            e.type = (EntryType)(data >> 62);

            // Check if we encountered an end tag
            if (e.type == ENTRY_TYPE_END) {
                // We send a NOP instead
                e.type = ENTRY_TYPE_NOP;

                // And register that this cpu's trace has ended
                m_positions[pid] = 0;
                m_num_finished++;
            }
        }
    } else {
        // This trace already ended so we only send a NOP
        e.addr = 0;
        e.type = ENTRY_TYPE_NOP;
    }

    return true;
}

bool TraceFile::eof() const {
    return (m_num_finished == m_positions.size());
}
