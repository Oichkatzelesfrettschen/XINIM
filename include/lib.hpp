/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#include "../h/callnr.h"
#include "../h/const.h"
#include "../h/error.h"
#include "../h/type.h"
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
