#ifndef SETJMP_H
#define SETJMP_H

/*
 * Minimal jmp_buf representation used by the legacy Minix sources.
 * The buffer merely stores a pointer to the host jmp_buf allocated at
 * runtime in the wrapper implementation.
 */
#define _JBLEN 3
typedef int jmp_buf[_JBLEN];

/* Prototypes for the host provided setjmp/longjmp functions. */
int setjmp(void *env);
void longjmp(void *env, int val);

#endif /* SETJMP_H */
