#include "../include/lib.hpp" // C++23 header

// Helper used when no arguments or environment pointers are passed.
static char *null_argv[] = {nullptr};

// Forward declaration for the main exec implementation.
int execve(const char *name, char *argv[], char *envp[]);

// Execute a file with a single argument list terminated by a null pointer.
int execl(const char *name, char *arg0) { return execve(name, &arg0, null_argv); }

// Execute a file with the provided argument list followed by the environment
// pointers located after the terminating null of the argument list.
int execle(const char *name, char *argv) {
    char **p = &argv;
    while (*p++)
        ;
    char **envp = reinterpret_cast<char **>(*p);
    return execve(name, &argv, envp);
}

// Execute a file using the argument vector 'argv'.
int execv(const char *name, char *argv[]) { return execve(name, argv, null_argv); }

// Execute a file using both argument and environment vectors.
int execve(const char *name, char *argv[], char *envp[]) {
    char stack[MAX_ISTACK_BYTES];
    char **argorg, **envorg, *hp, **ap, *p;
    int i, nargs, nenvps, stackbytes, ptrsize, offset;

    /* Count the argument pointers and environment pointers. */
    nargs = 0;
    nenvps = 0;
    argorg = argv;
    envorg = envp;
    while (*argorg++ != NIL_PTR)
        nargs++;
    while (*envorg++ != NIL_PTR)
        nenvps++;
    ptrsize = sizeof(NIL_PTR);

    /* Prepare to set up the initial stack. */
    hp = &stack[(nargs + nenvps + 3) * ptrsize];
    if (hp + nargs + nenvps >= &stack[MAX_ISTACK_BYTES])
        return static_cast<int>(ErrorCode::E2BIG);
    ap = (char **)stack;
    *ap++ = (char *)nargs;

    /* Prepare the argument pointers and strings. */
    for (i = 0; i < nargs; i++) {
        offset = hp - stack;
        *ap++ = (char *)offset;
        p = *argv++;
        while (*p) {
            *hp++ = *p++;
            if (hp >= &stack[MAX_ISTACK_BYTES])
                return static_cast<int>(ErrorCode::E2BIG);
        }
        *hp++ = (char)0;
    }
    *ap++ = NIL_PTR;

    /* Prepare the environment pointers and strings. */
    for (i = 0; i < nenvps; i++) {
        offset = hp - stack;
        *ap++ = (char *)offset;
        p = *envp++;
        while (*p) {
            *hp++ = *p++;
            if (hp >= &stack[MAX_ISTACK_BYTES])
                return static_cast<int>(ErrorCode::E2BIG);
        }
        *hp++ = (char)0;
    }
    *ap++ = NIL_PTR;
    stackbytes = (((hp - stack) + ptrsize - 1) / ptrsize) * ptrsize;
    return callm1(MM_PROC_NR, EXEC, len(const_cast<char *>(name)), stackbytes, 0,
                  const_cast<char *>(name), stack, NIL_PTR);
}

// Optimized EXEC when there are no arguments or environment strings.
int execn(const char *name) {
    /* Special version used when there are no args and no environment.  This call
     * is principally used by INIT, to avoid having to allocate MAX_ISTACK_BYTES.
     */

    char stack[4];

    stack[0] = 0;
    stack[1] = 0;
    stack[2] = 0;
    stack[3] = 0;
    return callm1(MM_PROC_NR, EXEC, len(const_cast<char *>(name)), 4, 0, const_cast<char *>(name),
                  stack, NIL_PTR);
}
