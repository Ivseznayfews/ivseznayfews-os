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
    bool write(uint32_t lba,const void* data) {
        if(!disk_sectors||lba>=disk_sectors||lba>=0x10000000||!wait_disk(false))return false;
        outb(0x1f6,0xe0|(lba>>24));for(int i=0;i<4;++i)inb(0x3f6);
        outb(0x1f2,1);outb(0x1f3,lba);outb(0x1f4,lba>>8);outb(0x1f5,lba>>16);outb(0x1f7,0x30);
        if(!wait_disk(true))return false;
        auto* p=(const uint8_t*)data;
        for(unsigned i=0;i<512;i+=2)outw(0x1f0,uint16_t(p[i])|(uint16_t(p[i+1])<<8));
        return wait_disk(false);
    }
    bool flush(){if(!wait_disk(false))return false;outb(0x1f7,0xe7);return wait_disk(false);}
    uint32_t sectors() { return disk_sectors; }
}
#include "critical.h"
static uint16_t u16(const uint8_t* p){return p[0]|uint16_t(p[1])<<8;}
static uint32_t u32(const uint8_t* p){return u16(p)|uint32_t(u16(p+2))<<16;}
static void put16(uint8_t* p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t* p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
static bool ready;
static uint32_t start,fat_start,data_start,clusters,root_cluster,spc,fat_sectors,fat_count;
static uint32_t fsinfo_lba,backup_fsinfo;
static uint32_t free_clusters;
static uint8_t sector[512] __attribute__((aligned(2)));
static bool valid(uint32_t c){return c>=2&&c<clusters+2;}
static uint32_t data_lba(uint32_t c){return data_start+(c-2)*spc;}
static uint32_t next(uint32_t c){if(!valid(c)||!ata::read(fat_start+c/128,sector))return 0;return u32(sector+(c%128)*4)&0xfffffff;}
static bool set_fat(uint32_t c,uint32_t value){
    if(!valid(c)||!ata::read(fat_start+c/128,sector))return false;
    uint32_t previous=u32(sector+(c%128)*4)&0xfffffff;
    for(uint32_t f=0;f<fat_count;++f){
        uint32_t lba=fat_start+f*fat_sectors+c/128;
        if(!ata::read(lba,sector))return false;
        auto* p=sector+(c%128)*4;put32(p,(u32(p)&0xf0000000)|(value&0xfffffff));
        if(!ata::write(lba,sector))return false;
    }
    if(!previous&&value)--free_clusters;
    else if(previous&&!value)++free_clusters;
    uint32_t locations[2]={fsinfo_lba,backup_fsinfo};
    for(uint32_t lba:locations){
        if(!lba)continue;
        if(!ata::read(lba,sector))return false;
        if(u32(sector)==0x41615252&&u32(sector+484)==0x61417272){
            put32(sector+488,free_clusters);put32(sector+492,0xffffffff);
            if(!ata::write(lba,sector))return false;
        }
    }
    return true;
}
static uint32_t allocate(){
    for(uint32_t row=0;row<fat_sectors;++row){
        if(!ata::read(fat_start+row,sector))return 0;
        for(unsigned j=0;j<128;++j){uint32_t c=row*128+j;if(!valid(c))continue;
            if(!(u32(sector+j*4)&0xfffffff)){
                if(!set_fat(c,0xfffffff))return 0;
                uint8_t zeros[512]={};
                for(uint32_t s=0;s<spc;++s)if(!ata::write(data_lba(c)+s,zeros)){set_fat(c,0);return 0;}
                return c;
            }
        }
    }
    return 0;
}
static bool release(uint32_t c){
    for(uint32_t budget=0;valid(c)&&budget<clusters;++budget){uint32_t n=next(c);if(!set_fat(c,0))return false;c=n;}
    return !valid(c);
}
struct Location {uint32_t lba;unsigned offset;vfs::Entry entry;};
static bool short_name(const char* name,unsigned n,uint8_t* out){
    for(unsigned i=0;i<11;++i)out[i]=' ';
    if(!n||n>12)return false;
    unsigned base=0,ext=0;bool dot=false;
    for(unsigned i=0;i<n;++i){char c=name[i];if(c=='.'){if(dot||!base||i+1==n)return false;dot=true;continue;}
        if(c>='a'&&c<='z')c-=32;
        if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
        if(dot){if(ext==3)return false;out[8+ext++]=c;}else {if(base==8)return false;out[base++]=c;}
    }
    return base!=0;
}
static void decode(const uint8_t* d,vfs::Entry& e){
    unsigned n=0;for(unsigned i=0;i<8&&d[i]!=' ';++i)e.name[n++]=d[i];
    if(d[8]!=' '){e.name[n++]='.';for(unsigned i=8;i<11&&d[i]!=' ';++i)e.name[n++]=d[i];}e.name[n]=0;
    e.size=u32(d+28);e.cluster=(uint32_t(u16(d+20))<<16)|u16(d+26);e.directory=d[11]&0x10;
}
static bool scan(uint32_t directory,const uint8_t* name,unsigned index,Location& result,bool free_slot=false){
    unsigned seen=0;uint32_t last=0;
    for(unsigned hop=0;hop<128&&valid(directory);++hop){
        last=directory;
        for(uint32_t s=0;s<spc;++s){uint32_t lba=data_lba(directory)+s;if(!ata::read(lba,sector))return false;
            for(unsigned off=0;off<512;off+=32){auto* d=sector+off;
                if(!d[0]||d[0]==0xe5){if(free_slot){result.lba=lba;result.offset=off;return true;}if(!d[0])return false;continue;}
                if(free_slot||(d[11]&8)||d[11]==0xf||d[0]=='.')continue;
                bool match=false;
                if(name){match=true;for(unsigned j=0;j<11;++j)if(d[j]!=name[j])match=false;}
                else match=seen++==index;
                if(match){result.lba=lba;result.offset=off;decode(d,result.entry);return true;}
            }
        }
        uint32_t n=next(directory);if(n>=0xffffff8)break;directory=n;
    }
    if(free_slot&&valid(last)){
        uint32_t c=allocate();if(!c)return false;
        if(!set_fat(last,c)){release(c);return false;}
        result.lba=data_lba(c);result.offset=0;return true;
    }
    return false;
}
static bool parent(const char* path,uint32_t& directory,uint8_t* final){
    directory=root_cluster;unsigned components=0;
    while(*path=='/')++path;
    while(*path){const char* part=path;unsigned n=0;while(path[n]&&path[n]!='/')++n;
        uint8_t name[11];if(++components>8||!short_name(part,n,name))return false;
        path+=n;if(!*path){for(unsigned i=0;i<11;++i)final[i]=name[i];return true;}
        while(*path=='/')++path;
        if(!*path)return false;
        Location loc;if(!scan(directory,name,0,loc)||!loc.entry.directory||!valid(loc.entry.cluster))return false;directory=loc.entry.cluster;
    }
    return false;
}
static bool find(const char* path,Location& location){uint32_t dir;uint8_t name[11];return ready&&parent(path,dir,name)&&scan(dir,name,0,location);}
static bool publish(const Location& loc,const uint8_t* name,uint32_t cluster,uint32_t size,bool directory){
    if(!ata::read(loc.lba,sector))return false;
    auto* d=sector+loc.offset;
    for(unsigned i=0;i<32;++i)d[i]=0;
    for(unsigned i=0;i<11;++i)d[i]=name[i];
    d[11]=directory?0x10:0x20;
    put16(d+20,cluster>>16);put16(d+26,cluster);put32(d+28,size);return ata::write(loc.lba,sector)&&ata::flush();
}
namespace fat32 {
    bool mount(){
        ready=false;if(!ata::init()||!ata::read(0,sector)||u16(sector+510)!=0xaa55)return false;
        start=0;uint32_t partition_size=ata::sectors();
        if(u16(sector+11)!=512){bool found=false;
            for(unsigned i=0;i<4;++i){auto* p=sector+446+i*16;if(p[4]==0xb||p[4]==0xc){start=u32(p+8);partition_size=u32(p+12);found=true;break;}}
            if(!found||!ata::read(start,sector))return false;
        }
        uint32_t reserved=u16(sector+14),total=u32(sector+32),size=u32(sector+36);
        spc=sector[13];fat_count=sector[16];
        if(u16(sector+510)!=0xaa55||u16(sector+11)!=512||!spc||spc>128||(spc&(spc-1))||!reserved||!fat_count||fat_count>2||!size||
           u16(sector+17)||u16(sector+22)||u16(sector+40)&0x80||u16(sector+42)||total>partition_size||uint64_t(start)+total>ata::sectors()||
           uint64_t(reserved)+uint64_t(fat_count)*size>=total)return false;
        fat_sectors=size;fat_start=start+reserved;data_start=fat_start+fat_count*size;
        clusters=(total-reserved-fat_count*size)/spc;root_cluster=u32(sector+44)&0xfffffff;
        if(clusters<65525||clusters>=0xfffffee||uint64_t(size)*128<clusters+2||!valid(root_cluster))return false;
        uint32_t fsi=u16(sector+48),backup=u16(sector+50);
        fsinfo_lba=fsi&&fsi<reserved?start+fsi:0;
        backup_fsinfo=fsinfo_lba&&backup&&backup+fsi<reserved?start+backup+fsi:0;
        free_clusters=0;
        for(uint32_t row=0;row<fat_sectors;++row){
            if(!ata::read(fat_start+row,sector))return false;
            for(unsigned j=0;j<128;++j)if(valid(row*128+j)&&!(u32(sector+j*4)&0xfffffff))++free_clusters;
        }
        ready=true;return true;
    }
    bool entry_at(const char* path,unsigned index,vfs::Entry& result){
        uint32_t dir=root_cluster;
        if(path[0]&&!(path[0]=='/'&&!path[1])){Location loc;if(!find(path,loc)||!loc.entry.directory)return false;dir=loc.entry.cluster;}
        Location loc;if(!ready||!scan(dir,nullptr,index,loc))return false;result=loc.entry;return true;
    }
    bool lookup(const char* path,vfs::Entry& result){Location loc;if(!find(path,loc))return false;result=loc.entry;return true;}
    int read(const char* path,void* destination,size_t capacity){
        Location loc;if(!find(path,loc)||loc.entry.directory)return -1;auto& e=loc.entry;if(e.size>capacity)return -2;
        uint32_t c=e.cluster,left=e.size;auto* out=(uint8_t*)destination;
        for(uint32_t budget=0;left&&budget<=e.size/512+1;++budget){if(!valid(c))return -3;
            for(uint32_t s=0;s<spc&&left;++s){if(!ata::read(data_lba(c)+s,sector))return -3;
                uint32_t n=left<512?left:512;for(uint32_t j=0;j<n;++j)*out++=sector[j];left-=n;}
            if(left)c=next(c);
        }
        return left?-3:int(e.size);
    }
    int write(const char* path,const void* data,size_t size){
        uint32_t dir;uint8_t name[11];if(!ready||size>65536||!parent(path,dir,name))return -1;
        Location loc;uint32_t old=0;bool exists=scan(dir,name,0,loc);
        if(exists){if(loc.entry.directory)return -1;old=loc.entry.cluster;}else if(!scan(dir,nullptr,0,loc,true))return -1;
        uint32_t first=0,last=0;size_t left=size;auto* bytes=(const uint8_t*)data;bool ok=true;
        while(left&&ok){uint32_t c=allocate();if(!c){ok=false;break;}if(!first)first=c;
            if(last&&!set_fat(last,c)){release(c);ok=false;break;}last=c;
            for(uint32_t s=0;s<spc&&left;++s){uint32_t n=left<512?left:512;
                for(unsigned j=0;j<512;++j)sector[j]=j<n?*bytes++:0;
                if(!ata::write(data_lba(c)+s,sector)){ok=false;break;}left-=n;
            }
        }
        if(!ok||!ata::flush()||!publish(loc,name,first,size,false)){release(first);return -3;}
        if(old&&!release(old))return -3;
        return ata::flush()?int(size):-3;
    }
    bool mkdir(const char* path){
        uint32_t dir;uint8_t name[11];Location loc;
        if(!ready||!parent(path,dir,name)||scan(dir,name,0,loc)||!scan(dir,nullptr,0,loc,true))return false;
        uint32_t c=allocate();if(!c)return false;
        uint8_t dots[512]={};for(unsigned i=0;i<11;++i)dots[i]=dots[32+i]=' ';
        dots[0]='.';dots[32]=dots[33]='.';dots[11]=dots[43]=0x10;
        put16(dots+20,c>>16);put16(dots+26,c);uint32_t parent_cluster=dir==root_cluster?0:dir;
        put16(dots+52,parent_cluster>>16);put16(dots+58,parent_cluster);
        if(!ata::write(data_lba(c),dots)||!publish(loc,name,c,0,true)){release(c);return false;}return true;
    }
}
namespace vfs {
    // BSP filesystem operations must not be interleaved by another process.
    struct Operations {
        bool(*mount)();bool(*entry)(const char*,unsigned,Entry&);bool(*lookup)(const char*,Entry&);
        int(*read)(const char*,void*,size_t);int(*write)(const char*,const void*,size_t);bool(*mkdir)(const char*);
    };
    static const Operations fat_ops={fat32::mount,fat32::entry_at,fat32::lookup,fat32::read,fat32::write,fat32::mkdir};
    static const Operations* root;
    bool mount(){InterruptGuard guard;root=fat_ops.mount()?&fat_ops:nullptr;return root;}
    bool mounted(){return root;}
    bool entry_at(const char* path,unsigned i,Entry& e){InterruptGuard guard;return root&&root->entry(path,i,e);}
    bool entry(unsigned i,Entry& e){return entry_at("/",i,e);}
    bool lookup(const char* path,Entry& e){InterruptGuard guard;return root&&root->lookup(path,e);}
    int read(const char* path,void* data,size_t capacity){InterruptGuard guard;return root?root->read(path,data,capacity):-1;}
    int write(const char* path,const void* data,size_t size){InterruptGuard guard;return root?root->write(path,data,size):-1;}
    bool mkdir(const char* path){InterruptGuard guard;return root&&root->mkdir(path);}
    void list(const char* path){
        if(!root){vga::print("FAT32: no disk mounted\n");return;}vga::print("FAT32 ");vga::print(path);vga::putchar('\n');Entry e;
        for(unsigned i=0;i<256&&entry_at(path,i,e);++i){vga::print(e.name);vga::print(e.directory?" /\n":"  ");if(!e.directory){vga::print_dec(e.size);vga::print(" bytes\n");}}
    }
    void cat(const char* path){char data[4096];int n=read(path,data,sizeof(data));if(n<0){vga::print("cat: missing file or larger than 4096 bytes\n");return;}
        for(int i=0;i<n;++i)if(data[i]=='\n'||(data[i]>=32&&data[i]<127))vga::putchar(data[i]);
        vga::putchar('\n');}
    void test(){
        Entry e;bool ok=lookup("WORK",e)?e.directory:mkdir("WORK");
        char data[1537],copy[1537]={};for(unsigned i=0;i<sizeof(data);++i)data[i]='A'+i%26;
        ok=ok&&write("WORK/TEST.TXT",data,sizeof(data))==int(sizeof(data))&&read("WORK/TEST.TXT",copy,sizeof(copy))==int(sizeof(copy));
        for(unsigned i=0;i<sizeof(data);++i)if(data[i]!=copy[i])ok=false;
        ok=ok&&write("WORK/TEST.TXT","short",5)==5&&read("WORK/TEST.TXT",copy,sizeof(copy))==5;
        for(unsigned i=0;i<5;++i)if(copy[i]!="short"[i])ok=false;
        ok=ok&&write("WORK/EMPTY.TXT",nullptr,0)==0&&read("WORK/EMPTY.TXT",copy,sizeof(copy))==0;
        ok=ok&&write("../BAD.TXT",data,1)<0;
        for(unsigned i=0;i<20&&ok;++i){char path[]="WORK/F00.TXT";path[6]='0'+i/10;path[7]='0'+i%10;ok=write(path,nullptr,0)==0;}
        ok=ok&&(lookup("WORK/SUB",e)?e.directory:mkdir("WORK/SUB"));
        ok=ok&&write("WORK/SUB/NEST.TXT","nested",6)==6&&read("WORK/SUB/NEST.TXT",copy,sizeof(copy))==6;
        for(unsigned i=0;i<6;++i)if(copy[i]!="nested"[i])ok=false;
        vga::print(ok?"fstest: PASS (mkdir, multicluster, overwrite, empty)\n":"fstest: FAIL\n");
    }
}
