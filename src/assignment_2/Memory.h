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
    sc_in_clk clk;

    Memory(sc_module_name name_) : sc_module(name_) {
        SC_THREAD(send);
        sensitive << clk.pos();
        SC_THREAD(execute);
        SC_THREAD(dispatch);
        sensitive << clk.pos();
        dont_initialize();
    }

    SC_HAS_PROCESS(Memory); // Needed because we didn't use SC_TOR

    ~Memory() override {
        // nothing to do here right now.
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

    void execute() {
        while (true) {
            wait(clk.negedge_event());
            for (auto & task : this->pipeline) {
                task.cycles += 1;
            }
        }
    }

    void send() {
        while (true) {
            if ((!this->pipeline.empty()) && this->pipeline.front().cycles >= 100) {
                cout << "send back" << endl;
                auto response = this->pipeline.front().req;
                this->pipeline.erase(this->pipeline.begin());
                this->send_buffer.push_back(response);

                request_id response_id;
                response_id.cpu_id = response.cpu_id;
                response_id.source = location::memory;
                this->bus->try_request(response_id);
                this->wait_ack();
            } else {
                wait();
            }
        }
    }

    void dispatch() {
        while (true) {
            if (!this->requests.empty()) {
                auto req = this->requests.front();
                this->requests.erase(this->requests.begin());

                if (req.source != location::memory) {
                    stats_memory_access(req.cpu_id, 1);
                    request response;
                    response.cpu_id = req.cpu_id;
                    response.addr = req.addr;
                    response.source = location::memory;
                    response.destination = location::cache;
                    response.op = req.op;
                    this->pipeline.push_back(task{.req =  response, .cycles =  0, .start_time = sc_time_stamp()});
                }
            }
            wait();
        }
    }

private:
    vector<request> requests = vector<request>();
    vector<request> send_buffer = vector<request>();
    typedef struct task {
        request req;
        int cycles;
        sc_time start_time;
    } task;
    vector<task> pipeline = vector<task>();

    bool ack_ok = false;
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
