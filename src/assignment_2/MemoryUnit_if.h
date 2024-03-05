//
// Created by yanghoo on 3/5/24.
//

#ifndef FRAMEWORK_MEMORYUNIT_IF_H
#define FRAMEWORK_MEMORYUNIT_IF_H
#include <systemc.h>
#include "types.h"

class Memory_if: public virtual sc_interface {
public:
    virtual int read(request) = 0;
    virtual int write(request) = 0;
    virtual void ack() = 0;
    virtual std::vector<request> get_requests() = 0;
};

#endif //FRAMEWORK_MEMORYUNIT_IF_H
