//
// Created by yanghoo on 2/25/24.
//
#include <systemc.h>

#ifndef FRAMEWORK_CPU_IF_H
#define FRAMEWORK_CPU_IF_H

class cpu_access_if: public virtual sc_interface {
public:
    virtual int read(uint64_t addr) = 0;
    virtual int write(uint64_t addr) = 0;
};

#endif //FRAMEWORK_CPU_IF_H
