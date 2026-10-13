#pragma once
#include "multiboot.h"
namespace framebuffer {
    bool valid_mode(const MultibootInfo* info);
    bool init(const MultibootInfo* info);
    bool active();
    int width();
    int height();
    void rect(int x, int y, int w, int h, uint32_t rgb);
    void text(int x, int y, const char* text, uint32_t rgb, int scale = 1);
    void glyph(int x, int y, char c, uint32_t rgb, int scale = 1);
    void present();
    void pointer(int x, int y);
}
