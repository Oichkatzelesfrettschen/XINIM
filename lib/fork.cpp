#include "../include/lib.hpp" // C++17 header

PUBLIC int fork() {
    return callm1(MM, static_cast<int>(SysCall::FORK), 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
