#pragma once
#include <stdint.h>

namespace pit {
    void init(uint32_t freq_hz);
    uint64_t ticks();
}
