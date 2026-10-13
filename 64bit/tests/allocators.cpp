// Compile the actual kernel allocators with only privileged IRQ operations stubbed.
#include "pmm.h"
#include "heap.h"
#include "vga.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

extern "C" { uint64_t kernel_end = 0; }
namespace vga {
    void print(const char*) {}
    void print_dec(uint64_t) {}
    void putchar(char) {}
}
static constexpr uintptr_t BASE = 0x1000000;
static constexpr size_t LENGTH = 8 * 1024 * 1024;
static MultibootInfo* info;
static MultibootMmapEntry* entries;
static void setup(bool partial = false) {
    info = (MultibootInfo*)BASE;
    entries = (MultibootMmapEntry*)(BASE + 256);
    memset(info, 0, 4096);
    info->flags = 1u << 6;
    info->mmap_addr = BASE + 256;
    info->mmap_length = 3 * sizeof(*entries);
    // Put a reserved overlap before a usable region to test precedence.
    entries[0] = {20, BASE + 3 * 4096 + 10, 4096, 2};
    entries[1] = {20, BASE + (partial ? 100 : 0), LENGTH - (partial ? 200 : 0), 1};
    entries[2] = {20, pmm::MAPPED_BYTES + 4096, UINT64_MAX, 1};
    assert(pmm::init(info));
}
static void physical_tests() {
    setup(true);
    assert(pmm::total_pages() == LENGTH / 4096 - 4);
    assert(pmm::used_pages() == 0);
    pmm::free_page((void*)(BASE + 3 * 4096)); // Reserved page is never freed.
    pmm::free_page((void*)0x100000);
    assert(pmm::used_pages() == 0);
    void* contiguous = pmm::alloc_pages(3);
    assert(contiguous == (void*)(BASE + 5 * 4096));
    void* early = pmm::alloc_page();
    assert(early == (void*)(BASE + 4096)); // No partial run was consumed.
    assert(pmm::used_pages() == 4);
    pmm::free_pages(contiguous, 4); // Last page wasn't allocated: atomic rejection.
    pmm::free_page((char*)early + 1);
    assert(pmm::used_pages() == 4);
    pmm::free_pages(contiguous, 3);
    pmm::free_pages(contiguous, 3);
    pmm::free_page(early);
    assert(pmm::used_pages() == 0);
    assert(!pmm::alloc_pages(0));
    assert(!pmm::alloc_pages(SIZE_MAX));
    assert(!pmm::alloc_pages(pmm::total_pages())); // Holes prevent a full-length run.
    assert(pmm::used_pages() == 0);
    // Exhaust the pool, then release it and verify the accounting exactly.
    void* pages[LENGTH / 4096];
    size_t n = 0;
    while ((pages[n] = pmm::alloc_page())) ++n;
    assert(n == pmm::total_pages());
    assert(pmm::used_pages() == n);
    assert(!pmm::alloc_pages(2));
    for (size_t i = 0; i < n; ++i) pmm::free_page(pages[i]);
    assert(pmm::used_pages() == 0);

    setup();
    info->flags |= 1u << 3;
    info->mods_addr = BASE + 512;
    info->mods_count = 1;
    uint32_t* module = (uint32_t*)(BASE + 512);
    module[0] = BASE + 6 * 4096;
    module[1] = BASE + 8 * 4096;
    assert(pmm::init(info));
    assert(pmm::total_pages() == LENGTH / 4096 - 5);
    pmm::free_page((void*)(BASE + 6 * 4096));
    assert(pmm::used_pages() == 0);
    // Malformed record lengths fail closed; unavailable bytes never become free.
    entries[0].size = UINT32_MAX;
    assert(!pmm::init(info));
    assert(pmm::total_pages() == 0);
    assert(!pmm::alloc_page());
    setup();
    info->mmap_length -= 1;
    assert(!pmm::init(info));
    assert(!pmm::alloc_page());
    setup();
    // Overflowing region end is clamped, and reserved regions take precedence.
    entries[2] = {20, BASE + 7 * 4096, UINT64_MAX, 2};
    assert(pmm::init(info));
    assert(pmm::total_pages() == 4); // 1,2,5,6.
    setup();
    entries[2] = {20, pmm::MAPPED_BYTES - 4096, UINT64_MAX, 1};
    assert(pmm::init(info));
    assert(pmm::total_pages() == LENGTH / 4096 - 2);
    n = 0;
    while ((pages[n] = pmm::alloc_page())) ++n;
    assert(pages[n - 1] == (void*)(pmm::MAPPED_BYTES - 4096));
    for (size_t i = 0; i < n; ++i) pmm::free_page(pages[i]);
    assert(pmm::used_pages() == 0);
    setup();
    info->mods_count = 1;
    info->mods_addr = pmm::MAPPED_BYTES - 4;
    info->flags |= 1u << 3;
    assert(!pmm::init(info));
    assert(!pmm::alloc_page());
    puts("PASS: PMM rounding, overlaps, reservations, contiguous runs, exhaustion, invalid frees/maps");
}
static void heap_tests() {
    setup();
    heap::init();
    auto* a = (unsigned char*)heap::kmalloc(4000);
    assert(a && ((uintptr_t)a % 16) == 0);
    void* blocker = pmm::alloc_page();
    assert(blocker == (void*)(BASE + 2 * 4096));
    memset(blocker, 0xa5, 4096);
    auto* b = (unsigned char*)heap::kmalloc(4096);
    assert(b && b > a);
    heap::kfree(a);
    heap::kfree(b);
    assert(heap::allocated_bytes() == 0);
    auto* large = (unsigned char*)heap::kmalloc(8000);
    assert(large);
    memset(large, 0x5a, 8000);
    for (size_t i = 0; i < 4096; ++i) assert(((unsigned char*)blocker)[i] == 0xa5);
    auto* reuse = (unsigned char*)heap::kmalloc(4000);
    assert(reuse == a);
    size_t bytes = heap::allocated_bytes();
    heap::kfree(reuse + 1);
    heap::kfree((void*)0xdeadbee);
    assert(heap::allocated_bytes() == bytes);
    heap::kfree(reuse);
    heap::kfree(reuse);
    heap::kfree(large);
    assert(heap::allocated_bytes() == 0);
    assert(!heap::kmalloc(0));
    assert(!heap::kmalloc(SIZE_MAX));
    assert(!heap::kmalloc(SIZE_MAX - 16));
    assert(!heap::kmalloc(pmm::MAPPED_BYTES));
    // Repeated fragmentation with deterministic payload checking.
    unsigned char* slots[64] = {};
    size_t sizes[64] = {};
    unsigned char patterns[64] = {};
    uint32_t state = 17;
    for (unsigned step = 0; step < 4000; ++step) {
        state = state * 1664525u + 1013904223u;
        unsigned index = (state >> 16) % 64;
        if (slots[index]) {
            for (size_t j = 0; j < sizes[index]; ++j) assert(slots[index][j] == patterns[index]);
            heap::kfree(slots[index]);
            slots[index] = nullptr;
        } else {
            sizes[index] = 1 + state % 16000;
            patterns[index] = (unsigned char)step;
            slots[index] = (unsigned char*)heap::kmalloc(sizes[index]);
            assert(slots[index] && ((uintptr_t)slots[index] & 15) == 0);
            memset(slots[index], patterns[index], sizes[index]);
        }
    }
    for (unsigned i = 0; i < 64; ++i) {
        if (slots[i]) for (size_t j = 0; j < sizes[i]; ++j) assert(slots[i][j] == patterns[i]);
        heap::kfree(slots[i]);
    }
    assert(heap::allocated_bytes() == 0);
    for (size_t i = 0; i < 4096; ++i) assert(((unsigned char*)blocker)[i] == 0xa5);
    puts("PASS: Heap noncontiguous extents, alignment, reuse, invalid/double frees, overflow, 4000 stress operations");
}
int main() {
    void* arena = mmap((void*)BASE, LENGTH, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (arena == MAP_FAILED) { perror("test arena mmap"); return 1; }
    physical_tests();
    heap_tests();
    assert(munmap(arena, LENGTH) == 0);
}
