bits 64
default rel
global _start
section .text
_start:
    mov eax,5
    syscall
    mov r12,rax
    mov [value],rax
    mov r13d,3
.again:
    mov eax,4
    mov edi,20
    syscall
    cmp [value],r12
    jne .bad
    dec r13d
    jnz .again
    mov eax,1
    mov edi,7
    syscall
.bad:
    mov eax,1
    mov edi,99
    syscall
section .bss
value: resq 1
section .note.GNU-stack noalloc noexec nowrite progbits
