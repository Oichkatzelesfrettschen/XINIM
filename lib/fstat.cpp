#include "../include/lib.hpp" // C++23 header
#include "../include/stat.hpp"

// Get status information for the open file descriptor 'fd'.
int fstat(int fd, struct stat *buf) noexcept {
    int n = callm1(FS, FSTAT, fd, 0, 0, reinterpret_cast<char *>(buf), NIL_PTR, NIL_PTR);
    return n;
}
