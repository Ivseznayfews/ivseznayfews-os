MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x7
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    ; Address fields are unused for ELF but retain their specified offsets.
    dd 0, 0, 0, 0, 0
    dd 0, 1024, 768, 32       ; linear graphics mode

section .bss
align 4096
pml4:           resb 4096
pdpt:           resb 4096
pd:             resb 16384
align 16
stack_bottom:
    resb 16384
stack_top:

section .data
align 8
mb_magic:   dd 0
mb_info:    dd 0

section .rodata
align 8
gdt64:
    dq 0
gdt64_code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
gdt64_data: equ $ - gdt64
    dq (1<<41) | (1<<44) | (1<<47)
gdt64_pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .text
global _start
extern kmain
extern __bss_start
extern __bss_end

bits 32
_start:
    cli
    mov esp, stack_top
    mov [mb_magic], eax
    mov [mb_info], ebx

    ; 清 .bss —— 关键修复！
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    ; 建立 PML4[0] -> PDPT
    mov eax, pdpt
    or eax, 0x03
    mov dword [pml4], eax
    mov dword [pml4 + 4], 0

    ; Four page directories identity-map 4 GB, including PCI framebuffer MMIO.
    xor ecx, ecx
.map_pdpt:
    mov eax, ecx
    shl eax, 12
    add eax, pd
    or eax, 0x03
    mov [pdpt + ecx*8], eax
    mov dword [pdpt + ecx*8 + 4], 0
    inc ecx
    cmp ecx, 4
    jne .map_pdpt

    ; PMM still allocates only below 1 GB; upper mappings are uncached MMIO.
    mov ecx, 0
.map_pd:
    mov eax, 0x200000
    mul ecx
    or eax, 0x83
    cmp ecx, 512
    jb .store_pde
    or eax, 0x18
.store_pde:
    mov dword [pd + ecx*8], eax
    mov dword [pd + ecx*8 + 4], 0
    inc ecx
    cmp ecx, 2048
    jne .map_pd

    mov eax, pml4
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    mov eax, cr0
    or eax, (1 << 31) | (1 << 16)
    mov cr0, eax

    lgdt [gdt64_pointer]
    jmp 0x08:.long_mode

bits 64
.long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top

    mov edi, [rel mb_magic]
    mov esi, [rel mb_info]
    call kmain

.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
