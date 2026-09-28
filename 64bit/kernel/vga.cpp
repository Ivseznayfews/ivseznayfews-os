#include "vga.h"
#include "io.h"
#include "task.h"

static const size_t VGA_WIDTH  = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEM = (uint16_t*)0xB8000;

static size_t row = 0, col = 0;
static uint8_t color = 0x0F;

static uint16_t entry(char c) { return (uint16_t)c | ((uint16_t)color << 8); }

static void scroll() {
    if (row < VGA_HEIGHT) return;
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y-1)*VGA_WIDTH + x] = VGA_MEM[y*VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT-1)*VGA_WIDTH + x] = entry(' ');
    row = VGA_HEIGHT - 1;
}

namespace vga {
    void update_cursor() {
        uint16_t pos = row * VGA_WIDTH + col;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }

    void clear() {
        task::lock();
        for (size_t i = 0; i < VGA_WIDTH*VGA_HEIGHT; i++) VGA_MEM[i] = entry(' ');
        row = col = 0;
        update_cursor();
        task::unlock();
    }

    void putchar(char c) {
        task::lock();
        if (c == '\n') {
            col = 0; row++; scroll(); update_cursor();
        } else if (c == '\b') {
            if (col > 0) col--;
            else if (row > 0) { row--; col = VGA_WIDTH - 1; }
            VGA_MEM[row*VGA_WIDTH + col] = entry(' ');
            update_cursor();
        } else {
            VGA_MEM[row*VGA_WIDTH + col] = entry(c);
            if (++col == VGA_WIDTH) { col = 0; row++; scroll(); }
            update_cursor();
        }
        task::unlock();
    }

    void print(const char* s) { while (*s) putchar(*s++); }

    void print_hex(uint64_t v) {
        const char* hex = "0123456789ABCDEF";
        print("0x");
        for (int i = 60; i >= 0; i -= 4) putchar(hex[(v >> i) & 0xF]);
    }

    void print_hex_short(uint64_t v) {
        const char* hex = "0123456789ABCDEF";
        print("0x");
        if (v == 0) { putchar('0'); return; }
        int started = 0;
        for (int i = 60; i >= 0; i -= 4) {
            int d = (v >> i) & 0xF;
            if (d || started) { putchar(hex[d]); started = 1; }
        }
    }

    void print_dec(uint64_t v) {
        if (v == 0) { putchar('0'); return; }
        char buf[24]; int i = 0;
        while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
        while (i--) putchar(buf[i]);
    }
}
