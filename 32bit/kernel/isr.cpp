#include "isr.h"
#include "idt.h"
#include "serial.h"
#include "vga.h"
#include "io.h"

static const char* exception_names[] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection", "Page Fault", "Reserved",
    "x87 FP", "Alignment Check", "Machine Check", "SIMD FP",
    "Virtualization", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor", "VMM Comm", "Security", "Reserved"
};

static void exception_handler(Registers* r) {
    vga::print("\n\n*** EXCEPTION: ");
    vga::print(exception_names[r->int_no]);
    vga::print(" (");
    vga::print_hex(r->int_no);
    vga::print(") err=");
    vga::print_hex(r->err_code);
    vga::print(" eip=");
    vga::print_hex(r->eip);
    vga::print("\nSystem halted.\n");

    serial::print("\n[EXCEPTION] ");
    serial::print(exception_names[r->int_no]);
    serial::print(" err=");
    serial::print_hex(r->err_code);
    serial::print(" eip=");
    serial::print_hex(r->eip);
    serial::print("\n");

    while (1) asm volatile("hlt");
}

// 默认 IRQ handler：什么都不做，EOI 由 irq_dispatch 统一发送
static void irq_stub(Registers*) {
}

namespace isr {
    void install_handlers() {
        for (int i = 0; i < 32; i++) idt::set_handler(i, exception_handler);
        for (int i = 32; i < 48; i++) idt::set_handler(i, irq_stub);
    }
    void init() { install_handlers(); }
}

extern "C" {
    void isr0();  void isr1();  void isr2();  void isr3();
    void isr4();  void isr5();  void isr6();  void isr7();
    void isr8();  void isr9();  void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15();
    void isr16(); void isr17(); void isr18(); void isr19();
    void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27();
    void isr28(); void isr29(); void isr30(); void isr31();
    void irq0();  void irq1();  void irq2();  void irq3();
    void irq4();  void irq5();  void irq6();  void irq7();
    void irq8();  void irq9();  void irq10(); void irq11();
    void irq12(); void irq13(); void irq14(); void irq15();
}

void (*isr_table[32])() = {
    isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,isr8,isr9,isr10,
    isr11,isr12,isr13,isr14,isr15,isr16,isr17,isr18,isr19,isr20,
    isr21,isr22,isr23,isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31
};
void (*irq_table[16])() = {
    irq0,irq1,irq2,irq3,irq4,irq5,irq6,irq7,
    irq8,irq9,irq10,irq11,irq12,irq13,irq14,irq15
};
