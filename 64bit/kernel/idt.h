#pragma once
#include <stdint.h>

// 顺序必须与 isr_stubs.asm 中 push 顺序严格对应（栈顶到栈底）
// 64 位下不保存段寄存器（扁平段，永远是 0x10）
struct Registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*isr_handler_t)(Registers*);

namespace idt {
    void init();
    void set_handler(int n, isr_handler_t h);
}
