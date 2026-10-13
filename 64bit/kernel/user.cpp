#include "user.h"
#include "storage.h"
#include "pmm.h"
#include "heap.h"
#include "pit.h"
#include "io.h"
#include "vga.h"
#include "serial.h"
static constexpr uint64_t BASE=0x8000000000ull, NX=1ull<<63;
static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd[512] __attribute__((aligned(4096)));
static uint64_t pt[512] __attribute__((aligned(4096)));
static bool running;
static uint64_t deadline;
extern "C" void syscall_entry();
struct Elf {
    uint8_t ident[16];uint16_t type,machine;uint32_t version;
    uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phsize,phnum,shsize,shnum,shindex;
} __attribute__((packed));
struct Segment {uint32_t type,flags;uint64_t offset,va,pa,filesz,memsz,align;} __attribute__((packed));
static bool accessible(uint64_t address,uint64_t size,bool write=false) {
    if(address<BASE || size>0x20000 || address-BASE>0x20000-size) return false;
    if(!size) return true;
    for(uint64_t i=(address-BASE)/4096;i<=(address-BASE+size-1)/4096;++i)
        if(!(pt[i]&1) || (write && !(pt[i]&2))) return false;
    return true;
}
namespace user {
    void init() {
        wrmsr(0xc0000080,rdmsr(0xc0000080)|1|(1<<11));
        wrmsr(0xc0000081,(0x13ull<<48)|(8ull<<32));
        wrmsr(0xc0000082,(uint64_t)syscall_entry);
        wrmsr(0xc0000084,0x44700);
    }
    bool active() {return running;}
    int run(const char* path) {
        if(running) return -1;
        auto* image=(uint8_t*)heap::kmalloc(65536);
        if(!image) return -2;
        int size=vfs::read(path,image,65536);
        if(size<int(sizeof(Elf))) {heap::kfree(image);return -3;}
        auto* e=(Elf*)image;
        if(e->ident[0]!=0x7f||e->ident[1]!='E'||e->ident[2]!='L'||e->ident[3]!='F'||e->ident[4]!=2||e->ident[5]!=1||
           e->type!=2||e->machine!=62||e->phsize!=sizeof(Segment)||e->phnum>16||e->phoff>uint64_t(size)||
           uint64_t(e->phnum)*sizeof(Segment)>uint64_t(size)-e->phoff) {heap::kfree(image);return -4;}
        auto* pages=(uint8_t*)pmm::alloc_pages(32);
        if(!pages) {heap::kfree(image);return -2;}
        for(size_t i=0;i<32*4096;++i) pages[i]=0;
        for(unsigned i=0;i<512;++i) pml4[i]=pdpt[i]=pd[i]=pt[i]=0;
        uint64_t kernel_cr3;asm volatile("mov %%cr3,%0":"=r"(kernel_cr3));
        pml4[0]=((uint64_t*)kernel_cr3)[0]; // Kernel identity mapping remains supervisor-only.
        pml4[1]=(uint64_t)pdpt|7;pdpt[0]=(uint64_t)pd|7;pd[0]=(uint64_t)pt|7;
        bool valid=true, entry_ok=false;
        for(unsigned i=0;i<e->phnum && valid;++i) {
            auto* s=(Segment*)(image+e->phoff+i*sizeof(Segment));
            if(s->type!=1) continue;
            if(!s->memsz) continue;
            if(s->va<BASE || s->memsz>65536 || s->va-BASE>65536-s->memsz || s->filesz>s->memsz ||
               s->offset>uint64_t(size) || s->filesz>uint64_t(size)-s->offset || (s->flags&3)==3 ||
               (s->va&4095)!=(s->offset&4095)) {valid=false;break;}
            for(uint64_t page=(s->va-BASE)/4096;page<=(s->va-BASE+s->memsz-1)/4096;++page) {
                if(pt[page]) {valid=false;break;}
                pt[page]=(uint64_t)(pages+page*4096)|5|((s->flags&2)?2:0)|((s->flags&1)?0:NX);
            }
            for(uint64_t j=0;j<s->filesz;++j) pages[s->va-BASE+j]=image[s->offset+j];
            if((s->flags&1) && e->entry>=s->va && e->entry-s->va<s->memsz) entry_ok=true;
        }
        for(unsigned i=28;i<32;++i) pt[i]=(uint64_t)(pages+i*4096)|7|NX;
        uint64_t entry=e->entry;
        heap::kfree(image);
        if(!valid||!entry_ok) {pmm::free_pages(pages,32);return -4;}
        running=true;deadline=pit::ticks()+300;
        serial::print("[USER] entering Ring 3\n");
        int status=user_enter(entry,BASE+0x20000,(uint64_t)pml4);
        running=false;pmm::free_pages(pages,32);
        serial::print("[USER] returned status=");serial::print_hex(status);serial::print("\n");
        return status;
    }
    void fault(Registers* r) {
        if(!running || (r->cs&3)!=3) return;
        serial::print("[USER] isolated fault vector=");serial::print_hex(r->int_no);serial::print("\n");
        user_leave(128+r->int_no);
    }
    void tick(Registers* r,uint64_t ticks) {
        if(running && (r->cs&3)==3 && ticks>=deadline) {outb(0x20,0x20);user_leave(124);}
    }
    void test() {
        int hello=run("HELLO.ELF"), bad=run("BADPTR.ELF"), fault=run("PROBE.ELF"), timeout=run("SPIN.ELF"), stack=run("BADSTACK.ELF");
        vga::print(hello==0 && bad==0 && fault==142 && timeout==124 && stack==141?
                   "usertest: PASS (Ring 3, syscall, isolation, timeout)\n":"usertest: FAIL\n");
    }
}
extern "C" void syscall_dispatch(Registers* r) {
    if(!user::active()) user_leave(125);
    // IRET faults in ring 0 for noncanonical user state. Reject it before return.
    if(!accessible(r->rip,1) || (pt[(r->rip-BASE)/4096]&NX) ||
       r->rsp<BASE || r->rsp>BASE+0x20000) user_leave(141);
    switch(r->rax) {
    case 0:
        if(r->rsi>4096 || !accessible(r->rdi,r->rsi)) {r->rax=uint64_t(-14);break;}
        for(uint64_t i=0;i<r->rsi;++i) vga::putchar(((const char*)r->rdi)[i]);
        r->rax=r->rsi;break;
    case 1:user_leave(r->rdi&255);
    case 2:r->rax=pit::ticks();break;
    case 3: {
        if(r->rdx>4096 || !accessible(r->rsi,r->rdx,true) || !accessible(r->rdi,13)) {r->rax=uint64_t(-14);break;}
        char path[13];bool terminated=false;
        for(unsigned i=0;i<13;++i) {path[i]=((const char*)r->rdi)[i];if(!path[i]) {terminated=true;break;}}
        r->rax=terminated?vfs::read(path,(void*)r->rsi,r->rdx):uint64_t(-22);break;
    }
    default:r->rax=uint64_t(-38);
    }
}
