ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = 0x8000000000;
 .text : { *(.text*) *(.rodata*) } :text
 . = ALIGN(4096);
 .data : { *(.data*) *(.bss*) } :data
 /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
