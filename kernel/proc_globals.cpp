#include "proc.hpp"

// Process table and scheduling pointers
proc proc[NR_TASKS + NR_PROCS];
proc *proc_ptr = nullptr;              // current process
proc *bill_ptr = nullptr;              // process to bill clock ticks
proc *rdy_head[NR_CPUS][SCHED_QUEUES]; // ready list heads
proc *rdy_tail[NR_CPUS][SCHED_QUEUES]; // ready list tails

unsigned busy_map = 0;            // bit map of busy tasks
message *task_mess[NR_TASKS + 1]; // pointers to task messages
