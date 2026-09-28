#include "heap.h"
#include "pmm.h"
#include "vga.h"

struct Block {
    size_t  size;
    int     free;
    Block*  next;
    Block*  prev;
};

static Block* g_head = nullptr;
static size_t g_total_alloc = 0;

static const size_t BLOCK_HEADER = sizeof(Block);

static Block* extend_heap(size_t need) {
    size_t bytes = need + BLOCK_HEADER;
    size_t pages = (bytes + 4095) / 4096;

    uint64_t first = (uint64_t)pmm::alloc_page();
    if (!first) return nullptr;

    for (size_t i = 1; i < pages; i++) {
        uint64_t p = (uint64_t)pmm::alloc_page();
        if (!p || p != first + i * 4096) {
            // 释放已分配的页，避免泄漏
            if (p) pmm::free_page((void*)p);
            for (size_t j = 0; j < i; j++)
                pmm::free_page((void*)(first + j * 4096));
            return nullptr;
        }
    }

    Block* b = (Block*)first;
    b->size = pages * 4096 - BLOCK_HEADER;
    b->free = 1;
    b->next = nullptr;
    b->prev = nullptr;

    if (!g_head) {
        g_head = b;
    } else {
        Block* cur = g_head;
        while (cur->next) cur = cur->next;
        cur->next = b;
        b->prev = cur;
    }
    return b;
}

static void split(Block* b, size_t size) {
    if (b->size < size + BLOCK_HEADER + 16) return;

    Block* nb = (Block*)((uint8_t*)b + BLOCK_HEADER + size);
    nb->size = b->size - size - BLOCK_HEADER;
    nb->free = 1;
    nb->next = b->next;
    nb->prev = b;
    if (b->next) b->next->prev = nb;

    b->size = size;
    b->next = nb;
}

static void try_merge(Block* b) {
    if (b->next && b->next->free) {
        Block* n = b->next;
        b->size += BLOCK_HEADER + n->size;
        b->next = n->next;
        if (n->next) n->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        Block* p = b->prev;
        p->size += BLOCK_HEADER + b->size;
        p->next = b->next;
        if (b->next) b->next->prev = p;
    }
}

namespace heap {
    void init() {
        g_head = nullptr;
        g_total_alloc = 0;
        extend_heap(4096 - BLOCK_HEADER);
    }

    void* kmalloc(size_t size) {
        if (size == 0) return nullptr;
        size = (size + 7) & ~7ULL;

        Block* b = g_head;
        while (b) {
            if (b->free && b->size >= size) {
                split(b, size);
                b->free = 0;
                g_total_alloc += b->size;
                return (uint8_t*)b + BLOCK_HEADER;
            }
            b = b->next;
        }

        Block* nb = extend_heap(size);
        if (!nb) return nullptr;
        split(nb, size);
        nb->free = 0;
        g_total_alloc += nb->size;
        return (uint8_t*)nb + BLOCK_HEADER;
    }

    void kfree(void* p) {
        if (!p) return;
        Block* b = (Block*)((uint8_t*)p - BLOCK_HEADER);
        b->free = 1;
        g_total_alloc -= b->size;
        try_merge(b);
    }

    void stats() {
        size_t total = 0, freeb = 0, blocks = 0;
        Block* b = g_head;
        while (b) {
            total += b->size + BLOCK_HEADER;
            if (b->free) freeb += b->size;
            blocks++;
            b = b->next;
        }
        vga::print("Heap: ");
        vga::print_dec(blocks);
        vga::print(" blocks, ");
        vga::print_dec(total / 1024);
        vga::print(" KB total, ");
        vga::print_dec(freeb / 1024);
        vga::print(" KB free\n");
    }
}
