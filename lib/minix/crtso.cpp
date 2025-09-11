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

#ifdef __x86_64__
  __asm__ volatile("mov %%rsp, %0" : "=r"(stack));
#elif defined(__aarch64__)
  __asm__ volatile("mov %0, sp" : "=r"(stack));
#else
  // Generic fallback - assume stack pointer is passed somehow
  stack = nullptr;
#endif

  if (stack) {
    argc = (int)(long)stack[0];
    argv = &stack[1];
    envp = &argv[argc + 1];
    _environ = envp;
    _exit(_main(argc, argv, envp));
  } else {
    // Fallback with no args
    _exit(_main(0, nullptr, nullptr));
  }
}
