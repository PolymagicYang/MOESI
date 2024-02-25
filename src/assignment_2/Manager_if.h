//
// Created by yanghoo on 2/26/24.
//

#ifndef FRAMEWORK_MANAGER_IF_H
#define FRAMEWORK_MANAGER_IF_H

#include <systemc.h>

class Manager_if : public virtual sc_interface {
public:
    virtual int finish() = 0;
};

#endif //FRAMEWORK_MANAGER_IF_H
