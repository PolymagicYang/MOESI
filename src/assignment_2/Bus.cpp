//
// Created by yanghoo on 2/22/24.
//
#include "Bus.h"

int Bus::try_request(request_id req) {
    this->requests.push_back(req);
    return 0;
}
