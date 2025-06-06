/* This file contains the main program of MINIX.  The routine main()
 * initializes the system and starts the ball rolling by setting up the proc
 * table, interrupt vectors, and scheduling each task to run to initialize
 * itself.
 *
 * The entries into this file are:
 *   main:		MINIX main program
 *   unexpected_int:	called when an interrupt to an unused vector < 16 occurs
 *   trap:		called when an unexpected trap to a vector >= 16 occurs
 *   panic:		abort MINIX due to a fatal error
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
#ifdef __x86_64__
void init_syscall_msrs(void);
#endif /* __x86_64__ */

#define SAFETY 8       /* margin of safety for stack overflow (ints)*/
#define VERY_BIG 39328 /* must be bigger than kernel size (clicks) */
#define BASE 1536      /* address where MINIX starts in memory */
#define SIZES 8        /* sizes array has 8 entries */
#define CPU_TY1 0xFFFF /* BIOS segment that tells CPU type */
#define CPU_TY2 0x000E /* BIOS offset that tells CPU type */
#define PC_AT 0xFC     /* IBM code for PC-AT (in BIOS at 0xFFFFE) */

/*===========================================================================*
 *                                   main                                    *
 *===========================================================================*/
PUBLIC main() {
    /* Start the ball rolling. */

    register struct proc *rp;
    register int t;
    vir_clicks size;
    phys_clicks base_click, mm_base, previous_base;
    phys_bytes phys_b;
    extern unsigned sizes[8]; /* table filled in by build */
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

    lock();          /* we can't handle interrupts yet */
    current_cpu = 0; /* single CPU for now */
    paging_init();   /* set up initial page tables */
    idt_init();      /* install 64-bit IDT */
    base_click = BASE >> CLICK_SHIFT;
    size = sizes[0] + sizes[1];  /* kernel text + data size in clicks */
    mm_base = base_click + size; /* place where MM starts (in clicks) */

    for (rp = &proc[0]; rp <= &proc[NR_TASKS + LOW_USER]; rp++) {
        for (t = 0; t < NR_REGS; t++)
            rp->p_reg[t] = 0100 * t; /* debugging */
        t = rp - proc - NR_TASKS;    /* task number */
        rp->p_sp = (rp < &proc[NR_TASKS] ? t_stack[NR_TASKS + t + 1].stk : INIT_SP);
        rp->p_splimit = rp->p_sp;
        if (rp->p_splimit != INIT_SP)
            rp->p_splimit -= (TASK_STACK_BYTES - SAFETY) / sizeof(int);
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
	if (t < 0) {
		/* I/O tasks. */
		rp->p_map[T].mem_len  = VERY_BIG; 
		rp->p_map[T].mem_phys = base_click;
		rp->p_map[D].mem_len  = VERY_BIG; 
		rp->p_map[D].mem_phys = base_click + sizes[0];
		rp->p_map[S].mem_len  = VERY_BIG; 
		rp->p_map[S].mem_phys = base_click + sizes[0] + sizes[1];
		rp->p_map[S].mem_vir = sizes[0] + sizes[1];
	} else {
		/* MM, FS, and INIT. */
		previous_base = proc[NR_TASKS + t - 1].p_map[S].mem_phys;
		rp->p_map[T].mem_len  = sizes[2*t + 2];
		rp->p_map[T].mem_phys = (t == 0 ? mm_base : previous_base);
		rp->p_map[D].mem_len  = sizes[2*t + 3];
		rp->p_map[D].mem_phys = rp->p_map[T].mem_phys + sizes[2*t + 2];
		rp->p_map[S].mem_vir  = sizes[2*t + 3];
		rp->p_map[S].mem_phys = rp->p_map[D].mem_phys + sizes[2*t + 3];
	}


  proc[NR_TASKS+(HARDWARE)].p_sp = (int *) k_stack;
  proc[NR_TASKS+(HARDWARE)].p_sp += K_STACK_BYTES/2;
  proc[NR_TASKS+(HARDWARE)].p_splimit = (int *) k_stack;
  proc[NR_TASKS+(HARDWARE)].p_splimit += SAFETY/2;

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


/*===========================================================================*
 *                                   unexpected_int                          * 
 *===========================================================================*/
//------------------------------------------------------------------------------
// Handles unexpected interrupts that trigger on vectors lower than 16. This
// routine simply reports the event and the current program counter.
//------------------------------------------------------------------------------
void unexpected_int() {
    // Inform that an unexpected interrupt occurred.
    printf("Unexpected trap: vector < 16\n");

    // Print the current PC and the size of text+data+bss for debugging.
    printf("pc = 0x%x    text+data+bss = 0x%x\n", proc_ptr->p_pcpsw.pc,
           proc_ptr->p_map[D].mem_len << 4);
}


/*===========================================================================*
 *                                   trap                                    * 
 *===========================================================================*/
//------------------------------------------------------------------------------
// Handles traps for vectors greater than or equal to 16. These generally
// indicate an unexpected fault within the kernel or user space.
//------------------------------------------------------------------------------
void trap() {
    // Notify about the unexpected trap.
    printf("\nUnexpected trap: vector >= 16 ");

    // Provide additional context for developers.
    printf("This may be due to accidentally including\n");
    printf(
        "a non-MINIX library routine that is trying to make a system call.\n");
    printf("pc = 0x%x    text+data+bss = 0x%x\n", proc_ptr->p_pcpsw.pc,
           proc_ptr->p_map[D].mem_len << 4);
}


/*===========================================================================*
 *                                   div_trap                                * 
 *===========================================================================*/
//------------------------------------------------------------------------------
// Called when a divide instruction causes an overflow trap (vector 0). This
// routine reports the fault location to assist debugging.
//------------------------------------------------------------------------------
void div_trap() {
    // Inform that a divide overflow occurred.
    printf("Trap to vector 0: divide overflow.  ");

    // Show the program counter and memory size for context.
    printf("pc = 0x%x    text+data+bss = 0x%x\n", proc_ptr->p_pcpsw.pc,
           proc_ptr->p_map[D].mem_len << 4);
}


/*===========================================================================*
 *                                   panic                                   * 
 *===========================================================================*/
//------------------------------------------------------------------------------
// Handle unrecoverable kernel errors. This routine prints the provided message
// and halts the system after flushing any pending output.
//------------------------------------------------------------------------------
void panic(const char *s, int n) {
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
