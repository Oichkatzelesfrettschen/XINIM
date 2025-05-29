# Minimal 64-bit implementations of low level kernel routines.
# Converted from original 16-bit klib88.s

.globl _phys_copy, _cp_mess, _port_out, _port_in, _portw_out, _portw_in
.globl _lock, _unlock, _restore, _build_sig, _get_chrome, _vid_copy
.globl _get_byte, _reboot, _wreboot

.text

# void phys_copy(void *src, void *dst, long bytes)
_phys_copy:
    mov %rdi, %rsi      # source in RSI
    mov %rsi, %rdi      # destination in RDI
    mov %rdx, %rcx      # count in RCX
    rep movsb           # copy bytes
    ret

# void cp_mess(int src, void *src_click, void *src_off, void *dst_click, void *dst_off)
_cp_mess:
    mov %r8, %rdi       # destination pointer
    movl %edi, (%rdi)   # store sender process number
    mov %rdx, %rsi      # source pointer
    lea 4(%rdi), %rdi   # skip m_source
    lea 4(%rsi), %rsi
    mov $(MESS_SIZE - 4), %rcx
    rep movsb
    ret

# void port_out(unsigned port, unsigned val)
_port_out:
    mov %edi, %edx
    mov %esi, %eax
    out %al, (%dx)
    ret

# void port_in(unsigned port, unsigned *val)
_port_in:
    mov %edi, %edx
    in (%dx), %al
    mov %al, (%rsi)
    ret

# void portw_out(unsigned port, unsigned val)
_portw_out:
    mov %edi, %edx
    mov %esi, %ax
    out %ax, (%dx)
    ret

# void portw_in(unsigned port, unsigned *val)
_portw_in:
    mov %edi, %edx
    in (%dx), %ax
    mov %ax, (%rsi)
    ret

# void lock(void)
_lock:
    pushfq
    cli
    popq lockvar(%rip)
    ret

# void unlock(void)
_unlock:
    sti
    ret

# void restore(void)
_restore:
    pushq lockvar(%rip)
    popfq
    ret

# build_sig(struct sig_stuff *dst, struct proc *rp, int sig)
_build_sig:
    movl %edx, (%rdi)
    movq PC(%rsi), %rax
    movq %rax, 8(%rdi)
    movq PSW(%rsi), %rax
    movq %rax, 16(%rdi)
    ret

# int get_chrome(void)
_get_chrome:
    xor %eax, %eax
    ret

# void vid_copy(void *buf, unsigned base, unsigned off, unsigned words)
_vid_copy:
    mov %rdi, %rcx
    test %rcx, %rcx
    jz 1f
    mov %r8, %rdx
    mov %rdx, %rcx
    mov %rdx, %rsi
    mov %rcx, %rdi
    rep movsw
1:
    ret

# unsigned char get_byte(unsigned seg, unsigned off)
_get_byte:
    mov %rsi, %rax
    movzbl (%rax), %eax
    ret

_reboot:
    hlt
    ret

_wreboot:
    hlt
    ret

.data
lockvar: .quad 0
splimit: .quad 0

