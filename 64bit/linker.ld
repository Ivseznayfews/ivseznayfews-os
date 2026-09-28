ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text : {
        *(.multiboot)
        *(.text)
    }

    .rodata : { *(.rodata) }
    .data   : { *(.data) }

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        __bss_end = .;
    }

    . = ALIGN(4096);
    kernel_end = .;
}
