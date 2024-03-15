//
// Created by yanghoo on 2/24/24.
//
#include <systemc.h>
#include "types.h"

#ifndef FRAMEWORK_MEMORY_IF_H
#define FRAMEWORK_MEMORY_IF_H

/* NOTE: This interface is implemented by the cache. The Manager uses it to talk
 * to the cache. */
class Memory_if: public virtual sc_interface {
public:
    virtual int read(request) = 0;
    virtual int write(request) = 0;
    virtual void ack() = 0;
    virtual std::vector<request> get_requests() = 0;
};

#endif //FRAMEWORK_MEMORY_IF_H
