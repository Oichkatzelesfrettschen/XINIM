[BITS 64]

%define SECTOR_SIZE 512

    global absread
    global abswrite
    global dmaoverr

    extern pread
    extern pwrite

; ssize_t absread(int fd, uint64_t sector, void *buf)
absread:
    push rbp
    mov rbp, rsp
    ; rdi = fd, rsi = sector, rdx = buf
    mov rcx, rsi            ; sector
    shl rcx, 9              ; multiply by 512
    mov rsi, rdx            ; buf pointer
    mov rdx, SECTOR_SIZE    ; count
    ; rdi already fd
    call pread
    cmp rax, SECTOR_SIZE
    jne .error
    xor eax, eax            ; return 0
    pop rbp
    ret
.error:
    mov eax, -1
    pop rbp
    ret

; ssize_t abswrite(int fd, uint64_t sector, const void *buf)
abswrite:
    push rbp
    mov rbp, rsp
    mov rcx, rsi            ; sector
    shl rcx, 9
    mov rsi, rdx            ; buf pointer
    mov rdx, SECTOR_SIZE
    ; rdi already fd
    call pwrite
    cmp rax, SECTOR_SIZE
    jne .error_w
    xor eax, eax
    pop rbp
    ret
.error_w:
    mov eax, -1
    pop rbp
    ret

dmaoverr:
    xor eax, eax
    ret
