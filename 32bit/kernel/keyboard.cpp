#include "keyboard.h"
#include "io.h"
#include "idt.h"
#include "serial.h"

static key_callback_t g_cb = nullptr;
static int g_shift = 0;
static int g_ctrl  = 0;

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
    uint8_t sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) { g_shift = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { g_shift = 0; return; }
    if (sc == 0x1D) { g_ctrl = 1; return; }
    if (sc == 0x9D) { g_ctrl = 0; return; }

    if (sc & 0x80) return;

    char c = g_shift ? kbd_upper[sc] : kbd_lower[sc];
    if (c && g_cb) g_cb(c);
}

// 初始化 8042 键盘控制器：清缓冲区 + 打开键盘中断位
static void init_8042() {
    // 清空输出缓冲区
    while (inb(0x64) & 0x01) inb(0x60);

    // 读取配置字节
    outb(0x64, 0x20);
    while (!(inb(0x64) & 0x01));
    uint8_t cfg = inb(0x60);

    // bit0=1 打开键盘中断, bit6=0 不做翻译
    cfg |=  0x01;
    cfg &= ~0x10;

    // 写回配置字节
    outb(0x64, 0x60);
    while (inb(0x64) & 0x02);
    outb(0x60, cfg);

    // 启用键盘
    outb(0x64, 0xAE);
}

namespace keyboard {
    void init() {
        init_8042();
        idt::set_handler(33, keyboard_handler); // IRQ1 = 32+1
    }
    void set_callback(key_callback_t cb) { g_cb = cb; }
}
