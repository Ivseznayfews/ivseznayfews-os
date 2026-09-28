CXX      := g++
LD       := ld
AS       := nasm
CXXFLAGS := -m64 -O0 -fno-omit-frame-pointer -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -Wall -Wextra -Ikernel -mno-red-zone -fno-pic -fno-pie
LDFLAGS  := -m elf_x86_64 -T linker.ld

OBJS := boot/boot.o \
        kernel/kmain.o \
        kernel/vga.o \
        kernel/serial.o \
        kernel/gdt.o \
        kernel/gdt_flush.o \
        kernel/idt.o \
        kernel/isr.o \
        kernel/isr_stubs.o \
        kernel/pic.o \
        kernel/keyboard.o \
        kernel/shell.o \
        kernel/multiboot.o \
        kernel/pmm.o \
        kernel/heap.o \
        kernel/pit.o \
        kernel/task.o \
        kernel/task_switch.o

all: ivseznayfews.iso

boot/%.o: boot/%.asm
	$(AS) -f elf64 $< -o $@

kernel/%.o: kernel/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/%.o: kernel/%.asm
	$(AS) -f elf64 $< -o $@

ivseznayfews.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

ivseznayfews.iso: ivseznayfews.elf iso/boot/grub/grub.cfg
	cp ivseznayfews.elf iso/boot/
	grub-mkrescue -o $@ iso

run: ivseznayfews.iso
	qemu-system-x86_64 -cdrom ivseznayfews.iso -serial stdio

clean:
	rm -f $(OBJS) ivseznayfews.elf ivseznayfews.iso iso/boot/ivseznayfews.elf
