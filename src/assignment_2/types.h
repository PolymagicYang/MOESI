//
// Created by yanghoo on 2/22/24.
//

#ifndef FRAMEWORK_TYPES_H
#define FRAMEWORK_TYPES_H
#include <systemc.h>

enum location {
    // source type
    memory = 0,
    cache = 1,
};

enum op_type {
    read_hit = 0,
    read_miss = 1,
    write_hit = 2,
    write_miss = 3,
};

typedef struct request {
    uint8_t sender_id; // cpu no.
    uint8_t receiver_id;
    enum location source;
    enum location destination;
    enum op_type op;
    bool has_data;
    uint64_t addr;

    request& operator=(const request& rhs) {
        cpu_id = rhs.cpu_id;
        source = rhs.source;
        destination = rhs.destination;
        op = rhs.op;
        addr = rhs.addr;
        has_data = rhs.has_data;
        return *this;
    }

    bool operator==(const request& rhs) const {
        return cpu_id == rhs.cpu_id && source == rhs.source && op == rhs.op && addr == rhs.addr && rhs.destination == destination && has_data == rhs.has_data;
    }
} request;

std::ostream& operator<<(std::ostream& os, const request& val);

inline void sc_trace(sc_trace_file*& f, const request& val, const std::string& name) {
    sc_trace(f, val.cpu_id, name + ".cpu");
    sc_trace(f, val.addr, name + ".addr");
    sc_trace(f, val.op, name + ".op");
    sc_trace(f, val.source, name + ".source");
    sc_trace(f, val.destination, name + ".destination");
}

typedef struct request_id {
    uint8_t cpu_id; // cpu no.
    enum location source;

    request_id& operator=(const request_id& rhs) {
        cpu_id = rhs.cpu_id;
        source = rhs.source;
        return *this;
    }

    bool operator==(const request_id& rhs) const {
        return cpu_id == rhs.cpu_id && source == rhs.source;
    }
} request_id;

std::ostream& operator<<(std::ostream& os, const request_id& val);

inline void sc_trace(sc_trace_file*& f, const request_id& val, const std::string& name) {
    sc_trace(f, val.cpu_id, name + ".cpu");
    sc_trace(f, val.source, name + ".source");
}

enum cache_status {
    invalid = 0,
    exclusive = 1,
    shared = 2,
    modified = 3,
    owned = 4
};

typedef std::vector<request_id> bus_requests;

#endif //FRAMEWORK_TYPES_H
