#include "../include/lib.hpp" // C++17 header

/* Request memory segment move */
extern "C" phys_clicks get_size(void);

PUBLIC void brk2() {
    char *p1, *p2;

    p1 = reinterpret_cast<char *>(get_size());
    callm1(MM, BRK2, 0, 0, 0, p1, p2, NIL_PTR);
}
