/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

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
