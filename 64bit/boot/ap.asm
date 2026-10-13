org 0x8000
bits 16
    cli
    xor ax,ax
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7c00
    lgdt [gdtr]
    mov eax,cr0
    or eax,1
    mov cr0,eax
    jmp 0x08:protected
bits 32
protected:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov eax,cr4
    or eax,0x20
    mov cr4,eax
    mov eax,[0x7000]
    mov cr3,eax
    mov ecx,0xc0000080
    rdmsr
    or eax,0x900
    wrmsr
    mov eax,cr0
    or eax,0x80010000
    mov cr0,eax
    jmp 0x18:long_mode
bits 64
long_mode:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov rsp,[abs 0x7008]
    mov rdi,[abs 0x7018]
    mov rax,[abs 0x7010]
    call rax
.halt:
    cli
    hlt
    jmp .halt
align 8
gdt:
    dq 0,0x00cf9a000000ffff,0x00cf92000000ffff,0x00af9a000000ffff
gdtr:
    dw 31
    dd gdt
