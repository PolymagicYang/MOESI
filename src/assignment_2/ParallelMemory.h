#ifndef PARALELL_MEMORY_H
#define PARALELL_MEMORY_H

#include <iostream>
#include <systemc.h>

#include "Memory_if.h"
#include "bus_if.h"
#include "helpers.h"
#include "psa.h"

enum channel_state {
    busy = 0,
    ready = 1,
    idle = 2
};

class ParallelMemory : public Memory_if, public sc_module {
    public:
    sc_port<bus_if> bus;
    sc_in_clk clk;

    ParallelMemory(sc_module_name name_, int parallel_num_) : sc_module(name_), parallel_num(parallel_num_) {
        this->channels = new request[this->parallel_num];
        this->channel_states = new channel_state[this->parallel_num];
        for (int i = 0; i < parallel_num; i++) {
            this->channel_states[i] = channel_state::idle;
            SC_THREAD(execute);
        }
        SC_THREAD(dispatch);
        sensitive << clk.pos(); // It's positive because the bus is falling edge triggered.
        dont_initialize(); // don't call execute to initialise it.
    }

    SC_HAS_PROCESS(ParallelMemory); // Needed because we didn't use SC_TOR

    ~ParallelMemory() override {
        // nothing to do here right now.
        delete this->channels;
    }

    int read(request req) override {
        this->requests.push_back(req);
        return 0;
    }

    int write(request req) override {
        this->requests.push_back(req);
        return 0;
    }

    void ack() override {
        this->ack_ok = true;
    }

    vector<request> get_requests() override {
        // copy the result.
        vector<request> result = this->send_buffer;
        this->send_buffer.clear();
        return result;
    }

    void dispatch() {
        while (true) {
            // Pop the first request.
            if (!this->requests.empty()) {
                auto req = this->requests.front();

                for (int i = 0; i < this->parallel_num; i++) {
                    if (this->channel_states[i] == channel_state::idle) {
                        this->channels[i] = req;
                        this->channel_states[i] = channel_state::busy;
                        this->requests.erase(this->requests.begin());
                        break;
                    }
                }

                for (int i = 0; i < this->parallel_num; i++) {
                    if (this->channel_states[i] == channel_state::ready) {
                        auto response = this->channels[i];
                        request_id response_id;
                        response_id.cpu_id = response.cpu_id;
                        response_id.source = location::memory;

                        this->bus->try_request(response_id);
                        this->send_buffer.push_back(response);

                        this->channel_states[i] = channel_state::idle;
                        this->wait_ack();
                        break;
                    }
                }
            }
            wait();
        }
    }

    void execute() {
        int process_id = this->id;
        this->id += 1;
        while (true) {
            // Pop the first request.
            if (this->channel_states[process_id] == channel_state::busy) {
                auto req = this->channels[process_id];

                if (req.source != location::memory) {
                    request response;
                    response.cpu_id = req.cpu_id;
                    response.addr = req.addr;
                    response.source = location::memory;
                    response.destination = location::cache;
                    response.op = req.op;

                    wait(100); // One memory access consumes 100 cycles.

                    this->channel_states[process_id] = channel_state::ready;
                    this->channels[process_id] = response;
                }
            }
            wait();
        }
    }

private:
    vector<request> requests = vector<request>();
    vector<request> send_buffer = vector<request>();
    bool ack_ok = false;
    int parallel_num;
    request* channels;
    channel_state* channel_states;
    int id = 0;

    void wait_ack() {
        while (true) {
            wait();
            // This state can be invalid.
            if (this->ack_ok) {
                this->ack_ok = false;
                return;
            }
        }
    }
};

#endif
