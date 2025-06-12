#include "../include/lib.hpp" // C++17 header
#include <sys/types.h>        // For pid_t (standard location)

// Create a new process duplicating the calling process.
pid_t fork() { // Changed return type from int to pid_t
  // callm1 likely returns int, which is usually compatible with pid_t.
  // If pid_t could be wider than int on some theoretical platform,
  // a cast might be needed, but for Minix, int is typical for PIDs.
  return static_cast<pid_t>(callm1(MM, FORK, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR));
}
