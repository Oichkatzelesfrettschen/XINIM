#include "../include/lib.hpp" // C++17 header

// Request an alarm after the specified number of seconds.
int alarm(unsigned sec) {
    return callm1(MM, ALARM, static_cast<int>(sec), 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
