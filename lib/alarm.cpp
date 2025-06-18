#include "../include/lib.hpp" // C++23 header

// Request an alarm after the specified number of seconds.
// Returns the previous alarm value.
int alarm(unsigned int sec) {
    return callm1(MM, ALARM, static_cast<int>(sec), 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
