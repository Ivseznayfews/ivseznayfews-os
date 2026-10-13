#include "mouse.h"
#include "io.h"
#include "idt.h"
#include "critical.h"

static bool enabled;
static uint8_t packet[3], packet_index;
static int pending_x, pending_y;
static bool pending_click, left_down;
static bool writable() {
    for (unsigned i=0; i<100000; ++i) if (!(inb(0x64)&2)) return true;
    return false;
}
static bool command(uint8_t value) {
    if (!writable()) return false;
    outb(0x64, value);
    return true;
}
static bool data(uint8_t value) {
    if (!writable()) return false;
    outb(0x60, value);
    return true;
}
static bool read(uint8_t& value) {
    for (unsigned i=0; i<100000; ++i) {
        if (inb(0x64)&1) { value = inb(0x60); return true; }
    }
    return false;
}
static bool mouse_command(uint8_t value) {
    uint8_t reply;
    return command(0xd4) && data(value) && read(reply) && reply == 0xfa;
}
static void handler(Registers*) {
    if (!(inb(0x64)&0x20)) return;
    uint8_t value = inb(0x60);
    if (!packet_index && !(value&8)) return;
    packet[packet_index++] = value;
    if (packet_index != 3) return;
    packet_index = 0;
    if (packet[0]&0xc0) return;
    int dx = int(packet[1]) - ((packet[0]&0x10) ? 256 : 0);
    int dy = int(packet[2]) - ((packet[0]&0x20) ? 256 : 0);
    // Bound queued movement if the shell is occupied by a long command.
    pending_x += dx; pending_y -= dy;
    if (pending_x > 4096) pending_x = 4096;
    if (pending_x < -4096) pending_x = -4096;
    if (pending_y > 4096) pending_y = 4096;
    if (pending_y < -4096) pending_y = -4096;
    bool pressed = packet[0]&1;
    if (pressed && !left_down) pending_click = true;
    left_down = pressed;
}
namespace mouse {
    bool init() {
        uint8_t config;
        if (!command(0xa8) || !command(0x20) || !read(config)) return false;
        if (!mouse_command(0xf6) || !mouse_command(0xf4)) { command(0xa7); return false; }
        config = (config | 2) & ~0x20;
        if (!command(0x60) || !data(config)) { command(0xa7); return false; }
        idt::set_handler(44, handler);
        outb(0x21, inb(0x21) & ~4);  // PIC cascade
        outb(0xa1, inb(0xa1) & ~0x10); // IRQ12
        enabled = true;
        return true;
    }
    bool available() { return enabled; }
    bool pressed() { InterruptGuard guard; return left_down; }
    bool take(int& dx, int& dy, bool& click) {
        InterruptGuard guard;
        dx = pending_x; dy = pending_y; click = pending_click;
        pending_x = pending_y = 0; pending_click = false;
        return dx || dy || click;
    }
}
