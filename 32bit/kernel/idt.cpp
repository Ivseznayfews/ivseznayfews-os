#include "idt.h"
#include "io.h"
#include <stddef.h>

struct IDTEntry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static IDTEntry g_idt[256];
static IDTPtr   g_idtp;
static isr_handler_t g_handlers[256];

extern "C" void (*isr_table[32])();
extern "C" void (*irq_table[16])();

static void set_gate(int n, uint32_t base) {
    g_idt[n].base_low  = base & 0xFFFF;
    g_idt[n].base_high = (base >> 16) & 0xFFFF;
    g_idt[n].sel       = 0x08;
    g_idt[n].always0   = 0;
    g_idt[n].flags     = 0x8E;
}

namespace idt {
    void init() {
        g_idtp.limit = sizeof(g_idt) - 1;
        g_idtp.base  = (uint32_t)&g_idt;

        for (int i = 0; i < 256; i++) {
            g_idt[i].base_low = g_idt[i].base_high = 0;
            g_idt[i].sel = g_idt[i].always0 = g_idt[i].flags = 0;
            g_handlers[i] = nullptr;
        }

        for (int i = 0; i < 32; i++) set_gate(i,      (uint32_t)isr_table[i]);
        for (int i = 0; i < 16; i++) set_gate(32 + i, (uint32_t)irq_table[i]);

        asm volatile("lidt %0" : : "m"(g_idtp));
        // 注意：这里不再 sti，交给 kmain 最后统一开中断
    }

    void set_handler(int n, isr_handler_t h) { g_handlers[n] = h; }

    extern "C" void isr_dispatch(Registers* r) {
        if (g_handlers[r->int_no]) g_handlers[r->int_no](r);
    }

    // 关键修复：所有 IRQ 处理完统一发 EOI，不依赖具体 handler
    extern "C" void irq_dispatch(Registers* r) {
        if (g_handlers[r->int_no]) g_handlers[r->int_no](r);

        // 从 PIC (IRQ8-15) 需要向两个 PIC 都发 EOI
        if (r->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}
