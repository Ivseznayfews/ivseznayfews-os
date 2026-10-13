#pragma once
namespace net {
    bool init();
    void poll();
    void stats();
    void ping(const char* address);
    void exchange(const char* args, bool tcp);
}
