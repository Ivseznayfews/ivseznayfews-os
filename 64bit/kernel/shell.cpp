#include "shell.h"
#include "vga.h"
#include <stddef.h>
#include "heap.h"
#include "pmm.h"
#include "pit.h"
#include "task.h"
#include "keyboard.h"
#include "storage.h"
#include "user.h"
#include "smp.h"
#include "net.h"

static char line[256];
static size_t line_len = 0;

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void print_prompt() {
    vga::print("Ivseznayfews> ");
}

static void clear_line() {
    line_len = 0;
    for (int i = 0; i < 256; i++) line[i] = 0;
}

static void cmd_help() {
    vga::print("Commands: net, ping IP, udp/tcp IP PORT TEXT\n");
    vga::print("  help       - show this\n");
    vga::print("  clear      - clear screen\n");
    vga::print("  echo X     - print X\n");
    vga::print("  about      - about Ivseznayfews\n");
    vga::print("  mem        - physical pages and heap usage\n");
    vga::print("  tasks      - task names and scheduler switches\n");
    vga::print("  uptime     - elapsed seconds and timer ticks\n");
    vga::print("  ls / cat X - inspect the FAT32 disk\n");
    vga::print("  run X      - load an ELF into Ring 3\n");
    vga::print("  cpus / smptest - multicore worker status and test\n");
    vga::print("  usertest   - syscall and isolation checks\n");
    vga::print("  memtest    - allocator integrity check\n");
    vga::print("  reboot     - reboot the machine\n");
    vga::print("  shutdown   - power off (exits QEMU)\n");
    vga::print("  halt       - halt the CPU\n");
}

static void cmd_about() {
    vga::print("Ivseznayfews v2.0 (64-bit)\n");
    vga::print("An experimental operating system in C++\n");
    vga::print("Boot: GRUB/Multiboot   Arch: x86_64\n");
}

static void qemu_shutdown() {
    // QEMU ACPI PM1a_CNT 端口，写 0x2000 触发软关机
    asm volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
}

static void cmd_memtest() {
    const size_t sizes[] = {1, 31, 256, 4096, 8193};
    const size_t before = heap::allocated_bytes();
    bool ok = heap::kmalloc(0) == nullptr && heap::kmalloc(SIZE_MAX) == nullptr;
    // Alternate free order and verify payloads before releasing them.
    for (unsigned round = 0; round < 32 && ok; ++round) {
        uint8_t* buffers[5] = {};
        for (unsigned i = 0; i < 5; ++i) {
            buffers[i] = (uint8_t*)heap::kmalloc(sizes[i]);
            if (!buffers[i] || ((uintptr_t)buffers[i] & 15)) { ok = false; break; }
            for (size_t j = 0; j < sizes[i]; ++j) buffers[i][j] = (uint8_t)(round + i + j);
        }
        for (unsigned i = 0; i < 5; ++i) {
            if (!buffers[i]) continue;
            for (size_t j = 0; j < sizes[i]; ++j)
                if (buffers[i][j] != (uint8_t)(round + i + j)) ok = false;
        }
        const unsigned order[] = {1, 3, 0, 4, 2};
        for (unsigned i : order) heap::kfree(buffers[i]);
        if (heap::allocated_bytes() != before) ok = false;
    }
    vga::print(ok ? "memtest: PASS (32 rounds, payloads and accounting)\n"
                  : "memtest: FAIL (allocation or integrity error)\n");
}

static void execute() {
    if (line_len == 0) return;

    if (str_eq(line, "help"))       cmd_help();
    else if (str_eq(line, "clear")) vga::clear();
    else if (str_eq(line, "about")) cmd_about();
    else if (str_eq(line, "mem")) { pmm::print_stats(); heap::stats(); }
    else if (str_eq(line, "tasks")) task::print_stats();
    else if (str_eq(line, "ls")) vfs::list();
    else if (line[0]=='c' && line[1]=='a' && line[2]=='t' && line[3]==' ') vfs::cat(line+4);
    else if (line[0]=='r' && line[1]=='u' && line[2]=='n' && line[3]==' ') {
        int status=user::run(line+4);
        vga::print("Process exit: ");if(status<0)vga::putchar('-');vga::print_dec(status<0?-int64_t(status):status);vga::putchar('\n');
    }
    else if (str_eq(line, "net")) net::stats();
    else if (line[0]=='p' && line[1]=='i' && line[2]=='n' && line[3]=='g' && line[4]==' ') net::ping(line+5);
    else if (line[0]=='u' && line[1]=='d' && line[2]=='p' && line[3]==' ') net::exchange(line+4,false);
    else if (line[0]=='t' && line[1]=='c' && line[2]=='p' && line[3]==' ') net::exchange(line+4,true);
    else if (str_eq(line, "cpus")) smp::stats();
    else if (str_eq(line, "smptest")) smp::test();
    else if (str_eq(line, "usertest")) user::test();
    else if (str_eq(line, "memtest")) cmd_memtest();
    else if (str_eq(line, "uptime")) {
        uint64_t ticks = pit::ticks();
        vga::print("Uptime: "); vga::print_dec(ticks / 100);
        vga::print(" seconds, ticks="); vga::print_dec(ticks);
        vga::print(", dropped keys="); vga::print_dec(keyboard::dropped_keys());
        vga::putchar('\n');
    }

    else if (str_eq(line, "halt")) {
        vga::print("Halting CPU. Close the QEMU window to exit.\n");
        asm volatile("cli");
        while (1) asm volatile("hlt");
    }

    else if (str_eq(line, "shutdown")) {
        vga::print("Shutting down...\n");
        qemu_shutdown();
        asm volatile("cli");
        while (1) asm volatile("hlt");
    }

    else if (str_eq(line, "reboot")) {
        vga::print("Rebooting...\n");
        struct __attribute__((packed)) { uint16_t limit; uint64_t base; } null_idt = {0, 0};
        asm volatile("lidt %0" : : "m"(null_idt));
        asm volatile("int $0x03");
    }

    else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h'
          && line[3] == 'o' && line[4] == ' ') {
        vga::print(line + 5);
        vga::putchar('\n');
    }

    else {
        vga::print("Unknown command: ");
        vga::print(line);
        vga::putchar('\n');
    }
}

namespace shell {
    void init() {
        vga::print("Ivseznayfews Shell v2.0 (64-bit)\n");
        vga::print("Type 'help' for commands.\n\n");
        print_prompt();
    }

    void input(char c) {
        if (c == '\n') {
            vga::putchar('\n');
            line[line_len] = 0;
            execute();
            clear_line();
            print_prompt();
            return;
        }
        if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                vga::putchar('\b');
            }
            return;
        }
        if (line_len < 255) {
            line[line_len++] = c;
            vga::putchar(c);
        }
    }
}
