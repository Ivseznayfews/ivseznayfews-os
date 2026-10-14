#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ata {
    bool init();
    bool read(uint32_t lba, void* sector);
    bool write(uint32_t lba, const void* sector);
    bool flush();
    uint32_t sectors();
}
namespace vfs {
    struct Entry {char name[13];uint32_t size,cluster;bool directory;};
    bool mount();
    bool mounted();
    bool entry(unsigned index,Entry& result);
    bool entry_at(const char* directory,unsigned index,Entry& result);
    bool lookup(const char* path,Entry& result);
    int read(const char* path,void* data,size_t capacity);
    int write(const char* path,const void* data,size_t size);
    bool mkdir(const char* path);
    void list(const char* path="/");
    void cat(const char* path);
    void test();
}
