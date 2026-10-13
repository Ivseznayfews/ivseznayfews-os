# Ivseznayfews

A 64-bit hobby operating system written from scratch in C++ and x86 assembly.

> **Status**: Work in progress. Boots, runs a shell, manages memory, and
> runs an interactive shell alongside a scheduler monitor task.

## What works

- **64-bit long mode** — custom page tables, direct jump from GRUB multiboot
- **GDT / IDT** — flat segmentation, full interrupt descriptor table
- **PIC remapping** — IRQ0 (timer) and IRQ1 (keyboard) working
- **Keyboard driver** — scancode to ASCII, US layout, shift support
- **Shell** — help, clear, echo, about, mem, tasks, uptime, memtest, reboot, shutdown, halt
- **Multiboot parsing** — reads the memory map GRUB provides
- **Physical memory manager** — reserved-page protection and contiguous 4KB page runs
- **Kernel heap** — 16-byte alignment, guarded allocation/free, splitting and physical adjacency checks
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

## Memory and shell stage (v1.8)

`mem` reports the actual page pool within the current 1 GB identity mapping
and heap usage; BIOS/kernel/Multiboot info/map/module payloads and partial
pages are excluded.
`tasks` lists the shell and monitor tasks with the scheduler switch count.
`uptime` reports elapsed seconds, PIT ticks, and keyboard queue overflow count.
`memtest` runs 32 allocation/free rounds with payload checks and accounting checks.

The keyboard IRQ only decodes and queues input. The shell task drains a bounded
batch outside interrupt context, so commands can be preempted by the timer.
The queue holds 255 characters; overflow drops the newest input and is counted.

Page allocation searches for an entire contiguous run before changing the bitmap.
Only allocated, aligned pages may be freed. Heap blocks are merged only when
physically adjacent, and invalid/double frees are ignored. Allocation overflow
returns null. The heap retains pages for reuse after freeing objects; `memtest`
can therefore increase PMM allocation while heap object usage returns to baseline.
These protections do not provide allocation-generation tracking: stale pointers
must never be reused after another allocation occupies the same address.

## Regression check

Run `make test-host` for allocator regression checks without QEMU. It compiles
the real PMM/heap sources with privileged IRQ operations stubbed and checks
map validation/overlaps, reserved pages/modules, contiguous allocation,
exhaustion, fragmented heaps, invalid frees, alignment, overflow, and 4000
deterministic allocation/free operations. The host test uses a low-address Linux
mapping for the identity-mapped memory model.

Run `make test` with Python 3 and QEMU installed. The test boots the ISO under
TCG, injects real PS/2 keys, reads VGA memory to check help/echo output,
checks the new status commands, memory self-test and backspace editing,
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
