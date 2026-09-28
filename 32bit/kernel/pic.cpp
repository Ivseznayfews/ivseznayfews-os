#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

namespace pic {
    void remap() {
        outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
        io_wait();
        outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
        io_wait();
        outb(PIC1_DATA, 0x20);   // 主 PIC 偏移到 0x20
        io_wait();
        outb(PIC2_DATA, 0x28);   // 从 PIC 偏移到 0x28
        io_wait();
        outb(PIC1_DATA, 0x04);
        io_wait();
        outb(PIC2_DATA, 0x02);
        io_wait();
        outb(PIC1_DATA, ICW4_8086);
        io_wait();
        outb(PIC2_DATA, ICW4_8086);
        io_wait();

        // 关键修复：打开 IRQ0(定时器) 和 IRQ1(键盘)，其余关闭
        // bit0=IRQ0, bit1=IRQ1, 其余为 1 表示屏蔽
        outb(PIC1_DATA, 0xFC);   // 1111 1100 -> 只开 IRQ0, IRQ1
        outb(PIC2_DATA, 0xFF);   // 从 PIC 全关
    }
}
