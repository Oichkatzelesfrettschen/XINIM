#include <cassert> // for assert

/* Prototype for strlen from the Minix library. */
int strlen(char *s);
/* Prototype for strcmp from the Minix library. */
int strcmp(char *s1, char *s2);
/* Prototype for rand from the Minix library. */
int rand(void);

/* Verify basic behavior of strlen, strcmp and rand. */
int main(void) {
    /* strlen should count the characters in the string. */
    // Cast away const to match legacy prototypes.
    assert(strlen(const_cast<char *>("hello")) == 5);
    /* strcmp should report equality for identical strings. */
    // Legacy strcmp also expects mutable pointers.
    assert(strcmp(const_cast<char *>("a"), const_cast<char *>("a")) == 0);
    /* rand should produce a non-negative value. */
    int r = rand();
    assert(r >= 0);
    return 0;
}
