#pragma once
#include <stdint.h>
#include <stddef.h>

namespace vga {
    size_t cursor_row();
    size_t cursor_col();
    void clear();
    void putchar(char c);
    void print(const char* s);
    void print_hex(uint64_t v);           // 完整 16 位
    void print_hex_short(uint64_t v);     // 无前导零
    void print_dec(uint64_t v);
    void update_cursor();
}
