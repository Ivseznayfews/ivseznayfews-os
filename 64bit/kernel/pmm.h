#pragma once
#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

namespace pmm {
    constexpr uint64_t PAGE_SIZE = 4096;
    constexpr uint64_t MAPPED_BYTES = 1ull << 30;
    // Manages only complete usable pages within the boot identity mapping.
    bool init(const MultibootInfo* info);
    void* alloc_pages(size_t count);
    void* alloc_page();
    void free_pages(void* p, size_t count);
    void free_page(void* p);
    uint64_t total_pages();
    uint64_t used_pages();
    void print_stats();
}
