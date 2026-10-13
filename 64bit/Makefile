CXX      := g++
LD       := ld
AS       := nasm
CXXFLAGS := -m64 -O0 -fno-omit-frame-pointer -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -Wall -Wextra -Ikernel -mno-red-zone -fno-pic -fno-pie -mgeneral-regs-only -MMD -MP
LDFLAGS  := -z noexecstack -m elf_x86_64 -T linker.ld

OBJS := boot/boot.o \
        kernel/kmain.o \
        kernel/vga.o \
        kernel/framebuffer.o \
        kernel/desktop.o \
        kernel/mouse.o \
        kernel/storage.o \
        kernel/net.o \
        kernel/smp.o \
        boot/ap_blob.o \
        kernel/user.o \
        kernel/user_entry.o \
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

all: ivseznayfews.iso user-programs

boot/%.o: boot/%.asm
	$(AS) -f elf64 $< -o $@

# Pixel loops are optimized while preserving the kernel ABI flags.
kernel/framebuffer.o kernel/desktop.o: CXXFLAGS += -O2

kernel/%.o: kernel/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/%.o: kernel/%.asm
	$(AS) -f elf64 $< -o $@

ivseznayfews.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

ivseznayfews.iso: ivseznayfews.elf iso/boot/grub/grub.cfg
	cp ivseznayfews.elf iso/boot/
	grub-mkrescue -o $@ iso

run: ivseznayfews.iso disk
	./run-vm.sh

clean:
	rm -f $(OBJS:.o=.d) $(OBJS) ivseznayfews.elf ivseznayfews.iso iso/boot/ivseznayfews.elf

.PHONY: all run clean test test-host test-graphics
test-host:
	@tmp_binary=$$(mktemp /tmp/os-allocators.XXXXXX); \
	trap 'rm -f "$$tmp_binary"' EXIT; \
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -DOS_HOST_TEST -fno-pie -no-pie -Ikernel \
	    tests/allocators.cpp kernel/pmm.cpp kernel/heap.cpp -o "$$tmp_binary" && "$$tmp_binary"

test-graphics:
	@tmp_binary=$$(mktemp /tmp/os-graphics.XXXXXX); \
	trap 'rm -f "$$tmp_binary"' EXIT; \
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -Ikernel \
	    tests/graphics.cpp kernel/framebuffer.cpp -o "$$tmp_binary" && "$$tmp_binary"

test: test-host test-graphics ivseznayfews.iso
	python3 tests/smoke.py

-include $(OBJS:.o=.d)

USER_PROGRAMS := user/hello.elf user/badptr.elf user/probe.elf user/spin.elf user/badstack.elf
user/%.o: user/%.asm
	$(AS) -f elf64 $< -o $@
user/%.elf: user/%.o user/linker.ld
	$(LD) -m elf_x86_64 -T user/linker.ld $< -o $@
.PHONY: user-programs disk
user-programs: $(USER_PROGRAMS)
disk: user-programs
	@test -e .vm/disk-v2.img || python3 tools/make_disk.py .vm/disk-v2.img

boot/ap.bin: boot/ap.asm
	$(AS) -f bin $< -o $@
boot/ap_blob.o: boot/ap.bin
	$(LD) -r -b binary $< -o $@
