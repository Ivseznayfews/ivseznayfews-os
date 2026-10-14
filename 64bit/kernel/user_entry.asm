bits 64
section .bss
alignb 16
saved_user_rsp: resq 1
alignb 16
section .text
global user_enter, user_leave, syscall_entry
extern syscall_dispatch, user_context, user_syscall_top
user_enter:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    pushfq
    cli
    mov r8,[rel user_context]
    mov [r8],rsp
    mov rax, cr3
    mov [r8+8],rax
    mov cr3, rdx
    push qword 0x1b
    push rsi
    push qword 0x202
    push qword 0x23
    push rdi
    iretq
user_leave:
    cli
    mov r8,[rel user_context]
    mov rax,[r8+8]
    mov cr3, rax
    mov rsp,[r8]
    mov rax, rdi
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
syscall_entry:
    mov [rel saved_user_rsp], rsp
    mov rsp,[rel user_syscall_top]
    push qword 0x1b
    push qword [rel saved_user_rsp]
    and r11, 0x8d5
    or r11, 0x202
    push r11
    push qword 0x23
    push rcx
    push qword 0
    push qword 128
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    mov r12, rsp
    and rsp, -16
    cld
    call syscall_dispatch
    mov rsp, r12
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq
section .note.GNU-stack noalloc noexec nowrite progbits
