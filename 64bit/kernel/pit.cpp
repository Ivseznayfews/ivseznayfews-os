#include "pit.h"
#include "io.h"
#include "idt.h"

static volatile uint64_t g_ticks = 0;

static void pit_handler(Registers*) {
    g_ticks++;
}

namespace pit {
    void init(uint32_t freq_hz) {
        // PIT 输入时钟 1193182 Hz
        uint32_t divisor = 1193182 / (freq_hz ? freq_hz : 100);
        if (divisor < 1)     divisor = 1;
        if (divisor > 65535) divisor = 65535;

        // 通道0, 先低后高, 模式3(方波), 二进制
        outb(0x43, 0x36);
        outb(0x40, (uint8_t)(divisor & 0xFF));
        outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

        // 注册 IRQ0 处理
        idt::set_handler(32, pit_handler);
    }

    uint64_t ticks() { return g_ticks; }
}
