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
#include "desktop.h"
#include "mouse.h"
#include "storage.h"
#include "user.h"
#include "smp.h"
#include "net.h"

static void shell_task() {
    if (desktop::active()) vga::clear();
    shell::init();
    desktop::render();
    serial::print("[READY] Shell initialized\n");
    while (1) {
        keyboard::poll();
        net::poll();
        desktop::render();
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
    vga::print("    Ivseznayfews 64-bit v2.1\n");
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
    keyboard::set_callback(desktop::input);

    if (!pmm::init((MultibootInfo*)(uint64_t)mb_info)) {
        serial::print("[BOOT ERROR] Invalid or empty memory map\n");
        while (1) asm volatile("cli; hlt");
    }
    heap::init();
    auto* info = (MultibootInfo*)(uint64_t)mb_info;
    bool force_text = false;
    if ((info->flags & (1u << 2)) && info->cmdline && info->cmdline < pmm::MAPPED_BYTES - 5) {
        const char* args = (const char*)(uint64_t)info->cmdline;
        for (unsigned i=0; i<256 && info->cmdline+i+4<pmm::MAPPED_BYTES && args[i]; ++i)
            if (args[i]=='t' && args[i+1]=='e' && args[i+2]=='x' && args[i+3]=='t') force_text=true;
    }
    if (!force_text && desktop::init(info)) {
        if (mouse::init()) serial::print("[GUI] PS/2 mouse ready\n");
    } else if (!(info->flags & (1u << 12)) || info->framebuffer_type == 2) {
        serial::print("[GUI] text recovery mode\n");
    } else {
        serial::print("[GUI] Unsupported display; choose text recovery in GRUB\n");
    }
    pit::init(100);
    user::init();
    smp::init();
    net::init();
    serial::print(vfs::mount()?"[FS] FAT32 mounted\n":"[FS] no FAT32 disk\n");

    vga::print("[OK] Core + PIT ready\n");

    task::init();
    task::create(shell_task, "shell");
    task::create(monitor_task, "monitor");
    vga::print("[OK] 2 tasks created\n");

    vga::print("Starting shell and scheduler monitor...\n\n");

    task::start();
    while (1) asm volatile("hlt");
}
