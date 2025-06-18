#include <cstddef>

/**
 * @brief Locate character @p c in string @p s.
 *
 * This function mirrors the historical BSD `index` routine used by
 * the original MINIX utilities. It returns a pointer to the first
 * occurrence of @p c within @p s or `nullptr` if the character is not
 * found.
 *
 * @param s Null-terminated string to search.
 * @param c Character to locate.
 * @return Pointer to located character or `nullptr`.
 */
char *index(char *s, int c) {
    do {
        if (*s == c) {
            return s;
        }
    } while (*s++ != 0);
    return nullptr;
}
