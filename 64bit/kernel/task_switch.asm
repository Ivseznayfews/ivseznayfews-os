section .text
global task_start

; task_start(uint64_t rsp)
; 从 kmain 调用，切换到第一个任务
; 参数 rsp 在 rdi
task_start:
    mov rsp, rdi
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
