/**
 * @brief Find the last occurrence of a character in a C-string.
 *
 * @param s Pointer to the null-terminated string to search.
 * @param c Character to find.
 * @return Pointer to the last occurrence or nullptr if not found.
 * @sideeffects None.
 * @thread_safety Safe for concurrent use.
 * @compat rindex(3)
 * @example
 * char *p = rindex(str, '/');
 */
char *rindex(char *s, char c) {
    char *result = nullptr;
    do {
        if (*s == c)
            result = s;
    } while (*s++ != 0);
    return result;
}
