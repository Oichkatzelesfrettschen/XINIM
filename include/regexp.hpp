/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define void int
#define CHARBITS 0377
#define strchr index
#define NSUBEXP 10
typedef struct regexp {
    char *startp[NSUBEXP];
    char *endp[NSUBEXP];
    char regstart;   /* Internal use only. */
    char reganch;    /* Internal use only. */
    char *regmust;   /* Internal use only. */
    int regmlen;     /* Internal use only. */
    char program[1]; /* Unwarranted chumminess with compiler. */
} regexp;

extern regexp *regcomp(char *exp);
extern int regexec(regexp *prog, char *string, int bolflag);
extern void regsub(regexp *prog, char *source, char *dest);
extern void regerror(char *s);
