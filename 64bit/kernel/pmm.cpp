#include "pmm.h"
#include "vga.h"
#include "critical.h"
#include "spinlock.h"
static SpinLock allocator_lock;

static constexpr size_t MAX_PAGES = pmm::MAPPED_BYTES / pmm::PAGE_SIZE;
static uint8_t unavailable[MAX_PAGES / 8];
static uint8_t allocated[MAX_PAGES / 8];
static uint64_t g_total = 0, g_used = 0;
extern "C" uint64_t kernel_end;

static bool get(const uint8_t* bits, size_t i) {
    return (bits[i / 8] >> (i % 8)) & 1;
}
static void set(uint8_t* bits, size_t i, bool value) {
    if (value) bits[i / 8] |= 1u << (i % 8);
    else bits[i / 8] &= ~(1u << (i % 8));
}

static void mark_region(uint64_t base, uint64_t len, bool reserved) {
    if (!len || base >= pmm::MAPPED_BYTES) return;
    uint64_t end = len > pmm::MAPPED_BYTES - base ? pmm::MAPPED_BYTES : base + len;
    // Free only whole usable pages; reserve every page touched by a region.
    size_t first = reserved ? base / 4096 : (base + 4095) / 4096;
    size_t last = reserved ? (end + 4095) / 4096 : end / 4096;
    for (size_t i = first; i < last; ++i) set(unavailable, i, reserved);
}

static bool mapped(uint64_t address, uint64_t length) {
    return address && address < pmm::MAPPED_BYTES && length <= pmm::MAPPED_BYTES - address;
}

namespace pmm {
    bool init(const MultibootInfo* info) {
        InterruptGuard guard; SpinGuard shared(allocator_lock);
        g_total = g_used = 0;
        for (size_t i = 0; i < sizeof(unavailable); ++i) {
            unavailable[i] = 0xff;
            allocated[i] = 0;
        }
        if (!info || !(info->flags & (1u << 6)) || !info->mmap_length ||
            !mapped(info->mmap_addr, info->mmap_length)) return false;
        // Validate every record before opening any pages for allocation.
        uint64_t end = uint64_t(info->mmap_addr) + info->mmap_length;
        for (uint64_t pos = info->mmap_addr; pos < end;) {
            if (end - pos < sizeof(MultibootMmapEntry)) return false;
            auto* entry = (const MultibootMmapEntry*)pos;
            uint64_t step = uint64_t(entry->size) + 4;
            if (step < sizeof(MultibootMmapEntry) || step > end - pos) return false;
            pos += step;
        }
        // Reserved records win even when firmware reports overlapping regions.
        for (int pass = 0; pass < 2; ++pass) {
            for (uint64_t pos = info->mmap_addr; pos < end;) {
                auto* entry = (const MultibootMmapEntry*)pos;
                if ((entry->type == 1) == (pass == 0))
                    mark_region(entry->addr, entry->len, pass != 0);
                pos += uint64_t(entry->size) + 4;
            }
        }
        mark_region(0, (uint64_t)&kernel_end, true);
        mark_region(0, 0x100000, true);
        mark_region((uint64_t)info, sizeof(*info), true);
        mark_region(info->mmap_addr, info->mmap_length, true);
        if (info->flags & (1u << 12)) {
            mark_region(info->framebuffer_addr,
                        uint64_t(info->framebuffer_pitch) * info->framebuffer_height, true);
        }
        if (info->flags & (1u << 3)) {
            struct Module { uint32_t start, end, string, reserved; };
            uint64_t bytes = uint64_t(info->mods_count) * sizeof(Module);
            if (bytes && !mapped(info->mods_addr, bytes)) {
                for (size_t i = 0; i < sizeof(unavailable); ++i) unavailable[i] = 0xff;
                return false;
            }
            auto* modules = (const Module*)(uint64_t)info->mods_addr;
            for (uint32_t i = 0; i < info->mods_count; ++i) {
                if (modules[i].end < modules[i].start) {
                    for (size_t j = 0; j < sizeof(unavailable); ++j) unavailable[j] = 0xff;
                    return false;
                }
                mark_region(modules[i].start, uint64_t(modules[i].end) - modules[i].start, true);
            }
            mark_region(info->mods_addr, bytes, true);
        }
        for (size_t i = 0; i < MAX_PAGES; ++i) if (!get(unavailable, i)) ++g_total;
        return g_total != 0;
    }

    void* alloc_pages(size_t count) {
        InterruptGuard guard; SpinGuard shared(allocator_lock);
        if (!count || count > MAX_PAGES || count > g_total - g_used) return nullptr;
        size_t run = 0;
        for (size_t i = 0; i < MAX_PAGES; ++i) {
            run = get(unavailable, i) ? 0 : run + 1;
            if (run != count) continue;
            size_t first = i + 1 - count;
            for (size_t j = first; j <= i; ++j) {
                set(unavailable, j, true);
                set(allocated, j, true);
            }
            g_used += count;
            return (void*)(first * PAGE_SIZE);
        }
        return nullptr;
    }
    void* alloc_page() { return alloc_pages(1); }

    void free_pages(void* p, size_t count) {
        InterruptGuard guard; SpinGuard shared(allocator_lock);
        uint64_t address = (uint64_t)p;
        size_t first = address / PAGE_SIZE;
        if (!p || address % PAGE_SIZE || !count || first >= MAX_PAGES || count > MAX_PAGES - first) return;
        for (size_t i = first; i < first + count; ++i) if (!get(allocated, i)) return;
        for (size_t i = first; i < first + count; ++i) {
            set(unavailable, i, false);
            set(allocated, i, false);
        }
        g_used -= count;
    }
    void free_page(void* p) { free_pages(p, 1); }
    uint64_t total_pages() { InterruptGuard guard; SpinGuard shared(allocator_lock); return g_total; }
    uint64_t used_pages() { InterruptGuard guard; SpinGuard shared(allocator_lock); return g_used; }
    void print_stats() {
        uint64_t total, used;
        { InterruptGuard guard; SpinGuard shared(allocator_lock); total = g_total; used = g_used; }
        vga::print("PMM: managed="); vga::print_dec(total);
        vga::print(" pages, allocated="); vga::print_dec(used);
        vga::print(", free="); vga::print_dec(total - used);
        vga::print(" pages ("); vga::print_dec((total - used) * 4);
        vga::print(" KB)\n");
    }
}
