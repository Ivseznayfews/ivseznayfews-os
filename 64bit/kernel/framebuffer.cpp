#include "framebuffer.h"
#include "font.h"
#include "heap.h"

static volatile uint32_t* front;
static uint32_t* back;
static int w, h, stride;
static int clip_l,clip_t,clip_r,clip_b;
static uint8_t red, green, blue;
static int left, top, right, bottom;
static int pointer_x = -20, pointer_y = -20;
static uint32_t color(uint32_t rgb) {
    return ((rgb >> 16 & 255) << red) | ((rgb >> 8 & 255) << green) | ((rgb & 255) << blue);
}
static void damage(int x, int y, int width, int height) {
    if (x < left) left = x;
    if (y < top) top = y;
    if (x + width > right) right = x + width;
    if (y + height > bottom) bottom = y + height;
}
static void copy_rect(int x, int y, int width, int height) {
    for (int yy = y; yy < y + height && yy < h; ++yy) {
        if (yy < 0) continue;
        for (int xx = x; xx < x + width && xx < w; ++xx)
            if (xx >= 0) front[yy * stride + xx] = back[yy * w + xx];
    }
}
namespace framebuffer {
    bool valid_mode(const MultibootInfo* info) {
        if (!info || !(info->flags & (1u << 12)) || info->framebuffer_type != 1 ||
            info->framebuffer_bpp != 32 || info->framebuffer_width < 800 ||
            info->framebuffer_width > 1920 || info->framebuffer_height < 600 ||
            info->framebuffer_height > 1080 || info->framebuffer_pitch % 4 ||
            info->framebuffer_pitch < info->framebuffer_width * 4 ||
            info->framebuffer_pitch > 16384 || info->framebuffer_addr % 4 ||
            info->framebuffer_addr < 0x100000) return false;
        uint64_t bytes = uint64_t(info->framebuffer_pitch) * info->framebuffer_height;
        if (info->framebuffer_addr >= (1ull << 32) || bytes > (1ull << 32) - info->framebuffer_addr) return false;
        if (info->red_size != 8 || info->green_size != 8 || info->blue_size != 8 ||
            info->red_position > 24 || info->green_position > 24 || info->blue_position > 24) return false;
        uint32_t r = 255u << info->red_position, g = 255u << info->green_position, b = 255u << info->blue_position;
        return !(r & g) && !(r & b) && !(g & b);
    }
    bool init(const MultibootInfo* info) {
        if (!valid_mode(info)) return false;
        back = (uint32_t*)heap::kmalloc(size_t(info->framebuffer_width) * info->framebuffer_height * 4);
        if (!back) return false;
        front = (volatile uint32_t*)info->framebuffer_addr;
        w = info->framebuffer_width; h = info->framebuffer_height;
        stride = info->framebuffer_pitch / 4;
        red = info->red_position; green = info->green_position; blue = info->blue_position;
        left = top = 0; right = w; bottom = h;
        reset_clip();
        return true;
    }
    bool active() { return front != nullptr; }
    int width() { return w; }
    int height() { return h; }
    void reset_clip(){clip_l=clip_t=0;clip_r=w;clip_b=h;}
    void clip(int x,int y,int width,int height){
        clip_l=x<0?0:x;clip_t=y<0?0:y;clip_r=x+width>w?w:x+width;clip_b=y+height>h?h:y+height;
    }
    void rect(int x, int y, int width, int height, uint32_t rgb) {
        if (!front || width <= 0 || height <= 0) return;
        int x2 = x + width, y2 = y + height;
        if (x < clip_l) x = clip_l;
        if (y < clip_t) y = clip_t;
        if (x2 > clip_r) x2 = clip_r;
        if (y2 > clip_b) y2 = clip_b;
        if (x >= x2 || y >= y2) return;
        uint32_t pixel = color(rgb);
        for (int yy = y; yy < y2; ++yy)
            for (int xx = x; xx < x2; ++xx) back[yy * w + xx] = pixel;
        damage(x, y, x2 - x, y2 - y);
    }
    void glyph(int x, int y, char c, uint32_t rgb, int scale) {
        if (c < 32 || c > 126) c = '?';
        for (int yy = 0; yy < 16; ++yy)
            for (int xx = 0; xx < 8; ++xx)
                if (font8x16[c - 32][yy] & (1 << (7-xx)))
                    rect(x + xx*scale, y + yy*scale, scale, scale, rgb);
    }
    void text(int x, int y, const char* str, uint32_t rgb, int scale) {
        while (*str && x + 8*scale <= w) { glyph(x, y, *str++, rgb, scale); x += 8*scale; }
    }
    void present() {
        if (!front) return;
        if (right > left && bottom > top) copy_rect(left, top, right-left, bottom-top);
        left = w; top = h; right = bottom = 0;
    }
    void pointer(int x, int y) {
        if (!front) return;
        copy_rect(pointer_x, pointer_y, 12, 18);
        pointer_x = x; pointer_y = y;
        for (int yy = 0; yy < 17; ++yy) {
            int span = yy < 12 ? yy / 2 + 1 : 3;
            for (int xx = 0; xx <= span; ++xx) {
                if (x + xx >= w || y + yy >= h || x + xx < 0 || y + yy < 0) continue;
                front[(y+yy)*stride+x+xx] = color(xx == 0 || xx == span || yy == 0 ? 0x101826 : 0xe7f5ff);
            }
        }
    }
}
