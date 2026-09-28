#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "keyboard.h"
#include "shell.h"

extern "C" void kmain(uint32_t magic, uint32_t mb_info) {
    vga::clear();
    serial::init();

    vga::print("=================================\n");
    vga::print("       Ivseznayfews v0.5\n");
    vga::print("=================================\n\n");

    serial::print("[Ivseznayfews] boot, magic=");
    serial::print_hex(magic);
    serial::print("\n");

    gdt::init();
    vga::print("[OK] GDT loaded\n");

    idt::init();
    vga::print("[OK] IDT loaded\n");

    isr::init();
    pic::remap();
    vga::print("[OK] PIC remapped\n");

    keyboard::init();
    keyboard::set_callback(shell::input);
    vga::print("[OK] Keyboard initialized\n");

    asm volatile("sti");
    vga::print("[OK] Interrupts enabled\n\n");

    shell::init();

    while (1) asm volatile("hlt");
}
