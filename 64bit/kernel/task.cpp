#include "task.h"
#include "vga.h"

#define MAX_TASKS  8
#define STACK_SIZE 16384

struct Task {
    uint64_t rsp;
    uint64_t stack;
    void   (*entry)();
    int      id;
};

// 静态分配任务栈，避免堆的复杂性
static uint8_t task_stacks[MAX_TASKS][STACK_SIZE] __attribute__((aligned(16)));

static Task     g_tasks[MAX_TASKS];
static int      g_task_count = 0;
static int      g_current = -1;
static uint64_t g_switch_count = 0;
static volatile int g_multitasking = 0;
static volatile int g_sched_lock = 0;

static void task_entry() {
    g_tasks[g_current].entry();
    // A returning kernel task stays idle rather than returning into garbage.
    while (1) asm volatile("hlt");
}

static void setup_stack(Task* t) {
    uint64_t top = t->stack + STACK_SIZE;
    top &= ~0xFULL;
    uint64_t* sp = (uint64_t*)top;

    // Long-mode IRETQ always restores SS:RSP, even within ring 0.
    // Reserve a dummy return address for the SysV function-entry alignment.
    *--sp = 0;
    uint64_t initial_rsp = (uint64_t)sp;
    *--sp = 0x10;
    *--sp = initial_rsp;
    *--sp = 0x202;
    *--sp = 0x08;
    *--sp = (uint64_t)task_entry;
    *--sp = 0;
    *--sp = 0;
    for (int i = 0; i < 15; i++) *--sp = 0;

    t->rsp = (uint64_t)sp;
}

namespace task {
    void init() {
        g_task_count = 0;
        g_current    = -1;
        g_switch_count = 0;
        g_multitasking = 0;
        g_sched_lock = 0;
    }

    int create(void (*entry)()) {
        if (!entry || g_multitasking || g_task_count >= MAX_TASKS) return -1;
        int id = g_task_count++;
        Task* t = &g_tasks[id];
        t->entry = entry;
        t->id    = id;
        t->stack = (uint64_t)task_stacks[id];
        setup_stack(t);
        return id;
    }

    void start() {
        if (g_task_count == 0) return;
        asm volatile("cli" ::: "memory");
        g_current = 0;
        g_multitasking = 1;
        task_start(g_tasks[0].rsp);
    }

    void lock()   { g_sched_lock++; }
    void unlock() { if (g_sched_lock > 0) g_sched_lock--; }

    uint64_t switches() { return g_switch_count; }
}

extern "C" uint64_t task_switch_from_irq(uint64_t current_rsp) {
    if (!g_multitasking) return current_rsp;
    if (g_task_count < 2) return current_rsp;

    if (g_sched_lock > 0) {
        if (g_current >= 0 && g_current < g_task_count)
            g_tasks[g_current].rsp = current_rsp;
        return current_rsp;
    }

    if (g_current >= 0 && g_current < g_task_count) {
        g_tasks[g_current].rsp = current_rsp;
    }

    g_current = (g_current + 1) % g_task_count;
    g_switch_count++;

    return g_tasks[g_current].rsp;
}
