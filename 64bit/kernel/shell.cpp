#include "shell.h"
#include "vga.h"
#include <stddef.h>

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
    vga::print("Commands:\n");
    vga::print("  help       - show this\n");
    vga::print("  clear      - clear screen\n");
    vga::print("  echo X     - print X\n");
    vga::print("  about      - about Ivseznayfews\n");
    vga::print("  reboot     - reboot the machine\n");
    vga::print("  shutdown   - power off (exits QEMU)\n");
    vga::print("  halt       - halt the CPU\n");
}

static void cmd_about() {
    vga::print("Ivseznayfews v1.0 (64-bit)\n");
    vga::print("An experimental operating system in C++\n");
    vga::print("Boot: GRUB/Multiboot   Arch: x86_64\n");
}

static void qemu_shutdown() {
    // QEMU ACPI PM1a_CNT 端口，写 0x2000 触发软关机
    asm volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
}

static void execute() {
    if (line_len == 0) return;

    if (str_eq(line, "help"))       cmd_help();
    else if (str_eq(line, "clear")) vga::clear();
    else if (str_eq(line, "about")) cmd_about();

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
        vga::print("Ivseznayfews Shell v1.0 (64-bit)\n");
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
