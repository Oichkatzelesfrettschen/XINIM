/* This file contains some dumping routines for debugging. */

#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "../include/stdio.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "type.hpp"
#include "proc.hpp"
#include <cstddef> 
#include <cstdint> 
#include <inttypes.h> 

static void prname(int i) noexcept; 

#define NSIZE 20
uint64_t aout[NR_PROCS]; 
char nbuff[NSIZE + 1];   
int vargv;               

extern "C" {

void p_dmp() noexcept { 
    struct proc *rp;
    char *np;
    [[maybe_unused]] std::size_t base, limit; 
    [[maybe_unused]] uint64_t first, last;    
    [[maybe_unused]] uint64_t dst;      
    int index;

    printf("\nproc  -pid- --pc--  --sp--  flag  user  -sys-  base limit recv   command\n");

    dst = umap(proc_addr(SYSTASK), D, reinterpret_cast<std::size_t>(nbuff),
               static_cast<std::size_t>(NSIZE));

    for (rp = &proc[0]; rp < &proc[NR_PROCS + NR_TASKS]; rp++) {
        if (rp->p_flags & static_cast<int>(P_SLOT_FREE))
            continue;
        first = rp->p_map[T].mem_phys; 
        last = rp->p_map[S].mem_phys + rp->p_map[S].mem_len; 

        base = (rp->p_map[T].mem_phys + rp->p_map[T].mem_len) * CLICK_SIZE / 1024;
        limit = (rp->p_map[S].mem_phys + rp->p_map[S].mem_len) * CLICK_SIZE / 1024;

        prname(static_cast<int>(rp - proc));
        printf(" %4d %4" PRIxPTR " %4" PRIxPTR " %4x %6" PRId64 " %7" PRId64 "  %3zuK %3zuK  ",
               rp->p_pid,
               static_cast<uintptr_t>(rp->p_pcpsw.pc), 
               static_cast<uintptr_t>(rp->p_sp),       
               rp->p_flags, rp->user_time, rp->sys_time,
               (rp->p_map[D].mem_vir + rp->p_map[D].mem_len) * CLICK_SIZE / 1024,                                  
               rp->p_map[S].mem_len * CLICK_SIZE / 1024); 

        if (rp->p_flags == 0)
            printf("      ");
        else
            prname(NR_TASKS + rp->p_getfrom);

        index = static_cast<int>(rp - proc - NR_TASKS);
        if (index >= 0 && index < NR_PROCS && aout[index] != 0) { 
            phys_copy(reinterpret_cast<void*>(static_cast<uintptr_t>(dst)), 
                      reinterpret_cast<const void*>(static_cast<uintptr_t>(aout[index])), 
                      static_cast<std::size_t>(NSIZE));
            nbuff[NSIZE] = '\0';                           
            for (np = &nbuff[0]; np < &nbuff[NSIZE]; np++) 
                if (*np <= ' ' || *np >= 0177)
                    *np = '\0';        
            if (index == INIT_PROC_NR) 
                printf("/bin/sh");     
            else
                printf("%s", nbuff);
        }
        printf("\n");
    }
    printf("\n");
}

void map_dmp() noexcept { 
    struct proc *rp;
    printf("\nPROC   -----TEXT-----  -----DATA-----  ----STACK-----  BASE SIZE\n");
    for (rp = proc_addr(0); rp < proc_addr(NR_PROCS); rp++) {
        if (rp->p_flags & static_cast<int>(P_SLOT_FREE))
            continue;

        prname(static_cast<int>(rp - proc)); 

        printf(" %4zx %4" PRIx64 " %4zx  %4zx %4" PRIx64 " %4zx  %4zx %4" PRIx64 " %4zx\n",
               rp->p_map[T].mem_vir, rp->p_map[T].mem_phys, rp->p_map[T].mem_len,
               rp->p_map[D].mem_vir, rp->p_map[D].mem_phys, rp->p_map[D].mem_len,
               rp->p_map[S].mem_vir, rp->p_map[S].mem_phys, rp->p_map[S].mem_len);
    }
}

void set_name(int proc_nr, char *ptr) noexcept { 
    uint64_t src, dst; 

    if (ptr == nullptr) {
        aout[proc_nr] = 0; 
        return;
    }

    src = umap(proc_addr(proc_nr), D, reinterpret_cast<std::size_t>(ptr + 2), 2);
    if (src == 0) return;
    dst = umap(proc_addr(SYSTASK), D, reinterpret_cast<std::size_t>(&vargv), 2);
    phys_copy(reinterpret_cast<void*>(static_cast<uintptr_t>(dst)), 
              reinterpret_cast<const void*>(static_cast<uintptr_t>(src)), 
              2ULL);

    aout[proc_nr] = umap(proc_addr(proc_nr), D, static_cast<std::size_t>(vargv), static_cast<std::size_t>(NSIZE));
}

} // extern "C" 

static const char *nayme[] = {"PRINTR", "TTY   ", "WINCHE", "FLOPPY", "RAMDSK", "CLOCK ",
                       "SYS   ", "HARDWR", "MM    ", "FS    ", "INIT  "}; 

static void prname(int i) noexcept { 
    if (i == ANY + NR_TASKS)
        printf("ANY   ");
    else if (i >= 0 && i < 11)
        printf("%s", nayme[i]);
    else
        printf("%4d  ", i - NR_TASKS);
}
