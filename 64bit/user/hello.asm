bits 64
section .text
global _start
_start:
    xor eax,eax
    mov rdi,message
    mov esi,message_end-message
    syscall
    mov eax,3
    mov rdi,path
    mov rsi,buffer
    mov edx,512
    syscall
    test rax,rax
    js .fail
    mov rsi,rax
    mov rdi,buffer
    xor eax,eax
    syscall
    mov eax,1
    xor edi,edi
    syscall
.fail:
    mov eax,1
    mov edi,1
    syscall
message: db "Hello from a disk-loaded Ring 3 program!",10
message_end:
path: db "README.TXT",0,0,0
section .bss
buffer: resb 512
