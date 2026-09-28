#pragma once
#include <stdint.h>

namespace pmm {
    void     init(uint64_t mmap_addr, uint32_t mmap_length);
    void*    alloc_page();       // 返回物理地址，失败返回 nullptr
    void     free_page(void* p);
    uint64_t total_pages();
    uint64_t used_pages();
    void     print_stats();
}
