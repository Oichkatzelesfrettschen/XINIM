[BITS 16]
[ORG 0x7C00]

; ------------------------------------------------------------------
; Minimal boot block for loading a 64-bit kernel
; ------------------------------------------------------------------
; This rewritten boot sector first loads the kernel from disk using
; BIOS services, then performs the transition from real mode to
; protected mode and finally long mode.  A single 2&nbsp;MiB identity
; mapped page is created so the kernel can be entered in 64&nbsp;bit mode.
; The number of sectors to read and the 32&nbsp;bit entry address of the
; kernel are patched in by the build utility.
; ------------------------------------------------------------------

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Load kernel at 0x100000 using BIOS INT 13h
    mov si, [sector_count]
    mov di, 0x1000            ; load segment 0x1000:0 => 0x10000
    mov bx, 0x0000
.load_loop:
    push si
    mov ah, 0x02
    mov al, 1                 ; one sector
    mov ch, byte [track]
    mov cl, byte [sector]
    mov dh, byte [head]
    mov dl, 0x00
    int 0x13
    jc disk_error
    pop si
    add word [sector], 1
    add bx, 512
    dec si
    jnz .load_loop

    ; Enable A20 (via port 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Set up GDT
    lgdt [gdt_desc]

    ; Enter protected mode
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
    mov dword [pdpt+4], 0
    mov dword [pd], 0x00000083   ; 2MiB page
    mov dword [pd+4], 0

    mov eax, pml4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5       ; PAE
    mov cr4, eax

    ; enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 0x80000001   ; PG | PE
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

; ------------------------------------------------------------------
; Data and tables
; ------------------------------------------------------------------
sector_count:   dw 0      ; patched by build
kernel_entry:   dd 0      ; patched by build (low 32 bits)
track:          db 0
sector:         db 2      ; first sector after the boot block
head:           db 0

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

; Pad and boot signature
times 510-($-$$) db 0
    dw 0xAA55
