//
// Created by yanghoo on 2/22/24.
//
#include "Bus.h"

int Bus::try_request(request req) {
    this->requests.push_back(req);
    return 0;
}
