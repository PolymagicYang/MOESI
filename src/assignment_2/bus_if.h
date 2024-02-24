#include <systemc.h>

#ifndef BUS_SLAVE_IF_H
#define BUS_SLAVE_IF_H

class bus_if : public virtual sc_interface {
    public:
    virtual int try_request(request) = 0;
};

#endif
