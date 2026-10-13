#pragma once
#include <stdint.h>
#include <stddef.h>

struct MultibootInfo {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length, drives_addr, config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t framebuffer_bpp, framebuffer_type;
    uint16_t framebuffer_reserved; // GRUB aligns the color-info union to 4 bytes.
    uint8_t red_position, red_size, green_position, green_size, blue_position, blue_size;
} __attribute__((packed));

struct MultibootMmapEntry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

namespace multiboot {
    void     set_info(uint64_t ptr);
    uint32_t total_memory_kb();
    void     print_mmap_summary();
}

static_assert(offsetof(MultibootInfo, framebuffer_addr) == 88, "Multiboot framebuffer layout");
static_assert(offsetof(MultibootInfo, red_position) == 112, "GRUB color union layout");
