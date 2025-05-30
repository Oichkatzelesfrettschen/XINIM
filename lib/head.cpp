/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Minimal startup for the kernel image. */
#include <stddef.h>

extern void _main(void);
extern void _exit(int status);
extern void *_stackpt;
extern char endbss;

/* Symbols marking the start of each segment. */
char begtext;
char begdata;
char begbss;

/* Data area for the loader. */
long _data_org[] = {0xDADA, 0, 0, 0, 0, 0, 0, 0};

/* Break pointer initialized to the end of the bss. */
char *brksize = &endbss;

/* Stack limit used by the kernel. */
long sp_limit = 0;

void _start(void) {
    /* Load the stack pointer from _stackpt and enter main. */
    __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
    _main();
    for (;;) {
        /* Halt if main ever returns. */
    }
}
