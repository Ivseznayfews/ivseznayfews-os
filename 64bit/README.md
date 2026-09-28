# Ivseznayfews

A 64-bit hobby operating system written from scratch in C++ and x86 assembly.

> **Status**: Work in progress. Boots, runs a shell, manages memory, and
> can run multiple tasks. Multitasking has a known stability bug
> (see "Known Issues").

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

## Known issues

- **Multitasking is unstable.** After a few seconds of switching between
  two tasks, a General Protection Fault occurs when iretq attempts to
  return with an invalid CS value. The task stacks appear to be
  corrupted. Suspected cause: IRQ stack sharing between tasks. Fix is
  planned via per-task interrupt stacks / TSS.

## Building

Requires: g++ (x86_64, GCC 13+), nasm, ld, grub-mkrescue, xorriso, qemu-system-x86_64.

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
