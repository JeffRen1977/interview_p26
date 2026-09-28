// One inbound gateway event.
#pragma once

#include <string>

struct Request {
    std::string user_id;
    std::string endpoint;
    // Milliseconds since epoch (or any monotonic ms clock).
    // The engine NEVER reads a clock; callers and tests inject timestamps.
    long long timestamp = 0;
    // How many quota units this event consumes (weighted limiter).
    int cost = 1;
};
