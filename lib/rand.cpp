/* Simple linear congruential random number generator. */

/* Seed value used by rand(). */
static long seed = 1L;

/* Return a pseudo-random integer in the range [0, 32767]. */
int rand(void) {
    seed = (1103515245L * seed + 12345) & 0x7FFFFFFF;
    return (int)(seed & 077777);
}
