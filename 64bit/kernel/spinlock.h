#pragma once
class SpinLock {
    unsigned value=0;
public:
    void lock() { while(__atomic_exchange_n(&value,1,__ATOMIC_ACQUIRE)) while(__atomic_load_n(&value,__ATOMIC_RELAXED)) asm volatile("pause"); }
    void unlock() { __atomic_store_n(&value,0,__ATOMIC_RELEASE); }
};
class SpinGuard {
    SpinLock& lock;
public:
    explicit SpinGuard(SpinLock& l):lock(l) {lock.lock();}
    ~SpinGuard() {lock.unlock();}
    SpinGuard(const SpinGuard&)=delete;
};
