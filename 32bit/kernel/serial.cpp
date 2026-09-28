#include "serial.h"
#include "io.h"

namespace serial {
    void init() {
        outb(0x3F8 + 1, 0x00);
        outb(0x3F8 + 3, 0x80);
        outb(0x3F8 + 0, 0x03);
        outb(0x3F8 + 1, 0x00);
        outb(0x3F8 + 3, 0x03);
        outb(0x3F8 + 2, 0xC7);
        outb(0x3F8 + 4, 0x0B);
    }
    void putchar(char c) { outb(0x3F8, (uint8_t)c); }
    void print(const char* s) { while (*s) putchar(*s++); }
    void print_hex(uint32_t v) {
        const char* hex = "0123456789ABCDEF";
        print("0x");
        for (int i = 28; i >= 0; i -= 4) putchar(hex[(v >> i) & 0xF]);
    }
    void print_dec(uint32_t v) {
        if (v == 0) { putchar('0'); return; }
        char buf[16]; int i = 0;
        while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
        while (i--) putchar(buf[i]);
    }
}
