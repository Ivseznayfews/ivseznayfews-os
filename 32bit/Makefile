CXX      := i686-linux-gnu-g++
LD       := i686-linux-gnu-ld
AS       := nasm
CXXFLAGS := -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -Wall -Wextra -Ikernel
LDFLAGS  := -m elf_i386 -T linker.ld

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
        kernel/shell.o

all: innefews.iso

boot/%.o: boot/%.asm
	$(AS) -f elf32 $< -o $@

kernel/%.o: kernel/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/%.o: kernel/%.asm
	$(AS) -f elf32 $< -o $@

innefews.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

innefews.iso: innefews.elf iso/boot/grub/grub.cfg
	cp innefews.elf iso/boot/
	grub-mkrescue -o $@ iso

run: innefews.iso
	qemu-system-i386 -cdrom innefews.iso -serial stdio

clean:
	rm -f $(OBJS) innefews.elf innefews.iso iso/boot/innefews.elf
