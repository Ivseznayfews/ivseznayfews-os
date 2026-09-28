#include "multiboot.h"
#include "vga.h"
#include "serial.h"

static MultibootInfo* g_mbi = nullptr;

namespace multiboot {
    void set_info(uint64_t ptr) { g_mbi = (MultibootInfo*)ptr; }

    uint32_t total_memory_kb() {
        if (!g_mbi) return 0;
        return g_mbi->mem_lower + g_mbi->mem_upper;
    }

    void print_mmap_summary() {
        if (!g_mbi) { vga::print("no mbi\n"); return; }

        vga::print("flags = ");
        vga::print_hex_short(g_mbi->flags);
        vga::putchar('\n');

        if (g_mbi->flags & (1 << 0)) {
            vga::print("mem_lower = ");
            vga::print_dec(g_mbi->mem_lower);
            vga::print(" KB\nmem_upper = ");
            vga::print_dec(g_mbi->mem_upper);
            vga::print(" KB\n");
        }

        if (g_mbi->flags & (1 << 6)) {
            vga::print("mmap: ");
            vga::print_dec(g_mbi->mmap_length / 24);
            vga::print(" entries\n");

            uint64_t addr = g_mbi->mmap_addr;
            uint64_t end  = addr + g_mbi->mmap_length;
            while (addr < end) {
                MultibootMmapEntry* e = (MultibootMmapEntry*)addr;
                vga::print("  ");
                vga::print_hex_short(e->addr);
                vga::print(" + ");
                vga::print_hex_short(e->len);
                if (e->type == 1) vga::print("  [usable]");
                else vga::print("  [reserved]");
                vga::putchar('\n');
                addr += e->size + 4;
            }
        }
    }
}
