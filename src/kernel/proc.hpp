#pragma once
// Modernized for C++23

#include <sys/const.hpp>      
#include <sys/type.hpp>       
#include "../include/defs.hpp" 
#include "./type.hpp"          
#include "const.hpp"           

#ifdef printf
#undef printf
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Kernel Primitives (Renamed to avoid collisions with POSIX)
int mini_send(int caller, int dest, message *m_ptr) noexcept;
int mini_rec(int caller, int src, message *m_ptr) noexcept;
int ipc_send(int dest, message *m_ptr) noexcept;
int ipc_receive(int src, message *m_ptr) noexcept;
void interrupt(int task, message *m_ptr) noexcept;
void sys_call(int function, int caller, int src_dest, message *m_ptr) noexcept;
void kernel_sched() noexcept;

// Architecture / Hardware Primitives
void port_in(unsigned port, unsigned *val) noexcept;
void port_out(unsigned port, unsigned val) noexcept;
void portw_in(unsigned port, unsigned *val) noexcept;
void portw_out(unsigned port, unsigned val) noexcept;
void phys_copy(void *dst, const void *src, std::size_t n) noexcept;
void phys_copy16(void *dst, const void *src, std::size_t words) noexcept;
void lock() noexcept;
void restore() noexcept;
void unlock() noexcept;
void reboot() noexcept;
void halt() noexcept;

// Process & Memory Management
void pick_proc() noexcept;
void ready(struct proc *rp) noexcept;
void unready(struct proc *rp) noexcept;
void cp_mess(int src, uint64_t src_phys, const void* src_vir, uint64_t dest_phys, void* dest_vir) noexcept;
void inform(int proc_nr) noexcept;
void cause_sig(int proc_nr, int sig_nr) noexcept;
uint64_t umap(struct proc *rp, int seg, std::size_t vir_addr, std::size_t bytes) noexcept;

/**
 * @brief Process descriptor stored in the kernel table.
 */
struct proc {
    std::uint64_t p_reg[NR_REGS];  
    xinim::virt_addr_t p_sp;       
    struct pc_psw p_pcpsw;         
    int p_flags;                   
    struct mem_map p_map[NR_SEGS]; 
    xinim::virt_addr_t p_splimit;  
    xinim::pid_t p_pid;            
    std::uint64_t p_token;         

    real_time user_time;   
    real_time sys_time;    
    real_time child_utime; 
    real_time child_stime; 
    real_time p_alarm;     

    struct proc *p_callerq;  
    struct proc *p_sendlink; 
    message *p_messbuf;      
    int p_getfrom;           

    struct proc *p_nextready; 
    int p_pending;            
    xinim::phys_addr_t cr3;   
    int p_priority;           
    int p_cpu;                
};

#ifndef EXTERN
#define EXTERN extern
#endif

EXTERN struct proc proc[NR_TASKS + NR_PROCS];

/* Bits for p_flags in proc[].  A process is runnable iff p_flags == 0 */
inline constexpr unsigned int P_SLOT_FREE = 001; 
inline constexpr unsigned int NO_MAP = 002;      
inline constexpr unsigned int SENDING = 004;     
inline constexpr unsigned int RECEIVING = 010;   

#define proc_addr(n) &proc[NR_TASKS + n] 
inline constexpr struct proc *NIL_PROC = nullptr;

EXTERN struct proc *proc_ptr;                        
EXTERN struct proc *bill_ptr;                        
EXTERN struct proc *rdy_head[NR_CPUS][SCHED_QUEUES]; 
EXTERN struct proc *rdy_tail[NR_CPUS][SCHED_QUEUES]; 

EXTERN unsigned int busy_map;            
EXTERN message *task_mess[NR_TASKS + 1]; 

#ifdef __cplusplus
}
#endif
