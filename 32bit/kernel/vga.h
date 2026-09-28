#pragma once
#include <stdint.h>
#include <stddef.h>

namespace vga {
    void clear();
    void putchar(char c);
    void print(const char* s);
    void print_hex(uint32_t v);
    void update_cursor();
}
