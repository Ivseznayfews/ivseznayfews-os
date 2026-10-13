#pragma once
#include <stdint.h>

namespace task {
    void init();
    int  create(void (*entry)(), const char* name = "task");
    void start();
    uint64_t switches();
    void print_stats();

    // 抢占锁：lock() 后调度器不切换，unlock() 后恢复
    void lock();
    void unlock();
}

extern "C" uint64_t task_switch_from_irq(uint64_t current_rsp);
extern "C" void     task_start(uint64_t rsp);
