bits 64
default rel
global _start
section .text
_start:
    mov eax,6
    lea rdi,[path]
    lea rsi,[message]
    mov edx,end-message
    syscall
    cmp rax,end-message
    jne .bad
    mov eax,1
    xor edi,edi
    syscall
.bad:
    mov eax,1
    mov edi,1
    syscall
path: db 'USER.TXT',0
message: db 'Ring 3 saved this file',10
end:
section .note.GNU-stack noalloc noexec nowrite progbits
