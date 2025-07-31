/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Simple linear congruential random number generator. */

/* Seed value used by rand(). */
static long seed = 1L;

/* Return a pseudo-random integer in the range [0, 32767]. */
int rand(void) {
    seed = (1103515245L * seed + 12345) & 0x7FFFFFFF;
    return (int)(seed & 077777);
}
