#pragma once
#include <stdint.h>
#include "idt.h"
namespace user {
    void init();
    int run(const char* path);
    int spawn(const char* path);
    int wait(int pid);
    bool kill(int pid);
    void stats();
    void process_test();
    void activate(int task_id);
    bool runnable(int task_id);
    bool active();
    void fault(Registers* r);
    void tick(Registers* r,uint64_t ticks);
    void test();
}
extern "C" uint64_t user_enter(uint64_t entry,uint64_t stack,uint64_t cr3);
extern "C" [[noreturn]] void user_leave(uint64_t code);
