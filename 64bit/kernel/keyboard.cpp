#include "keyboard.h"
#include "io.h"
#include "idt.h"
#include "critical.h"

static key_callback_t g_cb = nullptr;
static int g_shift = 0;
static int g_ctrl  = 0;
static bool g_extended = false;
static char queue[256];
static volatile uint16_t read_pos = 0, write_pos = 0;
static volatile uint64_t dropped = 0;

static const char kbd_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char kbd_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',
    0,  '*', 0,  ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void keyboard_handler(Registers*) {
    if (inb(0x64) & 0x20) return;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0 || sc == 0xE1) { g_extended = true; return; }
    if (g_extended) { g_extended = false; return; }
    if (sc == 0x2A || sc == 0x36) { g_shift = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { g_shift = 0; return; }
    if (sc == 0x1D) { g_ctrl = 1; return; }
    if (sc == 0x9D) { g_ctrl = 0; return; }

    if (sc & 0x80) return;

    char c = (g_ctrl && sc==0x1f) ? char(0x90) : (sc >= 0x3b && sc <= 0x40) ? char(0x81+sc-0x3b) :
             (g_shift ? kbd_upper[sc] : kbd_lower[sc]);
    if (c) {
        uint16_t next = (write_pos + 1) % sizeof(queue);
        if (next == read_pos) { ++dropped; return; }
        queue[write_pos] = c;
        write_pos = next;
    }
}

static void init_8042() {
    while (inb(0x64) & 0x01) inb(0x60);

    outb(0x64, 0x20);
    while (!(inb(0x64) & 0x01));
    uint8_t cfg = inb(0x60);

    cfg |=  0x01;
    cfg &= ~0x10;

    outb(0x64, 0x60);
    while (inb(0x64) & 0x02);
    outb(0x60, cfg);

    outb(0x64, 0xAE);
}

namespace keyboard {
    void init() {
        read_pos = write_pos = 0;
        dropped = 0;
        init_8042();
        idt::set_handler(33, keyboard_handler);
    }
    void poll() {
        // Process a bounded batch in task context, with interrupts enabled.
        for (unsigned i = 0; i < 256; ++i) {
            char c;
            {
                InterruptGuard guard;
                if (read_pos == write_pos) break;
                c = queue[read_pos];
                read_pos = (read_pos + 1) % sizeof(queue);
            }
            if (g_cb) g_cb(c);
        }
    }
    uint64_t dropped_keys() { InterruptGuard guard; return dropped; }
    void set_callback(key_callback_t cb) { g_cb = cb; }
}
