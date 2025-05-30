/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Entry point for standalone programs.
 * This function extracts argc, argv, and envp from the stack
 * and then invokes _main. It finally terminates by calling _exit.
 */
#include <stddef.h>

extern int _main(int argc, char **argv, char **envp);
extern void _exit(int status);

/* Environment pointer accessible to the program. */
char **_environ;

/* Starting point referenced by the linker. */
void _start(void) {
    char **stack;
    int argc;
    char **argv;
    char **envp;

    /* Capture the initial stack pointer. */
    __asm__ volatile("mov %%rsp, %0" : "=r"(stack));

    /* Argument count is stored at stack[0]. */
    argc = (int)(long)stack[0];
    /* Argument vector immediately follows argc. */
    argv = &stack[1];
    /* Environment pointer after argv[argc] and terminating NULL. */
    envp = &argv[argc + 1];
    _environ = envp;

    _exit(_main(argc, argv, envp));
}
