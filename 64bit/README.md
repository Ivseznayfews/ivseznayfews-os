# Ivseznayfews

A 64-bit hobby operating system written from scratch in C++ and x86 assembly.

> **Status**: Work in progress. Boots, runs a shell, manages memory, and
> runs an interactive shell alongside a scheduler monitor task.

## What works

- **64-bit long mode** — custom page tables, direct jump from GRUB multiboot
- **GDT / IDT** — flat segmentation, full interrupt descriptor table
- **PIC remapping** — IRQ0 (timer) and IRQ1 (keyboard) working
- **Keyboard driver** — scancode to ASCII, US layout, shift support
- **Shell** — help, clear, echo, about, reboot, shutdown, halt
- **Multiboot parsing** — reads the memory map GRUB provides
- **Physical memory manager** — bitmap allocator for 4KB pages
- **Kernel heap** — kmalloc / kfree with block splitting and coalescing
- **PIT timer** — 100 Hz
- **Multitasking** — round-robin scheduler, context switch via iretq

## Scheduler repair

Fresh tasks now contain the complete long-mode IRETQ frame (including SS/RSP)
and enter through an ABI-aligned trampoline. Interrupt stubs align the stack
before C++ calls and clear DF; exception vectors with hardware error codes
use the correct frame layout. Kernel compilation uses general registers only;
floating-point/SIMD task state is not supported yet.

The default boot initializes the shell, with a second idle task reporting the
scheduler switch count once per second over COM1.

## Regression check

Run `make test` with Python 3 and QEMU installed. The test boots the ISO under
TCG, injects real PS/2 keys, reads VGA memory to check help/echo output,
checks 65 seconds of increasing switch counts without exceptions, then uses
the shell shutdown command. `SMOKE_SECONDS` adjusts the observation period;
`QEMU` and `QEMU_EXTRA` can select a custom emulator path/options.

## Building

Requires: g++ (x86_64, GCC 13+), nasm, ld, grub-mkrescue, xorriso, mtools, qemu-system-x86_64.

    make
    make run

## Layout

    boot/boot.asm        Multiboot header, 32 to 64 bit transition
    linker.ld            Kernel memory layout
    kernel/
      kmain.cpp          Entry point
      vga.{h,cpp}        Text mode VGA driver
      serial.{h,cpp}     COM1 serial output
      gdt.{h,cpp}        Global Descriptor Table
      gdt_flush.asm
      idt.{h,cpp}        Interrupt Descriptor Table
      isr.{h,cpp}        Exception and IRQ dispatchers
      isr_stubs.asm      Assembly interrupt entry points
      pic.{h,cpp}        8259 PIC driver
      keyboard.{h,cpp}   PS/2 keyboard
      pit.{h,cpp}        8253 PIT timer
      shell.{h,cpp}      Command line
      multiboot.{h,cpp}  Multiboot info parsing
      pmm.{h,cpp}        Physical memory manager
      heap.{h,cpp}       Kernel heap
      task.{h,cpp}       Task structures and scheduler
      task_switch.asm    First-time task entry

## License

MIT
