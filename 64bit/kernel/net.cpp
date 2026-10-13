// Intel 8254x legacy descriptors; BSP owns the polling network stack.
#include "net.h"
#include "io.h"
#include "pit.h"
#include "vga.h"
#include "serial.h"
#include <stddef.h>
struct __attribute__((packed)) Rx { uint64_t address; uint16_t length, checksum; uint8_t status, errors; uint16_t special; };
struct __attribute__((packed)) Tx { uint64_t address; uint16_t length; uint8_t offset, command, status, start; uint16_t special; };
static volatile Rx rx[16] __attribute__((aligned(128)));
static volatile Tx tx[16] __attribute__((aligned(128)));
static uint8_t rbuf[16][2048] __attribute__((aligned(16))), tbuf[16][2048] __attribute__((aligned(16)));
static volatile uint32_t* regs;
static unsigned ri, ti;
static uint8_t mac[6], peer_mac[6];
static constexpr uint32_t local=0x0a00020f, gateway=0x0a000202;
static uint32_t cached_ip, wanted_ip;
static uint64_t received, sent, rejected;
static uint16_t identifier, ping_id;
static bool ping_reply, reply, connecting, established, reset;
static uint32_t remote_ip, seq, ack;
static uint16_t remote_port, source_port=40000;
static uint8_t response[513];static size_t response_size;
static uint16_t get16(const uint8_t* p){return uint16_t(p[0])<<8|p[1];}
static uint32_t get32(const uint8_t* p){return uint32_t(get16(p))<<16|get16(p+2);}
static void put16(uint8_t* p,uint16_t v){p[0]=v>>8;p[1]=v;}
static void put32(uint8_t* p,uint32_t v){put16(p,v>>16);put16(p+2,v);}
static void copy(void* dst,const void* src,size_t n){auto*d=(uint8_t*)dst;auto*s=(const uint8_t*)src;for(size_t i=0;i<n;++i)d[i]=s[i];}
static uint32_t sum(const uint8_t* p,size_t n,uint32_t s=0){while(n>=2){s+=get16(p);p+=2;n-=2;}if(n)s+=uint16_t(*p)<<8;return s;}
static uint16_t finish(uint32_t s){while(s>>16)s=(s&65535)+(s>>16);return ~s;}
static uint16_t transport_sum(uint32_t src,uint32_t dst,uint8_t proto,const uint8_t* p,size_t n){return finish(sum(p,n,(src>>16)+(src&65535)+(dst>>16)+(dst&65535)+proto+n));}
static uint32_t pci(unsigned b,unsigned d,unsigned off){outl(0xcf8,0x80000000|(b<<16)|(d<<11)|off);return inl(0xcfc);}
static bool transmit(const uint8_t* data,size_t n) {
    if(!regs||n>1514||!(tx[ti].status&1))return false;
    copy(tbuf[ti],data,n);for(size_t i=n;i<60;++i)tbuf[ti][i]=0;
    tx[ti].length=n<60?60:n;tx[ti].command=0x0b;tx[ti].status=0;
    asm volatile("mfence":::"memory");ti=(ti+1)%16;regs[0x3818/4]=ti;++sent;return true;
}
static void ethernet(uint8_t* p,const uint8_t* dest,uint16_t type){copy(p,dest,6);copy(p+6,mac,6);put16(p+12,type);}
static void arp(uint32_t target,const uint8_t* dest,bool answer) {
    uint8_t p[42]={};ethernet(p,dest,0x806);put16(p+14,1);put16(p+16,0x800);p[18]=6;p[19]=4;put16(p+20,answer?2:1);
    copy(p+22,mac,6);put32(p+28,local);if(answer)copy(p+32,dest,6);put32(p+38,target);transmit(p,sizeof(p));
}
static bool resolve(uint32_t ip) {
    uint32_t target=(ip&0xffffff00)==(local&0xffffff00)?ip:gateway;
    if(cached_ip==target)return true;
    wanted_ip=target;uint8_t broadcast[6]={255,255,255,255,255,255};
    for(unsigned attempt=0;attempt<3;++attempt) {
        arp(target,broadcast,false);uint64_t end=pit::ticks()+100;
        while(pit::ticks()<end){net::poll();if(cached_ip==target)return true;asm volatile("hlt");}
    }
    return false;
}
static bool ip_send(uint32_t dest,uint8_t protocol,const uint8_t* payload,size_t n) {
    if(n>1480||cached_ip!=((dest&0xffffff00)==(local&0xffffff00)?dest:gateway))return false;
    uint8_t p[1514]={};ethernet(p,peer_mac,0x800);p[14]=0x45;put16(p+16,n+20);put16(p+18,++identifier);put16(p+20,0x4000);p[22]=64;p[23]=protocol;
    put32(p+26,local);put32(p+30,dest);put16(p+24,finish(sum(p+14,20)));copy(p+34,payload,n);return transmit(p,n+34);
}
static bool tcp_send(uint8_t flags,const uint8_t* data=nullptr,size_t n=0) {
    uint8_t p[532]={};if(n>512)return false;
    put16(p,source_port);put16(p+2,remote_port);put32(p+4,seq);put32(p+8,ack);p[12]=0x50;p[13]=flags;put16(p+14,4096);
    if(n)copy(p+20,data,n);
    put16(p+16,transport_sum(local,remote_ip,6,p,n+20));return ip_send(remote_ip,6,p,n+20);
}
static void packet(const uint8_t* p,size_t n) {
    if(n<14)return;
    if(get16(p+12)==0x806) {
        if(n<42||get16(p+14)!=1||get16(p+16)!=0x800||p[18]!=6||p[19]!=4||get32(p+38)!=local)return;
        uint16_t op=get16(p+20);uint32_t sender=get32(p+28);
        if(op==2&&sender==wanted_ip){copy(peer_mac,p+22,6);cached_ip=sender;}
        if(op==1)arp(sender,p+22,true);
        return;
    }
    if(get16(p+12)!=0x800||n<34)return;
    const uint8_t* ip=p+14;unsigned h=(ip[0]&15)*4;unsigned total=get16(ip+2);
    if(ip[0]>>4!=4||h<20||h>n-14||total<h||total>n-14||finish(sum(ip,h))||get16(ip+6)&0x3fff||get32(ip+16)!=local){++rejected;return;}
    uint32_t src=get32(ip+12);const uint8_t* data=ip+h;size_t length=total-h;
    if(ip[9]==1) {
        if(length<8||finish(sum(data,length)))return;
        if(data[0]==0&&data[1]==0&&src==remote_ip&&get16(data+4)==ping_id&&get16(data+6)==1)ping_reply=true;
        if(data[0]==8&&data[1]==0&&length<=512) {
            uint8_t echo[512];copy(echo,data,length);echo[0]=0;echo[2]=echo[3]=0;put16(echo+2,finish(sum(echo,length)));
            // The received Ethernet source is the next hop for this reply.
            uint8_t saved[6];copy(saved,peer_mac,6);uint32_t saved_ip=cached_ip;
            copy(peer_mac,p+6,6);cached_ip=(src&0xffffff00)==(local&0xffffff00)?src:gateway;ip_send(src,1,echo,length);
            copy(peer_mac,saved,6);cached_ip=saved_ip;
        }
    } else if(ip[9]==17) {
        if(length<8||get16(data+4)<8||get16(data+4)>length)return;
        length=get16(data+4);if(get16(data+6)&&transport_sum(src,local,17,data,length))return;
        if(src==remote_ip&&get16(data)==remote_port&&get16(data+2)==source_port) {
            response_size=length-8;if(response_size>512)response_size=512;copy(response,data+8,response_size);reply=true;
        }
    } else if(ip[9]==6) {
        if(length<20||src!=remote_ip||get16(data)!=remote_port||get16(data+2)!=source_port||transport_sum(src,local,6,data,length))return;
        unsigned offset=(data[12]>>4)*4;if(offset<20||offset>length)return;
        uint8_t flags=data[13];uint32_t their_seq=get32(data+4),their_ack=get32(data+8);
        if(flags&4){reset=true;return;}
        if(connecting && (flags&0x12)==0x12 && their_ack==seq+1) {++seq;ack=their_seq+1;connecting=false;established=true;tcp_send(0x10);return;}
        if(!established||!(flags&0x10)||their_ack!=seq)return;
        size_t bytes=length-offset;
        if(their_seq!=ack){tcp_send(0x10);return;}
        if(bytes) {response_size=bytes>512?512:bytes;copy(response,data+offset,response_size);ack+=bytes;reply=true;}
        if(flags&1)++ack;
        if(bytes||(flags&1))tcp_send(0x10);
    }
}
namespace net {
    bool init() {
        for(unsigned b=0;b<256&&!regs;++b)for(unsigned d=0;d<32;++d) {
            if(pci(b,d,0)!=0x100e8086)continue;
            uint32_t bar=pci(b,d,0x10);if((bar&7)||!(bar&~15u))continue;
            uint32_t command=pci(b,d,4);outl(0xcf8,0x80000000|(b<<16)|(d<<11)|4);outl(0xcfc,(command&65535)|6);
            regs=(volatile uint32_t*)(uintptr_t)(bar&~15u);break;
        }
        if(!regs)return false;
        regs[0xd8/4]=0xffffffff;regs[0x100/4]=0;regs[0x400/4]=0;
        uint32_t lo=regs[0x5400/4],hi=regs[0x5404/4];for(unsigned i=0;i<4;++i)mac[i]=lo>>(i*8);mac[4]=hi;mac[5]=hi>>8;
        regs[0x5404/4]=hi|0x80000000;regs[0]|=1<<6;
        for(unsigned i=0;i<128;++i)regs[(0x5200/4)+i]=0;
        for(unsigned i=0;i<16;++i){rx[i].address=(uintptr_t)rbuf[i];tx[i].address=(uintptr_t)tbuf[i];tx[i].status=1;}
        regs[0x2800/4]=(uintptr_t)rx;regs[0x2804/4]=0;regs[0x2808/4]=sizeof(rx);regs[0x2810/4]=0;regs[0x2818/4]=15;
        regs[0x3800/4]=(uintptr_t)tx;regs[0x3804/4]=0;regs[0x3808/4]=sizeof(tx);regs[0x3810/4]=0;regs[0x3818/4]=0;
        regs[0x410/4]=0x0060200a;regs[0x400/4]=(1<<1)|(1<<3)|(16<<4)|(64<<12);
        regs[0x100/4]=(1<<1)|(1<<15)|(1<<26);serial::print("[NET] E1000 ready 10.0.2.15\n");return true;
    }
    void poll() {
        if(!regs)return;
        for(unsigned budget=0;budget<16&&(rx[ri].status&1);++budget) {
            asm volatile("lfence":::"memory");unsigned n=rx[ri].length;
            if((rx[ri].status&2)&&!rx[ri].errors&&n<=2048){++received;packet(rbuf[ri],n);}else ++rejected;
            rx[ri].status=0;asm volatile("mfence":::"memory");regs[0x2818/4]=ri;ri=(ri+1)%16;
        }
    }
    void stats(){vga::print(regs?"E1000: 10.0.2.15/24 gateway 10.0.2.2\n":"E1000: unavailable\n");vga::print("RX ");vga::print_dec(received);vga::print(" TX ");vga::print_dec(sent);vga::print(" dropped ");vga::print_dec(rejected);vga::putchar('\n');}
}
static bool address(const char*& s,uint32_t& out) {
    out=0;for(unsigned i=0;i<4;++i){unsigned v=0,digits=0;while(*s>='0'&&*s<='9'){v=v*10+(*s++-'0');if(++digits>3||v>255)return false;}if(!digits)return false;out=(out<<8)|v;if(i<3&&*s++!='.')return false;}
    return *s==0||*s==' ';
}
static bool wait_flag(bool& flag,unsigned ticks) {uint64_t end=pit::ticks()+ticks;while(pit::ticks()<end){net::poll();if(flag)return true;if(reset)return false;asm volatile("hlt");}return false;}
namespace net {
    void ping(const char* input) {
        uint32_t dest;if(!address(input,dest)||*input){vga::print("Usage: ping A.B.C.D\n");return;}
        if(!regs||!resolve(dest)){vga::print("ping: no interface / ARP timeout\n");return;}
        remote_ip=dest;ping_reply=false;reset=false;ping_id=++identifier;
        uint8_t p[16]={8,0};put16(p+4,ping_id);put16(p+6,1);copy(p+8,"IVSEPING",8);put16(p+2,finish(sum(p,sizeof(p))));
        uint64_t start=pit::ticks();ip_send(dest,1,p,sizeof(p));
        if(wait_flag(ping_reply,300)){vga::print("ping: reply, time <= ");vga::print_dec((pit::ticks()-start+1)*10);vga::print(" ms\n");}
        else vga::print("ping: timeout\n");
    }
    void exchange(const char* args,bool tcp) {
        uint32_t dest;if(!address(args,dest)||*args++!=' '){vga::print("Usage: udp/tcp IP PORT TEXT\n");return;}
        unsigned port=0,digits=0;while(*args>='0'&&*args<='9'){port=port*10+(*args++-'0');if(++digits>5||port>65535)break;}
        if(!digits||!port||port>65535||*args++!=' '){vga::print("Invalid port\n");return;}
        size_t n=0;while(args[n]&&n<256)++n;if(!n||n==256){vga::print("Payload must be 1..255 bytes\n");return;}
        if(!regs||!resolve(dest)){vga::print("network: no interface / ARP timeout\n");return;}
        remote_ip=dest;remote_port=port;source_port=40000+(++identifier%20000);reply=reset=established=connecting=false;response_size=0;
        if(tcp) {
            seq=uint32_t(pit::ticks()*1000);ack=0;connecting=true;
            for(unsigned retry=0;retry<3&&!established&&!reset;++retry){tcp_send(2);wait_flag(established,100);}
            if(!established){connecting=false;vga::print("tcp: connect failed\n");return;}
            tcp_send(0x18,(const uint8_t*)args,n);seq+=n;
        } else {
            uint8_t p[264]={};put16(p,source_port);put16(p+2,port);put16(p+4,n+8);copy(p+8,args,n);
            uint16_t checksum=transport_sum(local,dest,17,p,n+8);put16(p+6,checksum?checksum:65535);ip_send(dest,17,p,n+8);
        }
        if(wait_flag(reply,300)){vga::print(tcp?"tcp: reply ":"udp: reply ");for(size_t i=0;i<response_size;++i)vga::putchar(response[i]>=32&&response[i]<127?response[i]:'.');vga::putchar('\n');}
        else vga::print(tcp?"tcp: timeout/reset\n":"udp: timeout\n");
        // Diagnostic one-request client: explicitly abort after the reply.
        if(tcp&&established)tcp_send(0x14);
        established=connecting=false;
    }
}
