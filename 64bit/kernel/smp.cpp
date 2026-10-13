#include "smp.h"
#include "spinlock.h"
#include "critical.h"
#include "io.h"
#include "pit.h"
#include "vga.h"
#include "serial.h"
#include "heap.h"
static constexpr unsigned MAX_CPUS=8;
static uint8_t cpu_ids[MAX_CPUS];
static unsigned discovered, online=1;
static unsigned started[MAX_CPUS];
static uint64_t completed[MAX_CPUS];
static uint8_t stacks[MAX_CPUS][16384] __attribute__((aligned(16)));
static volatile uint32_t* lapic;
struct Job {void(*function)(void*);void* context;};
static Job queue[32];static unsigned head,tail;
static SpinLock queue_lock;
extern "C" uint8_t _binary_boot_ap_bin_start[],_binary_boot_ap_bin_end[];
static bool equal(const uint8_t* p,const char* s,unsigned n) {for(unsigned i=0;i<n;++i)if(p[i]!=uint8_t(s[i]))return false;return true;}
static bool checksum(const uint8_t* p,unsigned n) {uint8_t sum=0;for(unsigned i=0;i<n;++i)sum+=p[i];return sum==0;}
static uint32_t u32(const uint8_t* p) {return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static void add_cpu(uint8_t id) {
    for(unsigned i=0;i<discovered;++i)if(cpu_ids[i]==id)return;
    if(discovered<MAX_CPUS)cpu_ids[discovered++]=id;
}
static void discover_acpi() {
    for(uintptr_t addr=0xe0000;addr<0x100000;addr+=16) {
        auto* rsdp=(const uint8_t*)addr;
        if(!equal(rsdp,"RSD PTR ",8)||!checksum(rsdp,20))continue;
        uint32_t rsdt_addr=u32(rsdp+16);
        if(!rsdt_addr||rsdt_addr>=0x3ffff000)continue;
        auto* rsdt=(const uint8_t*)(uintptr_t)rsdt_addr;uint32_t len=u32(rsdt+4);
        if(!equal(rsdt,"RSDT",4)||len<36||len>4096||!checksum(rsdt,len))continue;
        for(unsigned i=36;i+4<=len;i+=4) {
            uint32_t table=u32(rsdt+i);if(table>=0x3ffff000||!table)continue;
            auto* m=(const uint8_t*)(uintptr_t)table;uint32_t size=u32(m+4);
            if(!equal(m,"APIC",4)||size<44||size>4096||!checksum(m,size))continue;
            for(unsigned j=44;j+2<=size;) {
                uint8_t type=m[j],length=m[j+1];if(length<2||length>size-j)break;
                if(type==0&&length>=8&&(u32(m+j+4)&1))add_cpu(m[j+3]);
                j+=length;
            }
            return;
        }
    }
}
static void delay_ms(unsigned ms) {
    uint16_t count=uint16_t(ms*1193);uint8_t old=inb(0x61);
    outb(0x61,old&~3);outb(0x43,0xb0);outb(0x42,count);outb(0x42,count>>8);outb(0x61,(old&~2)|1);
    for(unsigned i=0;i<2000000 && !(inb(0x61)&0x20);++i)asm volatile("pause");
    outb(0x61,old);
}
static bool ipi(uint8_t id,uint32_t command) {
    for(unsigned i=0;i<1000000;++i)if(!(lapic[0x300/4]&(1<<12))) {
        lapic[0x310/4]=uint32_t(id)<<24;lapic[0x300/4]=command;return true;
    }
    return false;
}
static bool execute(unsigned cpu) {
    Job job;
    {SpinGuard guard(queue_lock);if(head==tail)return false;job=queue[head];head=(head+1)%32;}
    job.function(job.context);__atomic_add_fetch(&completed[cpu],1,__ATOMIC_RELAXED);return true;
}
extern "C" void ap_main(uint64_t index) {
    if(index>=MAX_CPUS)while(1)asm volatile("cli;hlt");
    __atomic_store_n(&started[index],1,__ATOMIC_RELEASE);
    while(1) {if(!execute(index))asm volatile("pause");}
}
namespace smp {
    void init() {
        uint32_t a,b,c,d;asm volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
        if(!(d&(1<<9)))return;
        uint8_t bsp=b>>24;add_cpu(bsp);discover_acpi();
        if(discovered<2)return;
        uint64_t base=rdmsr(0x1b);
        if(base&(1<<10))return; // This first implementation uses xAPIC MMIO.
        wrmsr(0x1b,base|(1<<11));lapic=(volatile uint32_t*)(base&0xfffff000);
        lapic[0xf0/4]|=0x100;
        size_t length=_binary_boot_ap_bin_end-_binary_boot_ap_bin_start;
        if(length>4096)return;
        for(size_t i=0;i<length;++i)((volatile uint8_t*)0x8000)[i]=_binary_boot_ap_bin_start[i];
        uint64_t cr3;asm volatile("mov %%cr3,%0":"=r"(cr3));
        for(unsigned i=1;i<discovered;++i) {
            ((volatile uint64_t*)0x7000)[0]=cr3;
            ((volatile uint64_t*)0x7000)[1]=(uint64_t)(stacks[i]+sizeof(stacks[i]));
            ((volatile uint64_t*)0x7000)[2]=(uint64_t)ap_main;
            ((volatile uint64_t*)0x7000)[3]=i;
            asm volatile("mfence":::"memory");
            if(!ipi(cpu_ids[i],0xc500))break;
            delay_ms(10);
            if(!ipi(cpu_ids[i],0x8500))break;
            delay_ms(1);
            if(!ipi(cpu_ids[i],0x4608))break;
            delay_ms(1);
            if(!__atomic_load_n(&started[i],__ATOMIC_ACQUIRE))ipi(cpu_ids[i],0x4608);
            for(unsigned retry=0;retry<20&&!__atomic_load_n(&started[i],__ATOMIC_ACQUIRE);++retry)delay_ms(10);
            if(!__atomic_load_n(&started[i],__ATOMIC_ACQUIRE))break;
            ++online;
        }
        serial::print("[SMP] online=");serial::print_hex(online);serial::print("\n");
    }
    unsigned count(){return online;}
    bool submit(void(*function)(void*),void* context) {
        if(!function)return false;
        SpinGuard guard(queue_lock);unsigned next=(tail+1)%32;
        if(next==head)return false;
        queue[tail]={function,context};tail=next;return true;
    }
    bool work(){return execute(0);}
    void stats() {
        vga::print("CPUs online: ");vga::print_dec(online);vga::print(" / discovered: ");vga::print_dec(discovered?discovered:1);vga::putchar('\n');
        for(unsigned i=0;i<online;++i) {vga::print("CPU ");vga::print_dec(i);vga::print(" completed jobs: ");vga::print_dec(__atomic_load_n(&completed[i],__ATOMIC_RELAXED));vga::putchar('\n');}
    }
}
static SpinLock counter_lock;
static unsigned test_counter, test_done, test_expected, allocation_errors;
static void test_job(void*) {
    auto* allocation=(uint8_t*)heap::kmalloc(64);
    if(!allocation)__atomic_add_fetch(&allocation_errors,1,__ATOMIC_RELAXED);
    else {for(unsigned i=0;i<64;++i)allocation[i]=uint8_t(i);heap::kfree(allocation);}
    for(unsigned i=0;i<1000;++i){SpinGuard guard(counter_lock);++test_counter;}
    __atomic_add_fetch(&test_done,1,__ATOMIC_RELEASE);
}
namespace smp {
    void test() {
        if(__atomic_load_n(&test_done,__ATOMIC_ACQUIRE)!=test_expected) {vga::print("smptest: previous jobs still running\n");return;}
        test_counter=0;test_done=0;test_expected=0;allocation_errors=0;
        uint64_t end=pit::ticks()+500;
        for(unsigned i=0;i<128;++i) {
            while(!submit(test_job,nullptr)) {work();if(pit::ticks()>end){vga::print("smptest: TIMEOUT\n");return;}}
            ++test_expected;
        }
        while(__atomic_load_n(&test_done,__ATOMIC_ACQUIRE)<128 && pit::ticks()<end) {work();asm volatile("pause");}
        unsigned done=__atomic_load_n(&test_done,__ATOMIC_ACQUIRE);
        unsigned counter;{SpinGuard guard(counter_lock);counter=test_counter;}
        vga::print(done==128 && counter==128000 && !__atomic_load_n(&allocation_errors,__ATOMIC_RELAXED)?"smptest: PASS (128 jobs, 128000 locked increments)\n":"smptest: FAIL\n");stats();
    }
}
