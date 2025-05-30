#include "../include/lib.hpp" // C++17 header

// Send signal 'sig' to process 'proc'.
PUBLIC int kill(int proc, int sig) {
    return callm1(MM, KILL, proc, sig, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
