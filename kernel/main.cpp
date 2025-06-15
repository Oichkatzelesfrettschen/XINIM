/**
 * @file
 * @brief Kernel entry point and trap handlers.
 *
 * This file defines the core initialization routine as well as helpers used
 * when interrupts or traps occur. On fatal errors the kernel invokes ::panic
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstddef>    // For std::size_t
#include <cstdint>    // For uint64_t etc.
#include <inttypes.h> // For PRIx64, PRIuMAX etc.

#ifdef __x86_64__
void init_syscall_msrs() noexcept; // Changed (void) to (), added noexcept
#endif                             /* __x86_64__ */

#define SAFETY 8       /* margin of safety for stack overflow (ints)*/
#define VERY_BIG 39328 /* must be bigger than kernel size (clicks) */
#define BASE 1536      /* address where MINIX starts in memory */
#define SIZES 8        /* sizes array has 8 entries */
#define CPU_TY1 0xFFFF /* BIOS segment that tells CPU type */
#define CPU_TY2 0x000E /* BIOS offset that tells CPU type */
#define PC_AT 0xFC     /* IBM code for PC-AT (in BIOS at 0xFFFFE) */
/**
 * @brief Kernel main routine.
 *
 * Initializes process structures and starts the first process.
 */
int main() noexcept { // Changed from PUBLIC main(), added noexcept
    /* Start the ball rolling. */

    struct proc *rp;
    int t;
    std::size_t size;                            // vir_clicks -> std::size_t
    uint64_t base_click, mm_base, previous_base; // phys_clicks -> uint64_t
    uint64_t phys_b;                             // phys_bytes -> uint64_t (seems unused)
    extern unsigned int sizes[8];                // table filled in by build (unsigned int)
    extern int color, vec_table[], get_chrome(), (*task[])();
    extern int s_call(), disk_int(), tty_int(), clock_int(), disk_int();
    extern int wini_int(), lpr_int(), surprise(), trp(), divide();
    extern phys_bytes umap();

    /* Set up proc table entry for user processes.  Be very careful about
     * sp, since the 3 words prior to it will be clobbered when the kernel pushes
     * pc, cs, and psw onto the USER's stack when starting the user the first
     * time.  This means that with initial sp = 0x10, user programs must leave
     * the words at 0x000A, 0x000C, and 0x000E free.
     */

    lock();                                                  /* we can't handle interrupts yet */
    current_cpu = 0;                                         /* single CPU for now */
    paging_init();                                           /* set up initial page tables */
    idt_init();                                              /* install 64-bit IDT */
    base_click = static_cast<uint64_t>(BASE >> CLICK_SHIFT); // BASE is int
    size = static_cast<std::size_t>(sizes[0]) +
           static_cast<std::size_t>(sizes[1]); /* kernel text + data size in clicks */
    mm_base =
        base_click + size; /* place where MM starts (in clicks) (uint64_t + size_t -> uint64_t) */

    for (rp = &proc[0]; rp <= &proc[NR_TASKS + LOW_USER]; rp++) {
        for (t = 0; t < NR_REGS; t++)
            rp->p_reg[t] = 0100 * t; /* debugging */
        t = rp - proc - NR_TASKS;    /* task number */
        // rp->p_sp is uint64_t. t_stack[...].stk is likely int[] or similar. INIT_SP is uint64_t*
        // (nullptr).
        if (rp < &proc[NR_TASKS]) {
            rp->p_sp = reinterpret_cast<uint64_t>(t_stack[NR_TASKS + t + 1].stk);
        } else {
            rp->p_sp = reinterpret_cast<uint64_t>(INIT_SP); // INIT_SP is nullptr
        }
        rp->p_splimit = rp->p_sp;
        if (rp->p_splimit != reinterpret_cast<uint64_t>(INIT_SP))
            // TASK_STACK_BYTES is std::size_t. SAFETY is int.
            rp->p_splimit -=
                (static_cast<std::size_t>(TASK_STACK_BYTES) - static_cast<std::size_t>(SAFETY)) /
                sizeof(int);
        rp->p_pcpsw.pc = task[t + NR_TASKS];
        if (rp->p_pcpsw.pc != 0 || t >= 0)
            ready(rp);
        rp->p_pcpsw.psw = INIT_PSW;
        rp->p_flags = 0;
#if SCHED_ROUND_ROBIN
        rp->p_priority = (t < 0 ? TASK_Q : t < LOW_USER ? SERVER_Q : USER_Q);
#else
        rp->p_priority = (t < 0 ? PRI_TASK : t < LOW_USER ? PRI_SERVER : PRI_USER);
        rp->p_cpu = 0;

	/* Set up memory map for tasks and MM, FS, INIT. */
	// p_map members: mem_len (vir_clicks -> std::size_t), mem_phys (phys_clicks -> uint64_t), mem_vir (vir_clicks -> std::size_t)
	// VERY_BIG is int. base_click is uint64_t. sizes[] are unsigned int.
	if (t < 0) {
		/* I/O tasks. */
		rp->p_map[T].mem_len  = static_cast<std::size_t>(VERY_BIG);
		rp->p_map[T].mem_phys = base_click;
		rp->p_map[D].mem_len  = static_cast<std::size_t>(VERY_BIG);
		rp->p_map[D].mem_phys = base_click + static_cast<uint64_t>(sizes[0]);
		rp->p_map[S].mem_len  = static_cast<std::size_t>(VERY_BIG);
		rp->p_map[S].mem_phys = base_click + static_cast<uint64_t>(sizes[0]) + static_cast<uint64_t>(sizes[1]);
		rp->p_map[S].mem_vir = static_cast<std::size_t>(sizes[0]) + static_cast<std::size_t>(sizes[1]);
	} else {
		/* MM, FS, and INIT. */
		previous_base = proc[NR_TASKS + t - 1].p_map[S].mem_phys; // uint64_t
		rp->p_map[T].mem_len  = static_cast<std::size_t>(sizes[2*t + 2]);
		rp->p_map[T].mem_phys = (t == 0 ? mm_base : previous_base); // uint64_t
		rp->p_map[D].mem_len  = static_cast<std::size_t>(sizes[2*t + 3]);
		rp->p_map[D].mem_phys = rp->p_map[T].mem_phys + static_cast<uint64_t>(sizes[2*t + 2]); // uint64_t
		rp->p_map[S].mem_vir  = static_cast<std::size_t>(sizes[2*t + 3]);
		rp->p_map[S].mem_phys = rp->p_map[D].mem_phys + static_cast<uint64_t>(sizes[2*t + 3]); // uint64_t
	}

  // proc p_sp and p_splimit are uint64_t. k_stack is char[].
  proc[NR_TASKS+(HARDWARE)].p_sp = reinterpret_cast<uint64_t>(k_stack + K_STACK_BYTES / 2);
  proc[NR_TASKS+(HARDWARE)].p_splimit = reinterpret_cast<uint64_t>(k_stack + SAFETY / 2); // Original was +=, check logic

  for (rp = proc_addr(LOW_USER+1); rp < proc_addr(NR_PROCS); rp++)
	rp->p_flags = P_SLOT_FREE;

  /* Determine if display is color or monochrome and CPU type (from BIOS). */
  color = get_chrome();		/* 0 = mono, 1 = color */
  t = get_byte(CPU_TY1, CPU_TY2);	/* is this PC, XT, AT ... ? */
  if (t == PC_AT) pc_at = TRUE;

  bill_ptr = proc_addr(HARDWARE);	/* it has to point somewhere */
#ifdef __x86_64__
  init_syscall_msrs();
  pick_proc();

  /* Now go to the assembly code to start running the current process. */
  port_out(INT_CTLMASK, 0);	/* do not mask out any interrupts in 8259A */
  port_out(INT2_MASK, 0);	/* same for second interrupt controller */
  restart();
#endif /* __x86_64__ */
}

/**
 * @brief Interrupt handler for unused vectors (< 16).
 *
 * Reports the interrupt and dumps the current program counter.
 */
void unexpected_int() noexcept {
    // Inform that an unexpected interrupt occurred.
    printf("Unexpected trap: vector < 16\n");

    // Print the current PC and the size of text+data+bss for debugging.
    // proc_ptr->p_pcpsw.pc is u64_t. proc_ptr->p_map[D].mem_len is std::size_t (vir_clicks).
    printf("pc = 0x%" PRIx64 "    text+data+bss (clicks << 4) = 0x%zx\n",
           proc_ptr->p_pcpsw.pc,
           proc_ptr->p_map[D].mem_len << 4);
}


/**
 * @brief Trap handler for vectors >= 16.
 *
 * Prints diagnostic information about the faulting location.
 */
void trap() noexcept {
    // Notify about the unexpected trap.
    printf("\nUnexpected trap: vector >= 16 ");

    // Provide additional context for developers.
    printf("This may be due to accidentally including\n");
    printf(
        "a non-MINIX library routine that is trying to make a system call.\n");
    // proc_ptr->p_pcpsw.pc is u64_t. proc_ptr->p_map[D].mem_len is std::size_t (vir_clicks).
    printf("pc = 0x%" PRIx64 "    text+data+bss (clicks << 4) = 0x%zx\n",
           proc_ptr->p_pcpsw.pc,
           proc_ptr->p_map[D].mem_len << 4);
}


/**
 * @brief Divide overflow trap handler (vector 0).
 *
 * Reports the fault address for debugging.
 */
void div_trap() noexcept {
    // Inform that a divide overflow occurred.
    printf("Trap to vector 0: divide overflow.  ");

    // Show the program counter and memory size for context.
    // proc_ptr->p_pcpsw.pc is u64_t. proc_ptr->p_map[D].mem_len is std::size_t (vir_clicks).
    printf("pc = 0x%" PRIx64 "    text+data+bss (clicks << 4) = 0x%zx\n",
           proc_ptr->p_pcpsw.pc,
           proc_ptr->p_map[D].mem_len << 4);
}
/**
 * @brief Abort execution due to a fatal error.
 *
 * Prints the given message then waits for a reboot.
 */
void panic(const char *s, int n) noexcept {
    // Display the panic message if one was supplied.
    if (*s != 0) {
        printf("\nKernel panic: %s", s);
        if (n != NO_NUM) {
            printf(" %d", n);
        }
        printf("\n");
    }

    // Prompt for reboot to recover from the fatal error.
    printf("\nType space to reboot\n");
    wreboot();
}
