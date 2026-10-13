#pragma once
#include <stddef.h>

namespace heap {
    void  init();
    void* kmalloc(size_t size);
    void  kfree(void* p);
    size_t allocated_bytes();
    void  stats();
}
