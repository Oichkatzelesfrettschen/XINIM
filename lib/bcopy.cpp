/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Copy a block of memory from src to dest. */
void bcopy(char *src, char *dest, int n) {
    // Copy a block of data byte by byte.
    while (n--)
        *dest++ = *src++;
}
