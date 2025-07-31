/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Shared object startup code. */
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
