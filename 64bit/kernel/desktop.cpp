#include "desktop.h"
#include "framebuffer.h"
#include "vga.h"
#include "shell.h"
#include "pit.h"
#include "pmm.h"
#include "heap.h"
#include "task.h"
#include "mouse.h"
#include "serial.h"
#include "storage.h"
#include "smp.h"

extern "C" uint16_t kernel_console[80*25];
static bool full_redraw = true, console_dirty = true;
static int window_dx, window_dy, drag_x, drag_y;
static bool dragging, minimized;
static int selected, cursor_x = 450, cursor_y = 350;
static uint64_t last_second = UINT64_MAX;
static constexpr uint32_t BG=0x0b111b, PANEL=0x111c2a, EDGE=0x243348;
static constexpr uint32_t INK=0xe0eaf7, MUTED=0x8294ad, ACCENT=0x62dfb6;
static int sidebar() { return framebuffer::width() >= 960 ? 168 : 88; }
static int content_x() { return sidebar()+24; }
static int content_width() { return framebuffer::width()-content_x()-24; }
static void text(int x, int y, const char* s, uint32_t color=INK, int scale=1) {
    framebuffer::text(x,y,s,color,scale);
}
static void rect(int x, int y, int w, int h, uint32_t color) {
    framebuffer::rect(x,y,w,h,color);
}
static void number(int x, int y, uint64_t value, uint32_t color=INK, int scale=1) {
    char buffer[24]; int len=0;
    do { buffer[len++]='0'+value%10; value/=10; } while(value);
    char output[24];
    for(int i=0;i<len;++i) output[i]=buffer[len-i-1];
    output[len]=0; text(x,y,output,color,scale);
}
static void panel(int x, int y, int w, int h) {
    rect(x,y,w,h,EDGE); rect(x+1,y+1,w-2,h-2,PANEL);
}
static void status_cards() {
    int x=content_x(), cw=(content_width()-24)/3;
    const char* titles[]={"FREE MEMORY / KiB", "UPTIME / SEC", "TASK SWITCHES"};
    uint64_t values[]={(pmm::total_pages()-pmm::used_pages())*4, pit::ticks()/100, task::switches()};
    for(int i=0;i<3;++i) {
        int xx=x+i*(cw+12);
        panel(xx,104,cw,80);
        text(xx+14,115,titles[i],MUTED);
        number(xx+14,140,values[i],i==0?ACCENT:INK,2);
    }
}
static void terminal() {
    int x=content_x()+window_dx, width=content_width(), top=210+window_dy, bottom=framebuffer::height()-48+window_dy;
    if(minimized) { text(content_x()+16,230,"Terminal minimized. Press F1 to restore.",MUTED); return; }
    panel(x,top,width,bottom-top);
    rect(x+1,top+1,width-2,32,0x172437);
    text(x+14,top+9,"TERMINAL  /  drag title bar",ACCENT);
    text(x+width-32,top+9,"_",ACCENT);
    rect(x+1,top+34,width-2,bottom-top-35,0x0c1521);
    int rows=(bottom-top-48)/16;
    if(rows>25) rows=25;
    int first=int(vga::cursor_row())-rows+1;
    if(first<0) first=0;
    for(int yy=0;yy<rows;++yy)
        for(int xx=0;xx<80;++xx) {
            char c=kernel_console[(first+yy)*80+xx]&255;
            if(c && c!=' ') framebuffer::glyph(x+12+xx*8,top+42+yy*16,c,0xcbd8e9);
        }
    int cursor_row=int(vga::cursor_row())-first;
    if(cursor_row>=0 && cursor_row<rows)
        rect(x+12+int(vga::cursor_col())*8,top+42+cursor_row*16+14,8,2,ACCENT);
}
static void system_page() {
    int x=content_x(), width=content_width();
    panel(x,210,width,framebuffer::height()-258);
    text(x+24,232,"SYSTEM OVERVIEW",INK,2);
    text(x+24,276,"Kernel",MUTED); text(x+208,276,"Ivseznayfews 64-bit v2.0");
    text(x+24,308,"Scheduler",MUTED); text(x+208,308,"BSP tasks + AP worker queue");
    text(x+24,340,"Display",MUTED);
    number(x+208,340,framebuffer::width()); text(x+248,340,"x");
    number(x+264,340,framebuffer::height()); text(x+304,340,"/ 32-bit RGB");
    text(x+24,372,"Mouse",MUTED); text(x+208,372,mouse::available()?"PS/2 connected":"Keyboard navigation available");
    text(x+24,404,"Heap bytes in use",MUTED); number(x+208,404,heap::allocated_bytes(),ACCENT);
    int bar_width=width-48;
    uint64_t total=pmm::total_pages(), used=pmm::used_pages();
    rect(x+24,444,bar_width,12,EDGE);
    rect(x+24,444,total?int(used*bar_width/total):0,12,ACCENT);
    text(x+24,466,"Physical memory allocated / managed",MUTED);
    text(x+24,500,"Open Terminal and run memtest to check allocations.",MUTED);
}
static void help_page() {
    int x=content_x(), width=content_width();
    panel(x,210,width,framebuffer::height()-258);
    text(x+24,232,"WELCOME HOME",INK,2);
    text(x+24,276,"F1  Terminal     F2  System     F3  Help   F4 Files",ACCENT);
    text(x+24,304,"Click a sidebar item, or use Tab to switch views.",MUTED);
    const char* rows[]={"help       List available commands", "mem        Inspect physical memory and heap",
                        "tasks      Inspect the scheduler", "uptime     Show uptime and input queue status",
                        "memtest    Run the allocator integrity check", "clear      Clear the terminal",
                        "shutdown   Power off the virtual machine"};
    for(int i=0;i<7;++i) text(x+24,344+i*24,rows[i]);
}
static void files_page() {
    int x=content_x();panel(x,210,content_width(),framebuffer::height()-258);
    text(x+24,232,"FILES / FAT32",INK,2);
    if(!vfs::mounted()) {text(x+24,280,"No disk. Start with run-vm.sh to attach the test disk.",MUTED);return;}
    text(x+24,274,"NAME                 BYTES       ROOT / READ ONLY",MUTED);
    vfs::Entry entry;
    for(unsigned i=0;i<8 && vfs::entry(i,entry);++i) {
        text(x+24,306+i*24,entry.name,ACCENT);number(x+208,306+i*24,entry.size);
    }
    text(x+24,framebuffer::height()-82,"Terminal: cat readme.txt  /  run hello.elf",MUTED);
}
static void chrome() {
    int w=framebuffer::width(), h=framebuffer::height(), side=sidebar();
    rect(0,0,w,h,BG); rect(0,0,side,h,0x101a27); rect(side,0,1,h,EDGE);
    rect(16,22,32,32,ACCENT); text(24,24,"I",BG,2);
    if(side>100) { text(60,23,"IVSE",INK,2); text(60,47,"OS / 64",MUTED); }
    text(content_x(),24,"Ivseznayfews",INK,2);
    text(content_x(),61,"A small system. A real desktop.",MUTED);
    const char* names[]={"Terminal","System","Help","Files"};
    for(int i=0;i<4;++i) {
        int yy=112+i*52;
        if(selected==i) { rect(8,yy,side-16,40,0x203a40); rect(8,yy,3,40,ACCENT); }
        text(16,yy+12,names[i],selected==i?ACCENT:MUTED);
        if(side>100) { char key[]={'F',char('1'+i),0}; text(side-32,yy+12,key,MUTED); }
    }
    text(16,h-80,"v2.0",MUTED);
    rect(0,h-28,w,28,0x101a27); rect(0,h-28,w,1,EDGE);
    text(16,h-22,"READY",ACCENT);
    text(88,h-22,"F1 Terminal   F2 System   F3 Help",MUTED);
    text(w-200,h-22,"CPUS ONLINE",MUTED);number(w-88,h-22,smp::count(),ACCENT);
}
static void select_view(int value) {
    if(value==0 && minimized) {minimized=false;full_redraw=true;}
    if(value==selected) return;
    selected=value; full_redraw=true;
    serial::print("[GUI] view="); serial::putchar('0'+value); serial::putchar('\n');
}
namespace desktop {
    bool init(const MultibootInfo* info) {
        if(!framebuffer::init(info)) return false;
        cursor_x=framebuffer::width()-60; cursor_y=framebuffer::height()-70;
        serial::print("[GUI] framebuffer ready\n");
        return true;
    }
    bool active() { return framebuffer::active(); }
    void invalidate() { console_dirty=true; }
    void show_terminal() { select_view(0); console_dirty=true; }
    void input(char c) {
        if(!active()) { if((uint8_t)c<128 && c!='\t') shell::input(c); return; }
        uint8_t key=(uint8_t)c;
        if(key>=0x81 && key<=0x84) { select_view(key-0x81); return; }
        if(c=='\t') { select_view((selected+1)%4); return; }
        if(c==27) { select_view(0); return; }
        if(selected==0 && (c=='\n' || c=='\b' || (key>=32 && key<127))) shell::input(c);
    }
    void render() {
        if(!active()) return;
        int dx,dy; bool click;
        bool moved=mouse::take(dx,dy,click);
        if(moved) {
            cursor_x+=dx; cursor_y+=dy;
            if(cursor_x<0) cursor_x=0;
            if(cursor_y<0) cursor_y=0;
            if(cursor_x>=framebuffer::width()) cursor_x=framebuffer::width()-1;
            if(cursor_y>=framebuffer::height()) cursor_y=framebuffer::height()-1;
            if(click && cursor_x>=8 && cursor_x<sidebar()-8)
                for(int i=0;i<4;++i) if(cursor_y>=112+i*52 && cursor_y<152+i*52) select_view(i);
        }
        bool held=mouse::pressed();
        if(!held) dragging=false;
        if(selected==0 && !minimized && click) {
            int x=content_x()+window_dx, y=210+window_dy;
            if(cursor_x>=x && cursor_x<x+content_width() && cursor_y>=y && cursor_y<y+33) {
                if(cursor_x>=x+content_width()-48) {minimized=true;full_redraw=true;}
                else {dragging=true;drag_x=cursor_x-window_dx;drag_y=cursor_y-window_dy;}
            }
        }
        if(dragging && held && moved) {
            window_dx=cursor_x-drag_x;window_dy=cursor_y-drag_y;
            if(window_dx<sidebar()-content_x())window_dx=sidebar()-content_x();
            if(window_dx>23)window_dx=23;
            if(window_dy< -110)window_dy=-110;
            if(window_dy>20)window_dy=20;
            full_redraw=true;
        }
        uint64_t second=pit::ticks()/100;
        bool time_changed=second!=last_second;
        if(full_redraw) chrome();
        if(full_redraw || time_changed) status_cards();
        if(selected==0 && (full_redraw || console_dirty || time_changed)) terminal();
        if(selected==1 && (full_redraw || time_changed)) system_page();
        if(selected==2 && full_redraw) help_page();
        if(selected==3 && full_redraw) files_page();
        if(full_redraw || time_changed || console_dirty || moved) {
            framebuffer::present();
            framebuffer::pointer(cursor_x,cursor_y);
        }
        last_second=second; full_redraw=console_dirty=false;
    }
}
