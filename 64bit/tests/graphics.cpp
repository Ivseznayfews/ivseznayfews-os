#include "framebuffer.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
static void* allocation;
namespace heap { void* kmalloc(size_t bytes) { return allocation = malloc(bytes); } }
int main() {
    constexpr size_t stride=832, height=600, bytes=stride*height*4;
    auto* mapping=(uint32_t*)mmap((void*)0x18000000,bytes+8192,PROT_READ|PROT_WRITE,
                                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(mapping != MAP_FAILED);
    for(size_t i=0;i<(bytes+8192)/4;++i) mapping[i]=0xaabbccdd;
    MultibootInfo info={};
    info.flags=1u<<12; info.framebuffer_addr=(uintptr_t)(mapping+1024);
    info.framebuffer_pitch=stride*4; info.framebuffer_width=800; info.framebuffer_height=600;
    info.framebuffer_bpp=32; info.framebuffer_type=1;
    info.red_position=0; info.green_position=8; info.blue_position=16;
    info.red_size=info.green_size=info.blue_size=8;
    assert(framebuffer::valid_mode(&info));
    auto invalid=info; invalid.framebuffer_pitch=800*4-1; assert(!framebuffer::valid_mode(&invalid));
    invalid=info; invalid.framebuffer_bpp=24; assert(!framebuffer::valid_mode(&invalid));
    invalid=info; invalid.framebuffer_addr=(1ull<<32)-4096; assert(!framebuffer::valid_mode(&invalid));
    invalid=info; invalid.blue_position=8; assert(!framebuffer::valid_mode(&invalid));
    invalid=info; invalid.red_position=31; assert(!framebuffer::valid_mode(&invalid));
    invalid=info; invalid.framebuffer_width=2000; assert(!framebuffer::valid_mode(&invalid));
    assert(framebuffer::init(&info));
    auto* front=mapping+1024;
    framebuffer::rect(0,0,800,600,0x123456);
    framebuffer::rect(-10,-10,20,20,0xff0000);
    framebuffer::rect(798,598,100,100,0x00ff00);
    framebuffer::present();
    assert(front[0]==0x0000ff); // RGB channel ordering is taken from the boot mode.
    assert(front[10*stride+10]==0x563412);
    assert(front[599*stride+799]==0x00ff00);
    for(size_t y=0;y<height;++y) for(size_t x=800;x<stride;++x) assert(front[y*stride+x]==0xaabbccdd);
    for(size_t i=0;i<1024;++i) {
        assert(mapping[i]==0xaabbccdd);
        assert(front[bytes/4+i]==0xaabbccdd);
    }
    framebuffer::pointer(40,40);
    assert(front[40*stride+40]!=0x563412);
    framebuffer::pointer(799,599);
    assert(front[40*stride+40]==0x563412);
    framebuffer::glyph(20,20,'A',0xffffff);
    framebuffer::present();
    bool ink=false;
    for(int y=20;y<36;++y) for(int x=20;x<28;++x) ink|=front[y*stride+x]==0xffffff;
    assert(ink);
    framebuffer::clip(100,100,8,16);
    framebuffer::rect(80,80,80,80,0xffffff);
    framebuffer::glyph(100,100,'W',0x00ff00,2);
    framebuffer::present();
    assert(front[100*stride+99]==0x563412);
    assert(front[100*stride+108]==0x563412);
    assert(front[99*stride+100]==0x563412);
    assert(front[116*stride+100]==0x563412);
    assert(front[115*stride+107]==0xffffff || front[115*stride+107]==0x00ff00);
    framebuffer::reset_clip();
    framebuffer::rect(108,100,1,1,0xffffff);framebuffer::present();
    assert(front[100*stride+108]==0xffffff);
    free(allocation);
    assert(munmap(mapping,bytes+8192)==0);
    puts("PASS: framebuffer validation, channel order, pitch, clipping, glyphs and pointer restoration");
}
