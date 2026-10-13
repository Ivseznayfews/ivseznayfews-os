default abs
bits 64
section .text
global _start
_start:
    mov byte [0x100000],0
    mov eax,1
    mov edi,1
    syscall
