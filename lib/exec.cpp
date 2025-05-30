#include "../include/lib.hpp" // C++17 header

char *nullptr[1]; /* the EXEC calls need a zero pointer */

// Execute a program with a list of arguments.
PUBLIC int execl(const char *name, char *arg0) { return execve(name, &arg0, nullptr); }

// Execute a program with given environment.
PUBLIC int execle(const char *name, char *argv) {
    char **p;
    p = (char **)&argv;
    while (*p++) /* null statement */
        ;
    return execve(name, &argv, *p);
}

// Execute a program with argument array.
PUBLIC int execv(const char *name, char *argv[]) { return execve(name, argv, nullptr); }

// Low level execve system call wrapper.
PUBLIC int execve(const char *name, char *argv[], char *envp[]) {
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
        return (ErrorCode::E2BIG);
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
                return (ErrorCode::E2BIG);
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
                return (ErrorCode::E2BIG);
        }
        *hp++ = (char)0;
    }
    *ap++ = NIL_PTR;
    stackbytes = (((hp - stack) + ptrsize - 1) / ptrsize) * ptrsize;
    return callm1(MM_PROC_NR, EXEC, len(name), stackbytes, 0, name, stack, NIL_PTR);
}

// Exec a program without arguments or environment.
PUBLIC int execn(const char *name) {
    /* Special version used when there are no args and no environment.  This
     * call is principally used by INIT, to avoid having to allocate
     * MAX_ISTACK_BYTES.
     */

    char stack[4];

    stack[0] = 0;
    stack[1] = 0;
    stack[2] = 0;
    stack[3] = 0;
    return callm1(MM_PROC_NR, EXEC, len(name), 4, 0, name, stack, NIL_PTR);
}
