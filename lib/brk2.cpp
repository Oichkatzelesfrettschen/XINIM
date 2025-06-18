#include "../include/lib.hpp" // C++23 header

// Forward declaration from getutil.cpp
extern phys_clicks get_size(void);

// Request memory segment move to the end of BSS.
void brk2() {
    char *p1, *p2;

    p1 = (char *)get_size();
    callm1(MM, BRK2, 0, 0, 0, p1, p2, NIL_PTR);
}
