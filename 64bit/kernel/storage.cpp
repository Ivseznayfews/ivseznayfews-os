#include "storage.h"
#include "io.h"
#include "vga.h"
static uint32_t disk_sectors;
static bool wait_disk(bool drq) {
    for(unsigned i=0;i<1000000;++i) {
        uint8_t s=inb(0x1f7);
        if(s==0xff || s==0) return false;
        if(!(s&0x80)) { if(s&0x21) return false; if(!drq || (s&8)) return true; }
    }
    return false;
}
namespace ata {
    bool init() {
        outb(0x3f6,2); outb(0x1f6,0xa0);
        for(int i=0;i<4;++i) inb(0x3f6);
        outb(0x1f2,0);outb(0x1f3,0);outb(0x1f4,0);outb(0x1f5,0);outb(0x1f7,0xec);
        if(!wait_disk(true) || inb(0x1f4) || inb(0x1f5)) return false;
        uint16_t words[256]; for(auto& word:words) word=inw(0x1f0);
        if(!(words[49]&(1<<9))) return false;
        disk_sectors=uint32_t(words[60])|(uint32_t(words[61])<<16);
        return disk_sectors!=0;
    }
    bool read(uint32_t lba,void* data) {
        if(!disk_sectors || lba>=disk_sectors || lba>=0x10000000 || !wait_disk(false)) return false;
        outb(0x1f6,0xe0|(lba>>24));
        for(int i=0;i<4;++i) inb(0x3f6);
        outb(0x1f2,1);outb(0x1f3,lba);outb(0x1f4,lba>>8);outb(0x1f5,lba>>16);outb(0x1f7,0x20);
        if(!wait_disk(true)) return false;
        auto* out=(uint16_t*)data;
        for(int i=0;i<256;++i) out[i]=inw(0x1f0);
        return wait_disk(false);
    }
    uint32_t sectors() { return disk_sectors; }
}
static uint16_t u16(const uint8_t* p) { return p[0]|uint16_t(p[1])<<8; }
static uint32_t u32(const uint8_t* p) { return u16(p)|uint32_t(u16(p+2))<<16; }
static bool ready;
static uint32_t start,fat_start,data_start,clusters,root_cluster,sectors_per_cluster,fat_sectors;
static uint8_t sector[512] __attribute__((aligned(2)));
static bool valid_cluster(uint32_t c) { return c>=2 && c<clusters+2; }
static uint32_t next_cluster(uint32_t c) {
    if(!valid_cluster(c) || c/128>=fat_sectors || !ata::read(fat_start+c/128,sector)) return 0;
    return u32(sector+(c%128)*4)&0x0fffffff;
}
namespace fat32 {
    using vfs::Entry;
    bool mount() {
        ready=false;
        if(!ata::init() || !ata::read(0,sector) || u16(sector+510)!=0xaa55) return false;
        start=0;
        if(u16(sector+11)!=512) {
            bool found=false;
            for(int i=0;i<4;++i) {
                auto* p=sector+446+i*16;
                if(p[4]==0x0b || p[4]==0x0c) {start=u32(p+8);found=true;break;}
            }
            if(!found || !ata::read(start,sector)) return false;
        }
        uint32_t spc=sector[13], reserved=u16(sector+14), fats=sector[16];
        uint32_t total=u32(sector+32), size=u32(sector+36);
        if(u16(sector+510)!=0xaa55 || u16(sector+11)!=512 || !spc || spc>128 || (spc&(spc-1)) ||
           !reserved || !fats || fats>2 || !size || u16(sector+17) || u16(sector+22) ||
           uint64_t(start)+total>ata::sectors() || uint64_t(reserved)+uint64_t(fats)*size>=total) return false;
        sectors_per_cluster=spc; fat_sectors=size;
        fat_start=start+reserved; data_start=fat_start+fats*size;
        clusters=(total-reserved-fats*size)/spc;
        root_cluster=u32(sector+44)&0x0fffffff;
        if(clusters<65525 || clusters>=0x0fffffee || uint64_t(size)*128<clusters+2 || !valid_cluster(root_cluster)) return false;
        ready=true;return true;
    }
    bool mounted() { return ready; }
    bool entry(unsigned index,Entry& result) {
        if(!ready) return false;
        uint32_t c=root_cluster; unsigned seen=0;
        for(unsigned hop=0;hop<128 && valid_cluster(c);++hop) {
            for(uint32_t s=0;s<sectors_per_cluster;++s) {
                if(!ata::read(data_start+(c-2)*sectors_per_cluster+s,sector)) return false;
                for(unsigned off=0;off<512;off+=32) {
                    auto* d=sector+off;
                    if(!d[0]) return false;
                    if(d[0]==0xe5 || (d[11]&0x18) || d[11]==0x0f) continue;
                    if(seen++!=index) continue;
                    unsigned n=0;
                    for(int i=0;i<8 && d[i]!=' ';++i) result.name[n++]=d[i];
                    if(d[8]!=' ') {result.name[n++]='.';for(int i=8;i<11 && d[i]!=' ';++i) result.name[n++]=d[i];}
                    result.name[n]=0;result.size=u32(d+28);result.cluster=(uint32_t(u16(d+20))<<16)|u16(d+26);
                    return true;
                }
            }
            c=next_cluster(c);
        }
        return false;
    }
    bool lookup(const char* name,Entry& result) {
        if(*name=='/') ++name;
        char upper[13]; unsigned n=0;
        while(name[n] && n<12) {char c=name[n];upper[n]=(c>='a'&&c<='z')?c-32:c;++n;}
        if(name[n]) return false;
        upper[n]=0;
        for(unsigned i=0;i<256 && fat32::entry(i,result);++i) {
            unsigned j=0;while(upper[j]&&upper[j]==result.name[j])++j;
            if(!upper[j]&&!result.name[j]) return true;
        }
        return false;
    }
    int read(const char* name,void* destination,size_t capacity) {
        Entry e;
        if(!fat32::lookup(name,e)) return -1;
        if(e.size>capacity) return -2;
        uint32_t c=e.cluster,left=e.size; auto* out=(uint8_t*)destination;
        for(uint32_t hop=0;left && hop<=e.size/512+1;++hop) {
            if(!valid_cluster(c)) return -3;
            for(uint32_t s=0;s<sectors_per_cluster && left;++s) {
                if(!ata::read(data_start+(c-2)*sectors_per_cluster+s,sector)) return -3;
                uint32_t take=left<512?left:512;
                for(uint32_t j=0;j<take;++j) *out++=sector[j];
                left-=take;
            }
            if(left) c=next_cluster(c);
        }
        return left?-3:int(e.size);
    }
}
namespace vfs {
    struct Operations {
        bool (*mount)();
        bool (*entry)(unsigned,Entry&);
        bool (*lookup)(const char*,Entry&);
        int (*read)(const char*,void*,size_t);
    };
    static const Operations fat_ops={fat32::mount,fat32::entry,fat32::lookup,fat32::read};
    static const Operations* root;
    bool mount(){root=fat_ops.mount()?&fat_ops:nullptr;return root;}
    bool mounted(){return root;}
    bool entry(unsigned i,Entry& e){return root&&root->entry(i,e);}
    bool lookup(const char* path,Entry& e){return root&&root->lookup(path,e);}
    int read(const char* path,void* data,size_t capacity){return root?root->read(path,data,capacity):-1;}
    void list() {
        if(!ready) {vga::print("FAT32: no disk mounted\n");return;}
        vga::print("FAT32 / (read-only)\n");Entry e;
        for(unsigned i=0;i<256 && entry(i,e);++i) {
            vga::print(e.name);vga::print("  ");vga::print_dec(e.size);vga::print(" bytes\n");
        }
    }
    void cat(const char* name) {
        char data[4096];int n=read(name,data,sizeof(data));
        if(n<0) {vga::print("cat: missing file or file larger than 4096 bytes\n");return;}
        for(int i=0;i<n;++i) if(data[i]=='\n'||(data[i]>=32&&data[i]<127))vga::putchar(data[i]);
        vga::putchar('\n');
    }
}
