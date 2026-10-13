#pragma once
#include <stdint.h>

// Single-core critical section; nested guards preserve the caller's IF state.
class InterruptGuard {
#ifndef OS_HOST_TEST
    uint64_t flags;
#endif
public:
    InterruptGuard() {
#ifndef OS_HOST_TEST
        asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
#endif
    }
    ~InterruptGuard() {
#ifndef OS_HOST_TEST
        if (flags & (1ull << 9)) asm volatile("sti" ::: "memory");
#endif
    }
    InterruptGuard(const InterruptGuard&) = delete;
    InterruptGuard& operator=(const InterruptGuard&) = delete;
};
