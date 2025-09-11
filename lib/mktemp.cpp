/* mktemp - make a name for a temporary file */
#include <unistd.h>

/**
 * @brief Generate a simple temporary filename in-place.
 *
 * @param templ Template string with trailing XXXXXX to replace.
 * @return The input pointer for convenience.
 * @sideeffects Modifies the contents of @p templ.
 * @thread_safety Safe if each thread uses a unique buffer.
 * @compat mktemp(3)
 * @example
 * char name[] = "tmpXXXXXX";
 * mktemp(name);
 */
char *mktemp(char *templ) {
    int pid;
    char *p;

    pid = getpid(); /* get process id as semi-unique number */
    p = templ;
    while (*p++)
        ; /* find end of string */
    p--;  /* backup to last character */

    /* Replace XXXXXX at end of template with pid. */
    while (*--p == 'X') {
        *p = '0' + (pid % 10);
        pid = pid / 10;
    }
    return templ;
}
