[BITS 64]

    global _start
    global putc
    global getc
    global reset_diskette
    global diskio

    extern main
    extern exit
    extern putchar
    extern getchar
    extern pread
    extern pwrite
    extern lseek

%define SECTOR_SIZE 512

_start:
    xor edi, edi        ; argc = 0
    xor esi, esi        ; argv = NULL
    call main
    mov edi, eax
    jmp exit

; void putc(int c)
putc:
    mov edi, edi        ; char in dil -> rdi already
    call putchar
    ret

; int getc(void)
getc:
    call getchar
    movzx eax, al
    ret

; int reset_diskette(void) -- no-op
reset_diskette:
    xor eax, eax
    ret

; int diskio(int rw, unsigned long sector, void* buf, unsigned long count)
diskio:
    push rbp
    mov rbp, rsp
    ; args: rdi=rw, rsi=sector, rdx=buf, rcx=count
    mov r8, rdi         ; save rw
    mov r9, rcx         ; count
    mov r10, rdx        ; buffer pointer
    ; compute offset
    mov rax, rsi
    shl rax, 9
    mov rsi, rax        ; offset
    mov rdi, 0          ; fd 0 - rely on external open? not used
    ; disk file descriptor is global 'drive', expect to be in external variable
    ; we simply use drive as fd via external symbol
    extern drive_fd
    mov rdi, [rel drive_fd]
    mov rdx, r9
    imul rdx, SECTOR_SIZE
    cmp r8, 0
    je .readop
.writeop:
    mov rdi, [rel drive_fd]
    mov rsi, r10
    mov rdx, r9
    imul rdx, SECTOR_SIZE
    mov rcx, rax        ; offset
    call pwrite
    cmp rax, rdx
    jne .err
    xor eax, eax
    jmp .done
.readop:
    mov rdi, [rel drive_fd]
    mov rsi, r10
    mov rdx, r9
    imul rdx, SECTOR_SIZE
    mov rcx, rax        ; offset
    call pread
    cmp rax, rdx
    jne .err
    xor eax, eax
    jmp .done
.err:
    mov eax, -1
.done:
    pop rbp
    ret

section .data
    drive_fd dq 0
