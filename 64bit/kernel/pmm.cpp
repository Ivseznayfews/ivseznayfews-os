#include "pmm.h"
#include "multiboot.h"
#include "vga.h"

#define MAX_PAGES   (1024 * 1024 / 4)
#define BITMAP_SIZE (MAX_PAGES / 8)

static uint8_t  g_bitmap[BITMAP_SIZE];
static uint64_t g_total = 0;
static uint64_t g_used  = 0;

extern "C" uint64_t kernel_end;

static inline void bit_set(uint64_t i)   { g_bitmap[i >> 3] |=  (1 << (i & 7)); }
static inline void bit_clear(uint64_t i) { g_bitmap[i >> 3] &= ~(1 << (i & 7)); }
static inline int  bit_get(uint64_t i)   { return (g_bitmap[i >> 3] >> (i & 7)) & 1; }

static void mark_region(uint64_t base, uint64_t len, int used) {
    uint64_t start = base / 4096;
    uint64_t end   = (base + len + 4095) / 4096;
    if (end > MAX_PAGES) end = MAX_PAGES;
    for (uint64_t i = start; i < end; i++) {
        if (used) {
            if (!bit_get(i)) { bit_set(i); g_used++; }
        } else {
            if (bit_get(i))  { bit_clear(i); g_used--; }
        }
    }
}

namespace pmm {
    void init(uint64_t mmap_addr, uint32_t mmap_length) {
        // 初始全部标记为"已用"
        for (uint64_t i = 0; i < BITMAP_SIZE; i++) g_bitmap[i] = 0xFF;
        g_total = MAX_PAGES;
        g_used  = MAX_PAGES;

        // 遍历 mmap：把 type=1 的可用区域标记为空闲
        uint64_t addr = mmap_addr;
        uint64_t end  = addr + mmap_length;
        while (addr < end) {
            MultibootMmapEntry* e = (MultibootMmapEntry*)addr;
            if (e->type == 1) {
                mark_region(e->addr, e->len, 0);
            }
            addr += e->size + 4;
        }

        // 关键修复：把低端 1MB 强制标记为已用（BIOS、VGA、实模式向量等）
        mark_region(0, 0x100000, 1);

        // 重新占用内核自身 (0x100000 ~ kernel_end)
        uint64_t kstart = 0x100000;
        uint64_t kend   = (uint64_t)&kernel_end;
        if (kend > kstart) mark_region(kstart, kend - kstart, 1);

        // bitmap 自身所在的页也占用
        mark_region((uint64_t)g_bitmap, sizeof(g_bitmap), 1);
    }

    void* alloc_page() {
        // 从 1MB 之后开始扫描（跳过低端内存）
        uint64_t start_page = 0x100000 / 4096;
        for (uint64_t i = start_page; i < MAX_PAGES; i++) {
            if (!bit_get(i)) {
                bit_set(i);
                g_used++;
                return (void*)(i * 4096);
            }
        }
        return nullptr;
    }

    void free_page(void* p) {
        uint64_t i = (uint64_t)p / 4096;
        if (i < MAX_PAGES && bit_get(i)) {
            bit_clear(i);
            g_used--;
        }
    }

    uint64_t total_pages() { return g_total; }
    uint64_t used_pages()  { return g_used; }

    void print_stats() {
        vga::print("PMM: total=");
        vga::print_dec(g_total);
        vga::print(" pages, used=");
        vga::print_dec(g_used);
        vga::print(", free=");
        vga::print_dec(g_total - g_used);
        vga::putchar('\n');
        vga::print("PMM: free memory = ");
        vga::print_dec((g_total - g_used) * 4);
        vga::print(" KB\n");
    }
}
