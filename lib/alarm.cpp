#include "../include/lib.hpp" // C++17 header

// Request a timer alarm in 'sec' seconds.
PUBLIC int alarm(unsigned sec) {
    return callm1(MM, ALARM, static_cast<int>(sec), 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
