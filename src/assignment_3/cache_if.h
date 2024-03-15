#include <systemc.h>
#include "types.h"

#ifndef CPU_CACHE_IF_H
#define CPU_CACHE_IF_H

/* NOTE: This interface is implemented by the cache. The Manager uses it to talk
 * to the cache. */
class cache_if : public virtual sc_interface {
    public:
    virtual int cpu_read(uint64_t addr) = 0;

    virtual int cpu_write(uint64_t addr) = 0;

    virtual int send_new_event() = 0;

    virtual int send_data(request) = 0;

    virtual int ack() = 0;

    virtual int put_ack_from(location) = 0;

    virtual std::vector<request> get_requests() = 0;

    virtual bool get_cacheline_status(uint64_t, cache_status*) = 0;

    virtual bool has_data(uint64_t) = 0;
};

#endif
