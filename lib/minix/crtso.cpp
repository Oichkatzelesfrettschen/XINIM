/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Shared startup for MINIX binaries. */
#include <stddef.h>

extern int _main(int argc, char **argv, char **envp);
extern void _exit(int status);

char **_environ;

void _start(void) {
    char **stack;
    int argc;
    char **argv;
    char **envp;

    __asm__ volatile("mov %%rsp, %0" : "=r"(stack));
    argc = (int)(long)stack[0];
    argv = &stack[1];
    envp = &argv[argc + 1];
    _environ = envp;

    _exit(_main(argc, argv, envp));
}
