#include "glo.hpp"

// Memory manager global variables
mproc *mp = nullptr;           // pointer to current process table entry
auto dont_reply = 0;           // set to 1 to inhibit reply
int procs_in_use = 0;          // how many processes marked IN_USE
message mm_in{};               // incoming message
message mm_out{};              // reply message
int who = 0;                   // caller's process number
int mm_call = 0;               // system call number
int err_code = 0;              // temporary storage for error number
int result2 = 0;               // secondary result
char *res_ptr = nullptr;       // result pointer
char mm_stack[MM_STACK_BYTES]; // MM stack
