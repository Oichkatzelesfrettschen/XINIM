#include "../include/lib.hpp" // C++23 header

// Set the process file mode creation mask.
int umask(int complmode) { return callm1(FS, UMASK, complmode, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }
