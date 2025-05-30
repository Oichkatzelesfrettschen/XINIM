/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Kernel startup for MINIX binaries. */
#include <stddef.h>

extern void _main(void);
extern void *_stackpt;
extern char endbss;

char begtext;
char begdata;
char begbss;

long _data_org[] = {0xDADA, 0, 0, 0, 0, 0, 0, 0};

char *brksize = &endbss;
long sp_limit = 0;

void _start(void) {
    __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
    _main();
    for (;;) {
    }
}
