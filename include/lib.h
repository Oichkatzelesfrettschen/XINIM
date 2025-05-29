#include "../h/const.h"
#include "../h/type.h"
#include "../h/error.h"
#include "../h/callnr.h"
#include <stddef.h>
#include <stdint.h>

extern message M;

#define MM                 0
#define FS                 1
extern int callm1(int proc, int syscallnr, int int1, int int2, int int3, char *ptr1, char *ptr2, char *ptr3);
extern int callm3(int proc, int syscallnr, int int1, char *name);
extern int callx(int proc, int syscallnr);
extern int len(char *s);
extern int errno;
extern int begsig(void);            /* interrupts all vector here */
