#include "../include/lib.hpp" // C++23 header

// Global storage for the most recent error number.
int errno = 0; // accessed by system call wrappers

/**
 * @brief Send a message using the m1 layout.
 *
 * @param proc       Destination process.
 * @param syscallnr  System call number.
 * @param int1       First integer parameter.
 * @param int2       Second integer parameter.
 * @param int3       Third integer parameter.
 * @param ptr1       First pointer parameter.
 * @param ptr2       Second pointer parameter.
 * @param ptr3       Third pointer parameter.
 * @return Result of ::callx.
 */
int callm1(int proc, int syscallnr, int int1, int int2, int int3, char *ptr1, char *ptr2,
           char *ptr3) noexcept {
    /* Send a message and get the response.  The 'M.m_type' field of the
     * reply contains a value (>= 0) or an error code (<0). Use message format m1.
     */
    M.m1_i1() = int1;
    M.m1_i2() = int2;
    M.m1_i3() = int3;
    M.m1_p1() = ptr1;
    M.m1_p2() = ptr2;
    M.m1_p3() = ptr3;
    return callx(proc, syscallnr);
}

/**
 * @brief Send a message containing one integer and a string.
 *
 * @param proc       Destination process.
 * @param syscallnr  System call number.
 * @param int1       Integer argument.
 * @param name       Pointer to string argument.
 * @return Result of ::callx.
 */
int callm3(int proc, int syscallnr, int int1, const char *name) noexcept {
    /* This form of system call is used for those calls that contain at most
     * one integer parameter along with a string.  If the string fits in the
     * message, it is copied there.  If not, a pointer to it is passed.
     */
    std::size_t k = len(name);
    char *rp = &M.m3_ca1()[0];
    const char *src = name;
    M.m3_i1() = static_cast<int>(k);
    M.m3_i2() = int1;
    M.m3_p1() = const_cast<char *>(name);
    if (k <= M3_STRING) {
        while (k--)
            *rp++ = *src++;
    }
    return callx(proc, syscallnr);
}

/**
 * @brief Low-level send/receive wrapper.
 *
 * @param proc       Destination process.
 * @param syscallnr  System call number.
 * @return Kernel response value or error.
 */
int callx(int proc, int syscallnr) noexcept {
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

/**
 * @brief Return the length of a null-terminated string including the final null
 *        byte.
 *
 * @param s Pointer to string.
 * @return  Length in bytes including the terminator.
 */
std::size_t len(const char *s) noexcept {
    // Return the length of a character string including the terminating null.
    std::size_t k = 0;
    while (*s++ != 0) {
        ++k;
    }
    return k + 1U; // include the null terminator
}
