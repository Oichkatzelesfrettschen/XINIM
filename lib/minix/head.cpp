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
