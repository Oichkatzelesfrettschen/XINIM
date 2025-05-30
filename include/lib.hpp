/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../h/callnr.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "defs.hpp"
#include <stddef.h>

extern message M;

#define MM 0
#define FS 1
extern int callm1(int proc, int syscallnr, int int1, int int2, int int3, char *ptr1, char *ptr2,
                  char *ptr3);
extern int callm3(int proc, int syscallnr, int int1, char *name);
extern int callx(int proc, int syscallnr);
extern int len(char *s);
extern int errno;
extern int begsig(void); /* interrupts all vector here */

/* Memory allocation wrappers */
void *safe_malloc(size_t size);
void safe_free(void *ptr);
