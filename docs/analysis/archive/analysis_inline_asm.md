# Analysis Inline Asm

```text
./kernel/syscall.cpp:40:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:38:    __asm__ volatile("push %%rax\n\t"
./kernel/mpx64.cpp:107:    __asm__ volatile("movq _proc_ptr(%%rip), %%r15\n\t"
./kernel/mpx64.cpp:141:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:152:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:163:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:174:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:189:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:200:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:213:    __asm__ volatile("call save\n\t"
./kernel/mpx64.cpp:224:    __asm__ volatile("call save\n\t"
./lib/minix/head.cpp:18:  __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
./lib/minix/crtso.cpp:15:  __asm__ volatile("mov %%rsp, %0" : "=r"(stack));
./lib/syscall_x86_64.cpp:14:    register long rax __asm__("rax") = 0;
./lib/syscall_x86_64.cpp:15:    register long rdi __asm__("rdi") = dst;
./lib/syscall_x86_64.cpp:16:    register message *rsi __asm__("rsi") = m_ptr;
./lib/syscall_x86_64.cpp:17:    register long rdx __asm__("rdx") = SEND;
./lib/syscall_x86_64.cpp:18:    __asm__ volatile("syscall"
./lib/syscall_x86_64.cpp:33:    register long rax __asm__("rax") = 0;
./lib/syscall_x86_64.cpp:34:    register long rdi __asm__("rdi") = src;
./lib/syscall_x86_64.cpp:35:    register message *rsi __asm__("rsi") = m_ptr;
./lib/syscall_x86_64.cpp:36:    register long rdx __asm__("rdx") = RECEIVE;
./lib/syscall_x86_64.cpp:37:    __asm__ volatile("syscall"
./lib/syscall_x86_64.cpp:52:    register long rax __asm__("rax") = 0;
./lib/syscall_x86_64.cpp:53:    register long rdi __asm__("rdi") = srcdest;
./lib/syscall_x86_64.cpp:54:    register message *rsi __asm__("rsi") = m_ptr;
./lib/syscall_x86_64.cpp:55:    register long rdx __asm__("rdx") = BOTH;
./lib/syscall_x86_64.cpp:56:    __asm__ volatile("syscall"
./lib/crt0.cpp:21:  __asm__ volatile("mov %%rsp, %0" : "=r"(stack));
./lib/head.cpp:25:    __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
./lib/crtso.cpp:15:  __asm__ volatile("mov %%rsp, %0" : "=r"(stack));
./crypto/kyber_impl/verify.c:51:  __asm__("" : "+r"(b) : /* no inputs */);

```
