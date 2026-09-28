#pragma once
#include <stdint.h>

typedef void (*key_callback_t)(char c);

namespace keyboard {
    void init();
    void set_callback(key_callback_t cb);
}
