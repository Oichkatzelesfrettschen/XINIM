/* Copy a block of memory */
void bcopy(char *old, char *dest, int n) {
    /* Copy a block of data. */

    while (n--)
        *dest++ = *old++;
}
