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
#ifdef __x86_64__
  __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
#elif defined(__aarch64__)
  __asm__ volatile("ldr x0, =_stackpt; ldr x0, [x0]; mov sp, x0" ::: "sp");
#else
  // Generic fallback - set stack pointer if supported
  volatile void* stack = _stackpt;
  (void)stack;
#endif
  _main();
  for (;;) {
  }
}
