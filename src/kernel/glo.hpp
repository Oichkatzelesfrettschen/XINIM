#pragma once
// Modernized for C++23

#include <sys/const.hpp> 
#include <sys/type.hpp>  
#include "./const.hpp"    

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EXTERN
#define EXTERN extern
#endif

/* Clocks and timers */
EXTERN real_time realtime; 
EXTERN real_time lost_ticks; 

/* Processes, signals, and messages. */
EXTERN xinim::pid_t cur_proc;  
EXTERN xinim::pid_t prev_proc; 
EXTERN int sig_procs;          
EXTERN message int_mess;       

/* CPU type. */
EXTERN bool olivetti; 
EXTERN bool pc_at;    
EXTERN int current_cpu;

/* The kernel and task stacks. */
EXTERN struct t_stack {
    int stk[TASK_STACK_BYTES / sizeof(int)];
} t_stack[NR_TASKS - 1]; 

EXTERN char k_stack[K_STACK_BYTES]; 

#ifdef __cplusplus
}
#endif
