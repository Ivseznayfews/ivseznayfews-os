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
        outb(PIC1_DATA, 0x20);
        io_wait();
        outb(PIC2_DATA, 0x28);
        io_wait();
        outb(PIC1_DATA, 0x04);
        io_wait();
        outb(PIC2_DATA, 0x02);
        io_wait();
        outb(PIC1_DATA, ICW4_8086);
        io_wait();
        outb(PIC2_DATA, ICW4_8086);
        io_wait();

        // 只开 IRQ0(定时器) 和 IRQ1(键盘)
        outb(PIC1_DATA, 0xFC);
        outb(PIC2_DATA, 0xFF);
    }
}
