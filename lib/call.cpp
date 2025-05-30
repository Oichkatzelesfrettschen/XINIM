#include "../include/lib.hpp" // C++17 header

PUBLIC int errno; /* place where error numbers go */

// Send a message using the m1 layout.
PUBLIC int callm1(int proc, int syscallnr, int int1, int int2, int int3, char *ptr1, char *ptr2,
                  char *ptr3) {
    /* Send a message and get the response.  The 'M.m_type' field of the
     * reply contains a value (>= 0) or an error code (<0). Use message format m1.
     */
    M.m1_i1 = int1;
    M.m1_i2 = int2;
    M.m1_i3 = int3;
    M.m1_p1 = ptr1;
    M.m1_p2 = ptr2;
    M.m1_p3 = ptr3;
    return callx(proc, syscallnr);
}

// Send a message containing one integer and a string.
PUBLIC int callm3(int proc, int syscallnr, int int1, char *name) {
    /* This form of system call is used for those calls that contain at most
     * one integer parameter along with a string.  If the string fits in the
     * message, it is copied there.  If not, a pointer to it is passed.
     */
    register int k;
    register char *rp;
    k = len(name);
    M.m3_i1 = k;
    M.m3_i2 = int1;
    M.m3_p1 = name;
    rp = &M.m3_ca1[0];
    if (k <= M3_STRING)
        while (k--)
            *rp++ = *name++;
    return callx(proc, syscallnr);
}

// Low-level send/receive wrapper.
PUBLIC int callx(int proc, int syscallnr) {
    /* Send a message and get the response.  The 'M.m_type' field of the
     * reply contains a value (>= 0) or an error code (<0).
     */
    int k;

    M.m_type = syscallnr;
    k = sendrec(proc, &M);
    if (k != OK)
        return (k); /* send itself failed */
    if (M.m_type < 0) {
        errno = -M.m_type;
        return (-1);
    }
    return (M.m_type);
}

// Return the length of a null-terminated string including the final null.
PUBLIC int len(char *s) {
    /* Return the length of a character string, including the 0 at the end. */
    register int k;

    k = 0;
    while (*s++ != 0)
        k++;
    return (k + 1); /* return length including the 0-byte at end */
}
