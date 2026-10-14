#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void io_wait() { outb(0x80, 0); }
static inline uint16_t inw(uint16_t port) { uint16_t value; asm volatile("inw %1,%0":"=a"(value):"Nd"(port));return value; }
static inline uint32_t inl(uint16_t port) { uint32_t value; asm volatile("inl %1,%0":"=a"(value):"Nd"(port));return value; }
static inline void outl(uint16_t port,uint32_t value) { asm volatile("outl %0,%1"::"a"(value),"Nd"(port)); }
static inline uint64_t rdmsr(uint32_t msr) { uint32_t lo,hi;asm volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(msr));return (uint64_t(hi)<<32)|lo; }
static inline void wrmsr(uint32_t msr,uint64_t value) {asm volatile("wrmsr"::"c"(msr),"a"(uint32_t(value)),"d"(uint32_t(value>>32)):"memory");}

static inline void outw(uint16_t port,uint16_t value){asm volatile("outw %0,%1"::"a"(value),"Nd"(port));}
