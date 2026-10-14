#pragma once
#include <stdint.h>

namespace gdt {
    void init();
    void stack(uint64_t top);
}
