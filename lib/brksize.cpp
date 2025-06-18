/* Break size pointer used by sbrk/brk. */
extern char endbss;
char *brksize = &endbss;
