/* Break size pointer used by sbrk/brk. */
extern char endbss;
char *_brksize = &endbss;
