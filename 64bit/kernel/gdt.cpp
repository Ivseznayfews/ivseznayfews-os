#include "gdt.h"

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct GDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct TSS {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2, reserved1, ist[7], reserved2;
    uint16_t reserved3, iomap;
} __attribute__((packed));
static TSS tss;
static uint8_t interrupt_stack[32768] __attribute__((aligned(16)));
static GDTEntry g_gdt[7];
static GDTPtr   g_gdtp;

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    g_gdt[i].base_low    = base & 0xFFFF;
    g_gdt[i].base_mid    = (base >> 16) & 0xFF;
    g_gdt[i].base_high   = (base >> 24) & 0xFF;
    g_gdt[i].limit_low   = limit & 0xFFFF;
    g_gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    g_gdt[i].access      = access;
}

extern "C" void gdt_flush(uint64_t);

namespace gdt {
    void init() {
        g_gdtp.limit = sizeof(g_gdt) - 1;
        g_gdtp.base  = (uint64_t)&g_gdt;

        set_entry(0, 0, 0,          0,    0);
        set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);
        set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

        set_entry(3, 0, 0xFFFFFFFF, 0xF2, 0xCF);
        set_entry(4, 0, 0xFFFFFFFF, 0xFA, 0xAF);
        tss.rsp0 = (uint64_t)(interrupt_stack + sizeof(interrupt_stack));
        tss.iomap = sizeof(TSS);
        uint64_t base = (uint64_t)&tss;
        uint64_t low = (sizeof(TSS)-1) | ((base & 0xffffff) << 16) | (0x89ull<<40) | (((base>>24)&255)<<56);
        ((uint64_t*)g_gdt)[5] = low;
        ((uint64_t*)g_gdt)[6] = base>>32;
        gdt_flush((uint64_t)&g_gdtp);
        asm volatile("ltr %0" :: "r"((uint16_t)0x28));
    }
}
