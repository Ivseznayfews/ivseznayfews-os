bits 64
default rel
global _start
section .text
_start:
    mov rsp, 0x800000000000
    mov eax, 2
    syscall
    ud2
section .note.GNU-stack noalloc noexec nowrite progbits
