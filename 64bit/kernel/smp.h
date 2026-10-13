#pragma once
#include <stdint.h>
namespace smp {
    void init();
    unsigned count();
    bool submit(void (*function)(void*),void* context);
    bool work();
    void stats();
    void test();
}
