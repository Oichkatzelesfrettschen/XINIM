/* perror(s) print the current error message. */

#include "../h/error.h"
#include <unistd.h>

/* Global errno provided by the C library. */
extern int errno;

/* Return the length of a string without relying on strlen(). */
static int slen(const char *s);

/* Error message strings corresponding to errno values. */
/* Table mapping errno values to human readable messages. */

const char *error_message[NERROR + 1] = {"Error 0",

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

                                         "Result too large"

};

/*
 * Print the supplied message followed by the text representation of errno.
 * The output is
 * written directly to file descriptor 2.
 */
void perror(const char *s) {
    if (errno < 0 || errno > NERROR) {
        /* errno outside the valid range */
        write(2, "Invalid errno\n", 14);
    } else {
        write(2, s, slen(s));
        write(2, ": ", 2);
        write(2, error_message[errno], slen(error_message[errno]));
        write(2, "\n", 1);
    }
}

/* Simple strlen replacement used by perror. */
static int slen(const char *s) {
    int k = 0;
    while (*s++)
        k++;
    return k;
    "No space left on device",
        "Illegal seek",
        "Read-only file system",

        "Too many links",

        "Broken pipe",

        "Math argument",

        "Result too large"
};

perror(s) char *s;
    if (errno < 0 || errno > NERROR) {
        write(2, "Invalid errno\n", 14);
    } else {
        write(2, s, slen(s));
        write(2, ": ", 2);
        write(2, error_message[errno], slen(error_message[errno]));
        write(2, "\n", 1);
    }
    int k = 0;
    while (*s++)
        k++;
    return (k);
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
