.intel_syntax noprefix

.section .text

.globl save
.type save, @function
save:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    mov r15, [rip + proc_ptr]
    
    mov rax, [rsp + 120] 
    mov [r15 + 128], rax
    
    mov rax, [rsp + 136] 
    mov [r15 + 136], rax
    
    lea rax, [rsp + 120] 
    mov [r15 + 120], rax
    
    mov rax, [rsp + 112] 
    mov [r15 + 112], rax
    mov rax, [rsp + 104] 
    mov [r15 + 104], rax
    mov rax, [rsp + 96]  
    mov [r15 + 96], rax
    mov rax, [rsp + 88]  
    mov [r15 + 88], rax
    mov rax, [rsp + 80]  
    mov [r15 + 80], rax
    mov rax, [rsp + 72]  
    mov [r15 + 72], rax
    mov rax, [rsp + 64]  
    mov [r15 + 64], rax
    mov rax, [rsp + 56]  
    mov [r15 + 56], rax
    mov rax, [rsp + 48]  
    mov [r15 + 48], rax
    mov rax, [rsp + 40]  
    mov [r15 + 40], rax
    mov rax, [rsp + 32]  
    mov [r15 + 32], rax
    mov rax, [rsp + 24]  
    mov [r15 + 24], rax
    mov rax, [rsp + 16]  
    mov [r15 + 16], rax
    mov rax, [rsp + 8]   
    mov [r15 + 8], rax
    mov rax, [rsp]       
    mov [r15 + 0], rax
    
    lea rsp, [rip + k_stack + 8192] 
    ret

.globl restart
.type restart, @function
restart:
    mov r15, [rip + proc_ptr]
    
    mov rsp, [r15 + 120] 
    
    mov r15, [rip + proc_ptr]
    mov r14, [r15 + 104]
    mov r13, [r15 + 96]
    mov r12, [r15 + 88]
    mov r11, [r15 + 80]
    mov r10, [r15 + 72]
    mov r9,  [r15 + 64]
    mov r8,  [r15 + 56]
    mov rbp, [r15 + 48]
    mov rdi, [r15 + 40]
    mov rsi, [r15 + 32]
    mov rdx, [r15 + 24]
    mov rcx, [r15 + 16]
    mov rbx, [r15 + 8]
    mov rax, [r15 + 0]
    
    push [r15 + 136] 
    push [r15 + 128] 
    
    mov r15, [r15 + 112] 
    
    iretq

.globl isr_default
.type isr_default, @function
isr_default:
    call save
    call surprise
    jmp restart

.globl isr_clock
.type isr_clock, @function
isr_clock:
    call save
    call clock_int
    jmp restart

.globl isr_keyboard
.type isr_keyboard, @function
isr_keyboard:
    call save
    call tty_int
    jmp restart

.globl s_call
.type s_call, @function
s_call:
    call save
    mov rdi, [rip + proc_ptr]
    mov rsi, [rdi + 16] 
    mov rdx, [rdi + 0]  
    mov rcx, 0
    call sys_call
    jmp restart

.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    call save
    mov rdi, 3 
    mov esi, [rip + cur_proc]
    mov edx, 116 # ANY = NR_PROCS(16) + 100
    mov rcx, [rip + proc_ptr]
    call sys_call
    jmp restart

.globl lpr_int
.type lpr_int, @function
lpr_int:
    call save
    call pr_char
    jmp restart

.globl disk_int
.type disk_int, @function
disk_int:
    call save
    mov rdi, -6 
    lea rsi, [rip + int_mess]
    call interrupt
    jmp restart

.globl divide
.type divide, @function
divide:
    call save
    call div_trap
    jmp restart

.globl trp
.type trp, @function
trp:
    call save
    call trap
    jmp restart
