bits 64
default rel
global _start
section .text
_start:
    mov eax,4
    mov edi,80
    syscall
    mov eax,1
    mov edi,7
    syscall
section .note.GNU-stack noalloc noexec nowrite progbits
