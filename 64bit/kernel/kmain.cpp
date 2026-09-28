#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "keyboard.h"
#include "shell.h"
#include "multiboot.h"
#include "pmm.h"
#include "heap.h"
#include "pit.h"
#include "task.h"

static void task_a() {
    while (1) {
        vga::putchar('A');
        for (volatile int i = 0; i < 500000; i++);
    }
}

static void task_b() {
    while (1) {
        vga::putchar('B');
        for (volatile int i = 0; i < 500000; i++);
    }
}

extern "C" void kmain(uint32_t magic, uint32_t mb_info) {
    vga::clear();
    serial::init();

    vga::print("=================================\n");
    vga::print("    Ivseznayfews 64-bit v1.7\n");
    vga::print("=================================\n\n");

    multiboot::set_info(mb_info);
    gdt::init();
    idt::init();
    isr::init();
    pic::remap();
    keyboard::init();
    keyboard::set_callback(shell::input);

    {
        MultibootInfo* mbi = (MultibootInfo*)(uint64_t)mb_info;
        pmm::init(mbi->mmap_addr, mbi->mmap_length);
    }
    heap::init();
    pit::init(100);

    vga::print("[OK] Core + PIT ready\n");

    task::init();
    task::create(task_a);
    task::create(task_b);
    vga::print("[OK] 2 tasks created\n");

    asm volatile("sti");
    vga::print("[OK] Interrupts enabled\n\n");

    vga::print("Starting multitasking (slow pace)...\n");
    vga::print("Expect ABABAB... one char per second:\n\n");

    task::start();
    while (1) asm volatile("hlt");
}
