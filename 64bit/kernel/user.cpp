#include "user.h"
#include "storage.h"
#include "pmm.h"
#include "heap.h"
#include "pit.h"
#include "io.h"
#include "vga.h"
#include "serial.h"
#include "task.h"
#include "gdt.h"
#include "critical.h"
static constexpr uint64_t BASE=0x8000000000ull, NX=1ull<<63;
enum class State {Free, Loading, Ready, Running, Sleeping, Zombie};
struct alignas(4096) Process {
    uint64_t saved_rsp,kernel_cr3,syscall_top;
    uint64_t pml4[512] __attribute__((aligned(4096)));
    uint64_t pdpt[512] __attribute__((aligned(4096)));
    uint64_t pd[512] __attribute__((aligned(4096)));
    uint64_t pt[512] __attribute__((aligned(4096)));
    uint8_t irq_stack[32768] __attribute__((aligned(16)));
    uint8_t syscall_stack[32768] __attribute__((aligned(16)));
    uint8_t* pages;
    uint64_t entry, deadline, wake;
    int pid,task_id,status;
    State state;
    bool entered,killed;
    char name[13];
};
static Process processes[6];
static Process* current;
static int next_pid=1;
static uint64_t base_cr3;
extern "C" { void* user_context; uint64_t user_syscall_top; }
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
        if(!current || !(current->pt[i]&1) || (write && !(current->pt[i]&2))) return false;
    return true;
}
namespace user {
    void init() {
        asm volatile("mov %%cr3,%0":"=r"(base_cr3));
        wrmsr(0xc0000080,rdmsr(0xc0000080)|1|(1<<11));
        wrmsr(0xc0000081,(0x13ull<<48)|(8ull<<32));
        wrmsr(0xc0000082,(uint64_t)syscall_entry);
        wrmsr(0xc0000084,0x44700);
    }
    bool active() {return current && current->entered;}
    static Process* find(int pid){for(auto& p:processes)if(p.state!=State::Free&&p.pid==pid)return &p;return nullptr;}
    static Process* for_task(int id){for(auto& p:processes)if(p.state!=State::Free&&p.state!=State::Loading&&p.state!=State::Zombie&&p.task_id==id)return &p;return nullptr;}
    void activate(int id){
        current=for_task(id);user_context=current;
        user_syscall_top=current?current->syscall_top:0;
        gdt::stack(current?(uint64_t)(current->irq_stack+sizeof(current->irq_stack)):0);
        uint64_t cr3=current&&current->entered?(uint64_t)current->pml4:base_cr3;
        asm volatile("mov %0,%%cr3"::"r"(cr3):"memory");
    }
    bool runnable(int id){
        auto* p=for_task(id);
        if(p&&p->state==State::Sleeping){
            if(!p->killed&&pit::ticks()<p->wake)return false;
            p->state=State::Ready;
        }
        return true;
    }
    static void process_entry(){
        Process* p=current;
        int status=143;
        if(!p->killed){asm volatile("cli":::"memory");p->entered=true;p->state=State::Running;
            serial::print("[USER] entering Ring 3\n");
            status=user_enter(p->entry,BASE+0x20000,(uint64_t)p->pml4);
        }
        asm volatile("cli":::"memory");
        p->entered=false;p->status=status;
        pmm::free_pages(p->pages,32);p->pages=nullptr;p->state=State::Zombie;
        serial::print("[USER] returned status=");serial::print_hex(status);serial::print("\n");
        task::finish();
    }
    int spawn(const char* path) {
        Process* process=nullptr;
        {InterruptGuard guard;for(auto& p:processes)if(p.state==State::Free){process=&p;p.state=State::Loading;break;}}
        if(!process)return -1;
        struct Reservation {Process* p;~Reservation(){if(p->state==State::Loading)p->state=State::Free;}} reservation{process};
        auto& pml4=process->pml4;auto& pdpt=process->pdpt;auto& pd=process->pd;auto& pt=process->pt;
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

        pml4[0]=((uint64_t*)base_cr3)[0]; // Kernel identity mapping remains supervisor-only.
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
        InterruptGuard guard;
        process->task_id=-1;process->pid=next_pid++;process->entry=entry;process->pages=pages;
        process->deadline=pit::ticks()+300;process->wake=0;process->killed=false;process->entered=false;
        process->syscall_top=(uint64_t)(process->syscall_stack+sizeof(process->syscall_stack));
        unsigned j=0;while(path[j]&&j<12){process->name[j]=path[j];++j;}process->name[j]=0;
        process->state=State::Ready;
        int id=task::create(process_entry,process->name);
        if(id<0){process->state=State::Free;pmm::free_pages(pages,32);return -1;}
        process->task_id=id;return process->pid;
    }
    int wait(int pid){
        Process* p=find(pid);if(!p||p==current)return -1;
        while(p->state!=State::Zombie)asm volatile("hlt");
        InterruptGuard guard;int result=p->status;p->state=State::Free;p->task_id=-1;return result;
    }
    int run(const char* path){int pid=spawn(path);return pid<0?pid:wait(pid);}
    bool kill(int pid){InterruptGuard guard;auto* p=find(pid);if(!p||p->state==State::Zombie)return false;p->killed=true;return true;}
    void stats(){
        vga::print("PID  STATE     PROGRAM\n");
        for(auto& p:processes)if(p.state!=State::Free){
            vga::print_dec(p.pid);vga::print("  ");
            vga::print(p.state==State::Zombie?"exited":p.state==State::Sleeping?"sleeping":p.task_id==task::current()?"running":"ready");
            vga::print("  ");vga::print(p.name);vga::putchar('\n');
        }
    }
    void process_test(){
        void* warm=heap::kmalloc(65536);if(!warm){vga::print("proctest: FAIL allocation\n");return;}heap::kfree(warm);
        auto before=pmm::used_pages();
        int a=spawn("WORKER.ELF"),b=spawn("PROBE.ELF"),c=spawn("SPIN.ELF");
        int d=spawn("WORKER.ELF"),e=spawn("WORKER.ELF"),f=spawn("WORKER.ELF");
        bool ok=a>0&&b>0&&c>0&&d>0&&e>0&&f>0&&spawn("WORKER.ELF")==-1;
        if(c>0)ok=kill(c)&&ok;
        if(b>0)ok=(wait(b)==142)&&ok;
        if(c>0)ok=(wait(c)==143)&&ok;
        if(a>0)ok=(wait(a)==7)&&ok;
        if(d>0)ok=(wait(d)==7)&&ok;
        if(e>0)ok=(wait(e)==7)&&ok;
        if(f>0)ok=(wait(f)==7)&&ok;
        for(unsigned i=0;i<12&&ok;++i)ok=run("BADPTR.ELF")==0;
        ok=ok&&pmm::used_pages()==before;
        vga::print(ok?"proctest: PASS (concurrent, sleep, fault, kill, reap)\n":"proctest: FAIL\n");
    }
    void fault(Registers* r) {
        if(!active() || (r->cs&3)!=3) return;
        serial::print("[USER] isolated fault vector=");serial::print_hex(r->int_no);serial::print("\n");
        user_leave(128+r->int_no);
    }
    void tick(Registers* r,uint64_t ticks) {
        if(active() && (r->cs&3)==3 && (current->killed || ticks>=current->deadline)) {outb(0x20,0x20);user_leave(current->killed?143:124);}
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
    if(!accessible(r->rip,1) || (current->pt[(r->rip-BASE)/4096]&NX) ||
       r->rsp<BASE || r->rsp>BASE+0x20000) user_leave(141);
    switch(r->rax) {
    case 0:
        if(r->rsi>4096 || !accessible(r->rdi,r->rsi)) {r->rax=uint64_t(-14);break;}
        for(uint64_t i=0;i<r->rsi;++i) vga::putchar(((const char*)r->rdi)[i]);
        r->rax=r->rsi;break;
    case 1:user_leave(r->rdi&255);
    case 2:r->rax=pit::ticks();break;
    case 3: {
        if(r->rdx>4096 || !accessible(r->rsi,r->rdx,true) || !accessible(r->rdi,1)) {r->rax=uint64_t(-14);break;}
        char path[64];bool terminated=false;
        for(unsigned i=0;i<64;++i) {if(!accessible(r->rdi+i,1))break;path[i]=((const char*)r->rdi)[i];if(!path[i]) {terminated=true;break;}}
        r->rax=terminated?vfs::read(path,(void*)r->rsi,r->rdx):uint64_t(-22);break;
    }
    case 4: {
        if(r->rdi>300){r->rax=uint64_t(-22);break;}
        current->wake=pit::ticks()+r->rdi;current->state=State::Sleeping;
        asm volatile("sti":::"memory");
        while(pit::ticks()<current->wake&&!current->killed)asm volatile("hlt");
        asm volatile("cli":::"memory");current->state=State::Running;
        if(current->killed)user_leave(143);
        r->rax=0;break;
    }
    case 5:r->rax=current->pid;break;
    case 6: {
        if(r->rdx>4096||!accessible(r->rsi,r->rdx)){r->rax=uint64_t(-14);break;}
        char path[64];bool terminated=false;
        for(unsigned i=0;i<64;++i){if(!accessible(r->rdi+i,1))break;path[i]=((const char*)r->rdi)[i];if(!path[i]){terminated=true;break;}}
        r->rax=terminated?vfs::write(path,(const void*)r->rsi,r->rdx):uint64_t(-14);break;
    }
    default:r->rax=uint64_t(-38);
    }
}
