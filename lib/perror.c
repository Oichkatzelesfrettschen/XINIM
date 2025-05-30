/*
 * Custom perror implementation used by Minix.
 *
 * The function prints a user supplied prefix followed by the textual
 * description of the current errno value.  Messages are written directly
 * to file descriptor 2 in order to avoid using stdio.
 */

#include "../h/error.h"
#include <unistd.h>

/* Current errno value provided by the system. */
extern int errno;

/* Table mapping errno values to messages. */
char *error_message[NERROR + 1] = {
    "Error 0",
    "Not owner",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Arg list too long",
    "Exec format error",
    "Bad file number",
    "No children",
    "No more processes",
    "Not enough core",
    "Permission denied",
    "Bad address",
    "Block device required",
    "Mount device busy",
    "File exists",
    "Cross-device link",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Invalid argument",
    "File table overflow",
    "Too many open files",
    "Not a typewriter",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",
    "Too many links",
    "Broken pipe",
    "Math argument",
    "Result too large",
};

/*
 * Simple strlen replacement to keep dependencies minimal.
 */
static int slen(const char *s) {
    int k = 0;
    while (*s++) {
        k++;
    }
    return k;
}

/*
 * Print the error message corresponding to errno.
 */
void perror(const char *s) {
    if (errno < 0 || errno > NERROR) {
        write(2, "Invalid errno\n", 14);
    } else {
        write(2, s, slen(s));
        write(2, ": ", 2);
        write(2, error_message[errno], slen(error_message[errno]));
        write(2, "\n", 1);
    }
}

static int slen(s)
char *s;
{
  int k = 0;

  while (*s++) k++;
  return(k);
}
