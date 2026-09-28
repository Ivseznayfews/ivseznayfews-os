#pragma once
#include <stdint.h>

namespace serial {
    void init();
    void putchar(char c);
    void print(const char* s);
    void print_hex(uint32_t v);
    void print_dec(uint32_t v);
}
