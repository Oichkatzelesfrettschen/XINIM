#include "glo.hpp"

// Kernel global variable definitions
real_time realtime{}; // real time clock
int lost_ticks = 0;   // incremented when clock interrupts lost
int cur_proc = 0;     // current process
int prev_proc = 0;    // previous process
int sig_procs = 0;    // number of procs with pending signals
message int_mess{};   // messages built by interrupt routines
int olivetti = 0;     // TRUE for Olivetti-style keyboard
int pc_at = 0;        // PC-AT type diskette drives
int current_cpu = 0;  // current CPU id

t_stack t_stack[NR_TASKS - 1]; // task stacks; task = -1 never runs
char k_stack[K_STACK_BYTES];   // kernel stack
