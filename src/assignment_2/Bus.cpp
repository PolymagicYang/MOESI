//
// Created by yanghoo on 2/22/24.
//
#include "Bus.h"
#include "types.h"

int Bus::try_request(request req) {
    this->requests.push_back(req);
}
