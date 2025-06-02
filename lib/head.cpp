/* Minimal startup for the kernel image. */
#include <stddef.h>

extern void main(void);
extern void exit(int status);
extern void *stackpt;
extern char endbss;

/* Symbols marking the start of each segment. */
char begtext;
char begdata;
char begbss;

/* Data area for the loader. */
long data_org[] = {0xDADA, 0, 0, 0, 0, 0, 0, 0};

/* Break pointer initialized to the end of the bss. */
char *brksize = &endbss;

/* Stack limit used by the kernel. */
long sp_limit = 0;

void start(void) {
  /* Load the stack pointer from _stackpt and enter main. */
  __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
  main();
  for (;;) {
    /* Halt if main ever returns. */
  }
}
