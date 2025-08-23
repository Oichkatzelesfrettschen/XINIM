#include "../include/stat.hpp"

/**
 * @brief Determine if a file descriptor refers to a terminal.
 *
 * This modern implementation uses a normal function prototype and
 * leverages the typed \c stat structure defined in \c stat.hpp.
 *
 * @param fd File descriptor to query.
 * @return 1 if the descriptor refers to a character device, otherwise 0.
 * @sideeffects Performs a system call to inspect the descriptor.
 * @thread_safety Thread-safe; operates on the provided descriptor only.
 * @compat isatty(3)
 * @example
 * if (isatty(STDIN_FILENO)) {
 *     // interactive terminal
 * }
 */
int isatty(int fd) noexcept {
    struct stat s{};
    fstat(fd, &s);
    return ((s.st_mode & S_IFMT) == S_IFCHR) ? 1 : 0;
}
