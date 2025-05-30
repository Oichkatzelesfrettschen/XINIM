/* Copy a block of memory from src to dest. */
void bcopy(char *src, char *dest, int n) {
    // Copy a block of data byte by byte.
    while (n--)
        *dest++ = *src++;
}
