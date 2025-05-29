[BITS 16]
[ORG 0x7C00]

; ----------------------------------------------------------------------
; Simple 64-bit boot block
; ----------------------------------------------------------------------
; This NASM style boot sector loads a 64-bit kernel using BIOS
; INT 13h extensions and then switches the CPU from real mode to
; long mode.  The build utility patches the sector count, the LBA
; of the kernel and the 64-bit kernel entry address.
; ----------------------------------------------------------------------

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; fill disk address packet
    mov ax, [sector_count]
    mov [dap+2], ax
    mov eax, dword [kernel_lba]
    mov [dap+8], eax
    mov eax, dword [kernel_lba+4]
    mov [dap+12], eax

    mov dl, [drive]
    mov si, dap
    mov ah, 0x42
    int 0x13
    jc disk_error

    ; enable A20 via port 0x92
    in al, 0x92
    or al, 2
    out 0x92, al

    ; set up GDT and enter protected mode
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pmode_start

[BITS 32]
pmode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000


    ; Set up 64-bit page tables (identity map first 2MiB)
    mov eax, pdpt
    or eax, 0x03
    mov dword [pml4], eax
    mov dword [pml4+4], 0
    mov eax, pd
    or eax, 0x03
    mov dword [pdpt], eax
    ; build minimal page tables mapping first 2MiB
    mov dword [pml4], pdpt + 0x03
    mov dword [pml4+4], 0
    mov dword [pdpt], pd + 0x03

    mov eax, pml4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5            ; PAE
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 0x80000001        ; PG|PE
    mov cr0, eax
    jmp 0x18:lmode_start

[BITS 64]
lmode_start:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x90000
    mov rax, [kernel_entry]
    jmp rax

disk_error:
    hlt

; ----------------------------------------------------------------------
; Data and tables
; ----------------------------------------------------------------------

kernel_load_addr equ 0x00100000

sector_count:   dw 0        ; patched by build
kernel_entry:   dq 0        ; patched by build
kernel_lba:     dq 2        ; start sector of kernel
drive:          db 0

align 4
; disk address packet for INT 13h extensions
; offset 0: size (16), offset 2: count, offset 4: buffer, offset 8: lba

dap:
    db 16,0
    dw 0
    dd kernel_load_addr
    dq 0

align 8
pml4:   dq 0
pdpt:   dq 0
pd:     dq 0

align 16
GDT:
    dq 0
    dq 0x00CF9A000000FFFF      ; 0x08 32-bit code
    dq 0x00CF92000000FFFF      ; 0x10 32-bit data
    dq 0x00A09A0000000000      ; 0x18 64-bit code
    dq 0x00A0920000000000      ; 0x20 64-bit data
GDT_end:

gdt_desc:
    dw GDT_end - GDT - 1
    dd GDT

; boot signature
    times 510-($-$$) db 0
    dw 0xAA55
