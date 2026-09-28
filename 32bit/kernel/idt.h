#pragma once
#include <stdint.h>

// 顺序必须与 isr_stubs.asm 中 push 的顺序严格对应（从栈顶到栈底）
struct Registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

typedef void (*isr_handler_t)(Registers*);

namespace idt {
    void init();
    void set_handler(int n, isr_handler_t h);
}
