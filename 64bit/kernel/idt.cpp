#include "idt.h"
#include "io.h"
#include <stddef.h>

struct IDTEntry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static IDTEntry g_idt[256];
static IDTPtr   g_idtp;
static isr_handler_t g_handlers[256];

extern "C" void (*isr_table[32])();
extern "C" void (*irq_table[16])();

static void set_gate(int n, uint64_t base) {
    g_idt[n].base_low  = base & 0xFFFF;
    g_idt[n].base_mid  = (base >> 16) & 0xFFFF;
    g_idt[n].base_high = (base >> 32) & 0xFFFFFFFF;
    g_idt[n].sel       = 0x08;
    g_idt[n].ist       = 0;
    g_idt[n].flags     = 0x8E;
    g_idt[n].reserved  = 0;
}

namespace idt {
    void init() {
        g_idtp.limit = sizeof(g_idt) - 1;
        g_idtp.base  = (uint64_t)&g_idt;

        for (int i = 0; i < 256; i++) {
            g_idt[i].base_low = g_idt[i].base_mid = 0;
            g_idt[i].base_high = g_idt[i].reserved = 0;
            g_idt[i].sel = g_idt[i].ist = g_idt[i].flags = 0;
            g_handlers[i] = nullptr;
        }

        for (int i = 0; i < 32; i++) set_gate(i,      (uint64_t)isr_table[i]);
        for (int i = 0; i < 16; i++) set_gate(32 + i, (uint64_t)irq_table[i]);

        asm volatile("lidt %0" : : "m"(g_idtp));
        // 不在这里 sti，交给 kmain 最后统一开
    }

    void set_handler(int n, isr_handler_t h) { g_handlers[n] = h; }

    extern "C" void isr_dispatch(Registers* r) {
        if (g_handlers[r->int_no]) g_handlers[r->int_no](r);
    }

    extern "C" void irq_dispatch(Registers* r) {
        if (g_handlers[r->int_no]) g_handlers[r->int_no](r);
        if (r->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}
