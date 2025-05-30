/* Shared startup for MINIX binaries. */
#include <cstddef>

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
