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
static constexpr uint32_t BG=0x0b111b,PANEL=0x111c2a,EDGE=0x243348,INK=0xe0eaf7,MUTED=0x8294ad,ACCENT=0x62dfb6;
struct Window {int x,y,w,h;bool visible;};
static Window windows[5];
static int order[5]={0,1,2,3,4},selected,cursor_x,cursor_y,dragging=-1,drag_x,drag_y;
static bool redraw=true;
static uint64_t last_second=UINT64_MAX;
static char editor[4097],editor_path[64]="NOTE.TXT",file_path[64]="/";
static unsigned editor_size;
static bool editor_dirty;
static const char* editor_message="New document / F6 or Ctrl+S saves";
static const char* names[]={"Terminal","System","Help","Files","Editor"};
static int sidebar(){return framebuffer::width()>=960?168:88;}
static int content_x(){return sidebar()+24;}
static int content_width(){return framebuffer::width()-content_x()-24;}
static void rect(int x,int y,int w,int h,uint32_t c){framebuffer::rect(x,y,w,h,c);}
static void text(int x,int y,const char* s,uint32_t c=INK,int scale=1){framebuffer::text(x,y,s,c,scale);}
static void number(int x,int y,uint64_t n,uint32_t c=INK,int scale=1){
    char a[24],b[24];unsigned len=0;do{a[len++]='0'+n%10;n/=10;}while(n);
    for(unsigned i=0;i<len;++i)b[i]=a[len-i-1];
    b[len]=0;text(x,y,b,c,scale);
}
static void panel(int x,int y,int w,int h){rect(x,y,w,h,EDGE);rect(x+1,y+1,w-2,h-2,PANEL);}
static void focus(int id){
    windows[id].visible=true;
    int pos=0;while(pos<5&&order[pos]!=id)++pos;
    for(int i=pos;i<4;++i)order[i]=order[i+1];
    order[4]=id;selected=id;redraw=true;
    serial::print("[GUI] view=");serial::putchar('0'+id);serial::putchar('\n');
}
static void minimize(int id){
    windows[id].visible=false;dragging=-1;
    for(int i=4;i>=0;--i)if(windows[order[i]].visible){selected=order[i];break;}
    redraw=true;
}
static void cards(){
    int x=content_x(),cw=(content_width()-24)/3;
    const char* title[]={"FREE MEMORY / KiB","UPTIME / SEC","TASK SWITCHES"};
    uint64_t value[]={(pmm::total_pages()-pmm::used_pages())*4,pit::ticks()/100,task::switches()};
    for(int i=0;i<3;++i){int xx=x+i*(cw+12);panel(xx,104,cw,80);text(xx+14,115,title[i],MUTED);number(xx+14,140,value[i],i?INK:ACCENT,2);}
}
static void chrome(){
    int w=framebuffer::width(),h=framebuffer::height(),side=sidebar();
    rect(0,0,w,h,BG);rect(0,0,side,h,0x101a27);rect(side,0,1,h,EDGE);
    rect(16,22,32,32,ACCENT);text(24,24,"I",BG,2);
    if(side>100){text(60,23,"IVSE",INK,2);text(60,47,"OS / 64",MUTED);}
    text(content_x(),24,"Ivseznayfews",INK,2);text(content_x(),61,"Processes. Files. Windows.",MUTED);
    for(int i=0;i<5;++i){int y=112+i*52;
        if(selected==i&&windows[i].visible){rect(8,y,side-16,40,0x203a40);rect(8,y,3,40,ACCENT);}
        text(16,y+12,names[i],selected==i?ACCENT:MUTED);
        if(side>100){char key[]={'F',char('1'+i),0};text(side-32,y+12,key,MUTED);}
    }
    text(16,h-80,"v2.1",MUTED);rect(0,h-28,w,28,0x101a27);rect(0,h-28,w,1,EDGE);
    text(16,h-22,"READY",ACCENT);text(88,h-22,"F1-F5 windows  /  F6 save",MUTED);
    text(w-200,h-22,"CPUS ONLINE",MUTED);number(w-88,h-22,smp::count(),ACCENT);cards();
}
static void terminal(const Window& w){
    int rows=(w.h-48)/16;if(rows>25)rows=25;
    int first=int(vga::cursor_row())-rows+1;if(first<0)first=0;
    for(int y=0;y<rows;++y)for(int x=0;x<80;++x){char c=kernel_console[(first+y)*80+x]&255;if(c&&c!=' ')framebuffer::glyph(w.x+12+x*8,w.y+42+y*16,c,0xcbd8e9);}
    int cursor=int(vga::cursor_row())-first;
    if(selected==0&&cursor>=0&&cursor<rows)rect(w.x+12+vga::cursor_col()*8,w.y+42+cursor*16+14,8,2,ACCENT);
}
static void system(const Window& w){
    int x=w.x+20,y=w.y+48;text(x,y,"SYSTEM OVERVIEW",INK,2);y+=40;
    text(x,y,"Kernel: 64-bit v2.1");y+=28;text(x,y,"BSP: preemptive processes / AP: worker queue");y+=28;
    text(x,y,"Online CPUs:",MUTED);number(x+176,y,smp::count(),ACCENT);y+=28;
    text(x,y,"Heap bytes:",MUTED);number(x+176,y,heap::allocated_bytes(),ACCENT);y+=28;
    text(x,y,"FAT32:",MUTED);text(x+176,y,vfs::mounted()?"read / write":"no disk");y+=28;
    text(x,y,"Tests: memtest / usertest / proctest / fstest",MUTED);
}
static void help(const Window& w){
    const char* rows[]={"F1-F5 opens / raises a window","Click windows to change keyboard focus","Drag title bars; _ minimizes; X closes","Terminal: spawn sleep.elf / ps / wait PID","Files: click a directory or text file","Editor: type, Backspace, Enter; F6 saves","Shell: edit PATH / mkdir PATH / write PATH TEXT","Ctrl+S also saves; Esc raises Terminal"};
    text(w.x+20,w.y+48,"DESKTOP GUIDE",INK,2);for(unsigned i=0;i<8;++i)text(w.x+20,w.y+92+i*26,rows[i]);
}
static void files(const Window& w){
    text(w.x+20,w.y+46,file_path,ACCENT);text(w.x+20,w.y+72,"NAME                 BYTES       TYPE",MUTED);
    vfs::Entry e;unsigned rows=(w.h-130)/24;
    for(unsigned i=0;i<rows&&vfs::entry_at(file_path,i,e);++i){int y=w.y+100+i*24;text(w.x+20,y,e.name,ACCENT);number(w.x+208,y,e.size);text(w.x+320,y,e.directory?"DIR":"FILE",MUTED);}
    text(w.x+20,w.y+w.h-26,"Click text to edit. Backspace: root.",MUTED);
}
static void editor_page(const Window& w){
    text(w.x+20,w.y+44,editor_path,ACCENT);text(w.x+20,w.y+68,editor_message,MUTED);
    int columns=(w.w-40)/8,rows=(w.h-118)/16;
    unsigned line=0,col=0,total=1;
    for(unsigned i=0;i<editor_size;++i){if(editor[i]=='\n'||++col>=unsigned(columns)){++total;col=0;}}
    unsigned first=total>unsigned(rows)?total-rows:0;col=0;
    for(unsigned i=0;i<editor_size;++i){char c=editor[i];
        if(c!='\n'&&line>=first&&line-first<unsigned(rows))framebuffer::glyph(w.x+20+col*8,w.y+98+(line-first)*16,c,INK);
        if(c=='\n'||++col>=unsigned(columns)){++line;col=0;}
    }
    if(line>=first&&line-first<unsigned(rows))rect(w.x+20+col*8,w.y+112+(line-first)*16,8,2,ACCENT);
    text(w.x+20,w.y+w.h-24,editor_dirty?"UNSAVED / F6 or Ctrl+S to save":"Saved / 4096-byte editor",editor_dirty?0xf6c177:MUTED);
}
static void draw_window(int id){
    const auto& w=windows[id];framebuffer::clip(w.x,w.y,w.w,w.h);panel(w.x,w.y,w.w,w.h);
    rect(w.x+1,w.y+1,w.w-2,32,selected==id?0x172437:0x142030);
    text(w.x+14,w.y+9,names[id],selected==id?ACCENT:MUTED);text(w.x+104,w.y+9,"/ drag title bar",MUTED);
    text(w.x+w.w-32,w.y+9,"_",ACCENT);text(w.x+w.w-64,w.y+9,"X",MUTED);
    rect(w.x+1,w.y+34,w.w-2,w.h-35,0x0c1521);
    framebuffer::clip(w.x+1,w.y+34,w.w-2,w.h-35);
    if(id==0)terminal(w);else if(id==1)system(w);else if(id==2)help(w);else if(id==3)files(w);else editor_page(w);
    framebuffer::reset_clip();
}
static void save(){
    int n=vfs::write(editor_path,editor,editor_size);
    if(n==int(editor_size)){editor_dirty=false;editor_message="Saved to FAT32";serial::print("[GUI] editor saved\n");}
    else editor_message="Save failed. Check path and disk.";
    redraw=true;
}
namespace desktop {
    bool init(const MultibootInfo* info){
        if(!framebuffer::init(info))return false;
        for(int i=0;i<5;++i){int offset=i*12;
            windows[i]={content_x()+offset,210+offset,content_width()-offset,framebuffer::height()-258-offset,i==0};
        }
        cursor_x=framebuffer::width()-60;cursor_y=framebuffer::height()-70;
        serial::print("[GUI] framebuffer ready\n");return true;
    }
    bool active(){return framebuffer::active();}
    void invalidate(){redraw=true;}
    void show_terminal(){focus(0);}
    void edit(const char* path){
        if(!active()){vga::print("edit: graphical desktop required\n");return;}
        if(editor_dirty){editor_message="Save current document before opening another";focus(4);return;}
        unsigned n=0;while(path[n]&&n<63){editor_path[n]=path[n];++n;}
        if(path[n]){editor_message="Path too long";return;}editor_path[n]=0;
        int size=vfs::read(path,editor,4096);editor_size=size>=0?size:0;editor[editor_size]=0;
        editor_message=size>=0?"Opened / F6 or Ctrl+S saves":"New document / F6 or Ctrl+S saves";editor_dirty=false;focus(4);
    }
    void input(char c){
        uint8_t key=(uint8_t)c;
        if(!active()){if(key<128&&c!='\t')shell::input(c);return;}
        if(key>=0x81&&key<=0x85){focus(key-0x81);return;}
        if(key==0x86||key==0x90){if(selected==4)save();return;}
        if(c=='\t'){focus((selected+1)%5);return;}
        if(c==27){focus(0);return;}
        if(!windows[selected].visible)return;
        if(selected==0){if(c=='\n'||c=='\b'||(key>=32&&key<127))shell::input(c);}
        else if(selected==4){
            if(c=='\b'){if(editor_size){--editor_size;editor_dirty=true;}}
            else if((c=='\n'||(key>=32&&key<127))&&editor_size<4096){editor[editor_size++]=c;editor_dirty=true;}
            editor[editor_size]=0;redraw=true;
        }else if(selected==3&&c=='\b'){file_path[0]='/';file_path[1]=0;redraw=true;}
    }
    void render(){
        if(!active())return;
        int dx,dy;bool click;bool moved=mouse::take(dx,dy,click);
        if(moved){cursor_x+=dx;cursor_y+=dy;if(cursor_x<0)cursor_x=0;if(cursor_y<0)cursor_y=0;
            if(cursor_x>=framebuffer::width())cursor_x=framebuffer::width()-1;
            if(cursor_y>=framebuffer::height())cursor_y=framebuffer::height()-1;
        }
        bool held=mouse::pressed();if(!held)dragging=-1;
        if(click){
            if(cursor_x>=8&&cursor_x<sidebar()-8){for(int i=0;i<5;++i)if(cursor_y>=112+i*52&&cursor_y<152+i*52)focus(i);}
            else for(int i=4;i>=0;--i){int id=order[i];auto& w=windows[id];if(!w.visible||cursor_x<w.x||cursor_x>=w.x+w.w||cursor_y<w.y||cursor_y>=w.y+w.h)continue;
                focus(id);
                if(cursor_y<w.y+33){
                    if(cursor_x>=w.x+w.w-48)minimize(id);
                    else if(cursor_x>=w.x+w.w-80){if(id==4&&editor_dirty)editor_message="Unsaved changes / save before close";else minimize(id);}
                    else {dragging=id;drag_x=cursor_x-w.x;drag_y=cursor_y-w.y;}
                }else if(id==3&&cursor_y>=w.y+100&&cursor_y<w.y+w.h-36){
                    unsigned row=(cursor_y-w.y-100)/24;vfs::Entry e;
                    if(vfs::entry_at(file_path,row,e)){
                        char path[64];unsigned n=0;
                        if(!(file_path[0]=='/'&&!file_path[1])){while(file_path[n]&&n<48){path[n]=file_path[n];++n;}path[n++]='/';}
                        unsigned j=0;while(e.name[j]&&n<63)path[n++]=e.name[j++];path[n]=0;
                        if(e.directory){for(unsigned k=0;k<=n;++k)file_path[k]=path[k];redraw=true;}
                        else {bool executable=false;for(unsigned k=0;k+3<n;++k)if(path[k]=='.'&&path[k+1]=='E'&&path[k+2]=='L'&&path[k+3]=='F')executable=true;if(!executable)edit(path);}
                    }
                }
                break;
            }
        }
        if(dragging>=0&&held&&moved){auto& w=windows[dragging];w.x=cursor_x-drag_x;w.y=cursor_y-drag_y;
            if(w.x<sidebar())w.x=sidebar();
            if(w.x+w.w>framebuffer::width())w.x=framebuffer::width()-w.w;
            if(w.y<72)w.y=72;
            if(w.y+w.h>framebuffer::height()-28)w.y=framebuffer::height()-28-w.h;
            redraw=true;
        }
        uint64_t second=pit::ticks()/100;
        if(redraw||second!=last_second){framebuffer::reset_clip();chrome();for(int i=0;i<5;++i)if(windows[order[i]].visible)draw_window(order[i]);framebuffer::present();redraw=false;}
        framebuffer::pointer(cursor_x,cursor_y);
        last_second=second;
    }
}
