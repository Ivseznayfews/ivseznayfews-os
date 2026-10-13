bits 64
section .text
global _start
_start:
    xor eax,eax
    mov edi,0x100000
    mov esi,16
    syscall
    cmp rax,-14
    setne dil
    movzx edi,dil
    mov eax,1
    syscall
