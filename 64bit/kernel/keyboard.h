#pragma once
#include <stdint.h>

typedef void (*key_callback_t)(char c);

namespace keyboard {
    void init();
    void poll();
    uint64_t dropped_keys();
    void set_callback(key_callback_t cb);
}
