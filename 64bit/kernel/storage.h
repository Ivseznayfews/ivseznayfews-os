#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ata {
    bool init();
    bool read(uint32_t lba, void* sector);
    uint32_t sectors();
}
namespace vfs {
    struct Entry { char name[13]; uint32_t size, cluster; };
    bool mount();
    bool mounted();
    bool entry(unsigned index, Entry& result);
    bool lookup(const char* name, Entry& result);
    int read(const char* name, void* data, size_t capacity);
    void list();
    void cat(const char* name);
}
