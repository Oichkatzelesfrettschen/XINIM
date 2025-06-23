#include "../include/stat.hpp"

/**
 * @brief Determine if a file descriptor refers to a terminal.
 *
 * This modern implementation uses a normal function prototype and
 * leverages the typed \c stat structure defined in \c stat.hpp.
 *
 * @param fd File descriptor to query.
 * @return 1 if the descriptor refers to a character device, otherwise 0.
 */
int isatty(int fd) noexcept {
    struct stat s{};
    fstat(fd, &s);
    return ((s.st_mode & S_IFMT) == S_IFCHR) ? 1 : 0;
}
