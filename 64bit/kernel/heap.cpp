#include "heap.h"
#include "pmm.h"
#include "vga.h"
#include "critical.h"

struct alignas(16) Block {
    size_t size;
    bool free;
    Block* next;
    Block* prev;
};
static Block* g_head = nullptr;
static size_t g_total_alloc = 0;
static constexpr size_t HEADER = sizeof(Block);
static_assert(HEADER % 16 == 0, "heap payload must stay aligned");

static bool adjacent(Block* a, Block* b) {
    return (uint8_t*)a + HEADER + a->size == (uint8_t*)b;
}
static void merge_next(Block* b) {
    while (b->next && b->next->free && adjacent(b, b->next)) {
        Block* next = b->next;
        b->size += HEADER + next->size;
        b->next = next->next;
        if (b->next) b->next->prev = b;
    }
}
static Block* coalesce(Block* b) {
    if (b->prev && b->prev->free && adjacent(b->prev, b)) b = b->prev;
    merge_next(b);
    return b;
}
static Block* extend_heap(size_t need) {
    if (need > pmm::MAPPED_BYTES - HEADER - 4095) return nullptr;
    size_t pages = (need + HEADER + 4095) / 4096;
    Block* b = (Block*)pmm::alloc_pages(pages);
    if (!b) return nullptr;
    b->size = pages * 4096 - HEADER;
    b->free = true;
    // Keep separate physical extents ordered; merge only touching extents.
    Block* prev = nullptr;
    Block* next = g_head;
    while (next && (uintptr_t)next < (uintptr_t)b) { prev = next; next = next->next; }
    b->next = next;
    b->prev = prev;
    if (prev) prev->next = b;
    else g_head = b;
    if (next) next->prev = b;
    return coalesce(b);
}
static void split(Block* b, size_t size) {
    if (b->size - size < HEADER + 16) return;
    Block* next = (Block*)((uint8_t*)b + HEADER + size);
    next->size = b->size - size - HEADER;
    next->free = true;
    next->next = b->next;
    next->prev = b;
    if (b->next) b->next->prev = next;
    b->size = size;
    b->next = next;
}
namespace heap {
    void init() {
        InterruptGuard guard;
        // Initialization happens once, after PMM setup.
        g_head = nullptr;
        g_total_alloc = 0;
        extend_heap(4096 - HEADER);
    }
    void* kmalloc(size_t size) {
        InterruptGuard guard;
        if (!size || size > SIZE_MAX - 15) return nullptr;
        size = (size + 15) & ~size_t(15);
        if (size > pmm::MAPPED_BYTES - HEADER - 4095) return nullptr;
        Block* b = g_head;
        while (b && (!b->free || b->size < size)) b = b->next;
        if (!b) b = extend_heap(size);
        if (!b) return nullptr;
        split(b, size);
        b->free = false;
        g_total_alloc += b->size;
        return (uint8_t*)b + HEADER;
    }
    void kfree(void* p) {
        InterruptGuard guard;
        if (!p) return;
        // Validate against live payloads before accessing a header.
        for (Block* b = g_head; b; b = b->next) {
            if ((uint8_t*)b + HEADER != p) continue;
            if (b->free) return;
            g_total_alloc -= b->size;
            b->free = true;
            coalesce(b);
            return;
        }
    }
    size_t allocated_bytes() { InterruptGuard guard; return g_total_alloc; }
    void stats() {
        size_t total = 0, free_bytes = 0, blocks = 0, used = 0;
        {
            InterruptGuard guard;
            for (Block* b = g_head; b; b = b->next) {
                total += b->size + HEADER;
                if (b->free) free_bytes += b->size;
                ++blocks;
            }
            used = g_total_alloc;
        }
        vga::print("Heap: "); vga::print_dec(blocks);
        vga::print(" blocks, "); vga::print_dec(total);
        vga::print(" bytes total, "); vga::print_dec(used);
        vga::print(" allocated, "); vga::print_dec(free_bytes);
        vga::print(" free\n");
    }
}
