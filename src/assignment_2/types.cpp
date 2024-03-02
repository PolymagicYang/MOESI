//
// Created by yanghoo on 2/25/24.
//
#include "types.h"
using namespace::std;

std::ostream& operator<<(std::ostream& os, const request& val) {
    os << "cpu: " << to_string(val.cpu_id) <<  std::endl;
    os << "addr: " << val.addr<<  std::endl;
    os << "source: " << val.source <<  std::endl;
    os << "destination: " << val.destination <<  std::endl;
    os << "op: " << val.op <<  std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const request_id& val) {
    os << "cpu: " << to_string(val.cpu_id) <<  std::endl;
    os << "source: " << val.source <<  std::endl;
    return os;
}