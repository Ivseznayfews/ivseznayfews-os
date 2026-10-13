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

static void shell_task() {
    shell::init();
    serial::print("[READY] Shell initialized\n");
    while (1) {
        keyboard::poll();
        asm volatile("hlt");
    }
}

static void monitor_task() {
    uint64_t last = 0;
    while (1) {
        if (pit::ticks() - last >= 100) {
            last = pit::ticks();
            serial::print("[SCHED] switches=");
            serial::print_hex(task::switches());
            serial::print("\n");
        }
        asm volatile("hlt");
    }
}

extern "C" void kmain(uint32_t magic, uint32_t mb_info) {
    vga::clear();
    serial::init();

    vga::print("=================================\n");
    vga::print("    Ivseznayfews 64-bit v1.8\n");
    vga::print("=================================\n\n");

    if (magic != 0x2BADB002 || !mb_info ||
        !(((MultibootInfo*)(uint64_t)mb_info)->flags & (1u << 6))) {
        serial::print("[BOOT ERROR] Missing Multiboot memory map\n");
        while (1) asm volatile("cli; hlt");
    }
    multiboot::set_info(mb_info);
    gdt::init();
    idt::init();
    isr::init();
    pic::remap();
    keyboard::init();
    keyboard::set_callback(shell::input);

    if (!pmm::init((MultibootInfo*)(uint64_t)mb_info)) {
        serial::print("[BOOT ERROR] Invalid or empty memory map\n");
        while (1) asm volatile("cli; hlt");
    }
    heap::init();
    pit::init(100);

    vga::print("[OK] Core + PIT ready\n");

    task::init();
    task::create(shell_task, "shell");
    task::create(monitor_task, "monitor");
    vga::print("[OK] 2 tasks created\n");

    vga::print("Starting shell and scheduler monitor...\n\n");

    task::start();
    while (1) asm volatile("hlt");
}
