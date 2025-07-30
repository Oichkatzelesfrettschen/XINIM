/* This file contains the printer driver. It is a fairly simple driver,
 * supporting only one printer.  Characters that are written to the driver
 * are written to the printer without any changes at all.
 *
 * The valid messages and their parameters are:
 *
 *   TTY_O_DONE:   output completed
 *   TTY_WRITE:    a process wants to write on a terminal
 *   CANCEL:       terminate a previous incomplete system call immediately
 *
 *    m_type      TTY_LINE   PROC_NR    COUNT    ADDRESS
 * -------------------------------------------------------
 * | TTY_O_DONE  |minor dev|         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | TTY_WRITE   |minor dev| proc nr |  count  | buf ptr |
 * |-------------+---------+---------+---------+---------|
 * | CANCEL      |minor dev| proc nr |         |         |
 * -------------------------------------------------------
 *
 * Note: since only 1 printer is supported, minor dev is not used at present.
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
#include <cstddef> // For std::size_t
#include <cstdint> // For uint64_t
#include <cstdio>  // For printf

#define NORMAL_STATUS 0xDF  /* printer gives this status when idle */
#define BUSY_STATUS 0x5F    /* printer gives this status when busy */
#define ASSERT_STROBE 0x1D  /* strobe a character to the interface */
#define NEGATE_STROBE 0x1C  /* enable interrupt on interface */
#define SELECT 0x0C         /* select printer bit */
#define INIT_PRINTER 0x08   /* init printer bits */
#define NO_PAPER 0x20       /* status bit saying that paper is up */
#define OFF_LINE 0x10       /* status bit saying that printer not online*/
#define PR_ERROR 0x08       /* something is wrong with the printer */
#define PR_COLOR_BASE 0x378 /* printer port when color display used */
#define PR_MONO_BASE 0x3BC  /* printer port when mono display used */
#define LOW_FOUR 0xF        /* mask for low-order 4 bits */
#define CANCELED -999       /* indicates that command has been killed */
#define DELAY_COUNT 100     /* regulates delay between characters */
#define DELAY_LOOP 1000     /* delay when printer is busy */
#define MAX_REP 1000        /* controls max delay when busy */

static int port_base;          /* I/O port for printer: 0x 378 or 0x3BC */
static int caller;             /* process to tell when printing done (FS) */
static int requesting_proc;    /* user requesting the printing */
static std::size_t orig_count; /* original byte count (was int) */
static int es;     /* (es, offset) point to next character to - segment part from phys addr */
static int offset; /* print, i.e., in the user's buffer - offset part from phys addr */
PUBLIC std::size_t pcount; /* number of bytes left to print (was int) */
PUBLIC int pr_busy;        /* TRUE when printing, else FALSE */
PUBLIC int cum_count;      /* cumulative # characters printed */
PUBLIC int prev_ct;        /* value of cum_count 100 msec ago */

/*===========================================================================*
 *				printer_task				     *
 *===========================================================================*/
PUBLIC void printer_task() noexcept { // Added void return, noexcept
    /* Main routine of the printer task. */

    message print_mess; /* buffer for all incoming messages */

    print_init(); /* initialize */

    while (TRUE) {
        receive(ANY, &print_mess);
        switch (print_mess.m_type) {
        case TTY_WRITE:
            do_write(&print_mess);
            break;
        case CANCEL:
            do_cancel(&print_mess);
            break;
        case TTY_O_DONE:
            do_done(&print_mess);
            break;
        default:
            break;
        }
    }
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
static void do_write(message *m_ptr) noexcept { // PRIVATE -> static, modernized signature, noexcept
    /* The printer is used by sending TTY_WRITE messages to it. Process one. */

    int i, j, r, value;
    struct proc *rp;
    uint64_t phys; // phys_bytes -> uint64_t
    // extern phys_bytes umap(); // umap returns uint64_t

    r = OK; /* so far, no errors */

    /* Reject command if printer is busy or count is not positive. */
    if (pr_busy)
        r = ErrorCode::EAGAIN;
    if (count(*m_ptr) <= 0)
        r = ErrorCode::EINVAL;

    /* Compute the physical address of the data buffer within user space. */
    rp = proc_addr(proc_nr(*m_ptr));
    // umap expects (proc*, int, std::size_t, std::size_t) returns uint64_t
    // ADDRESS from message is char*, COUNT is int
    phys = umap(rp, D, reinterpret_cast<std::size_t>(address(*m_ptr)),
                static_cast<std::size_t>(count(*m_ptr)));
    if (phys == 0)
        r = ErrorCode::E_BAD_ADDR;

    if (r == OK) {
        /* Save information needed later. */
        lock(); // noexcept
        caller = m_ptr->m_source;
        requesting_proc = proc_nr(*m_ptr);
        pcount = static_cast<std::size_t>(count(*m_ptr));
        orig_count = static_cast<std::size_t>(count(*m_ptr));
        // phys is uint64_t. es, offset are int. CLICK_SHIFT, LOW_FOUR are int.
        es = static_cast<int>(phys >> CLICK_SHIFT);
        offset = static_cast<int>(phys & LOW_FOUR);

        /* Start the printer. */
        for (i = 0; i < MAX_REP; i++) {
            port_in(port_base + 1, &value);
            if (value == NORMAL_STATUS) {
                pr_busy = TRUE;
                pr_char();   /* print first character */
                r = SUSPEND; /* tell FS to suspend user until done */
                break;
            } else {
                if (value == BUSY_STATUS) {
                    for (j = 0; j < DELAY_LOOP; j++) /* delay */
                        ;
                    continue;
                }
                pr_error(value);
                r = ErrorCode::EIO;
                break;
            }
        }
    }

    /* Reply to FS, no matter what happened. */
    if (value == BUSY_STATUS)
        r = ErrorCode::EAGAIN;
    reply(TASK_REPLY, m_ptr->m_source, proc_nr(*m_ptr), r);
}

/*===========================================================================*
 *				do_done					     *
 *===========================================================================*/
static void do_done(message *m_ptr) noexcept { // PRIVATE -> static, modernized signature, noexcept
    /* Printing is finished.  Reply to caller (FS). */

    int status;

    status = (rep_status(*m_ptr) == OK ? orig_count : ErrorCode::EIO);
    if (requesting_proc != CANCELED) {
        reply(REVIVE, caller, requesting_proc, status);
        if (status == ErrorCode::EIO)
            pr_error(rep_status(*m_ptr));
    }
    pr_busy = FALSE;
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
PRIVATE do_cancel(m_ptr)
message *m_ptr; /* pointer to the newly arrived message */
{
    /* Cancel a print request that has already started.  Usually this means that
     * the process doing the printing has been killed by a signal.
     */

    if (pr_busy == FALSE)
        return;      /* this statement avoids race conditions */
    pr_busy = FALSE; /* mark printer as idle */
    pcount = 0;      /* causes printing to stop at next interrupt (pcount is std::size_t) */
    requesting_proc = CANCELED; /* marks process as canceled */
    reply(TASK_REPLY, m_ptr->m_source, proc_nr(*m_ptr), ErrorCode::EINTR);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
// Modernized K&R (already had types but now explicit), added noexcept
static void reply(int code, int replyee, int process, int status) noexcept {
    /* Send a reply telling FS that printing has started or stopped. */

    message pr_mess;

    pr_mess.m_type = code;          /* TASK_REPLY or REVIVE */
    rep_status(pr_mess) = status;   /* count or ErrorCode::EIO */
    rep_proc_nr(pr_mess) = process; /* which user does this pertain to */
    send(replyee, &pr_mess);        /* send the message */
}

/*===========================================================================*
 *				pr_error				     *
 *===========================================================================*/
static void pr_error(int status) noexcept { // PRIVATE -> static, modernized signature, noexcept
    /* The printer is not ready.  Display a message on the console telling why. */

    if (status & NO_PAPER)
        printf("Printer is out of paper\n");
    if ((status & OFF_LINE) == 0)
        printf("Printer is not on line\n");
    if ((status & PR_ERROR) == 0)
        printf("Printer error\n");
}

/*===========================================================================*
 *				print_init				     *
 *===========================================================================*/
static void print_init() noexcept { // PRIVATE -> static, modernized signature (was void), noexcept
    /* Color display uses 0x378 for printer; mono display uses 0x3BC. */

    int i;
    extern int color;

    port_base = (color ? PR_COLOR_BASE : PR_MONO_BASE);
    pr_busy = FALSE;
    port_out(port_base + 2, INIT_PRINTER); // port_out is noexcept
    for (i = 0; i < DELAY_COUNT; i++)
        ;                            /* delay loop */
    port_out(port_base + 2, SELECT); // port_out is noexcept
}

/*===========================================================================*
 *				pr_char				     *
 *===========================================================================*/
PUBLIC void pr_char() noexcept { // Added void return, noexcept
    /* This is the interrupt handler.  When a character has been printed, an
     * interrupt occurs, and the assembly code routine trapped to calls pr_char().
     * One annoying problem is that the 8259A controller sometimes generates
     * spurious interrupts to vector 15, which is the printer vector.  Ignore them.
     */

    int value, ch, i;
    char c;
    extern unsigned char get_byte(unsigned int seg,
                                  unsigned int off) noexcept; // Assuming modernized get_byte

    // pcount and orig_count are std::size_t
    if (pcount != orig_count)
        port_out(INT_CTL, ENABLE); // port_out is noexcept
    if (pr_busy == FALSE)
        return; /* spurious 8259A interrupt */

    while (pcount > 0) {                // pcount is std::size_t
        port_in(port_base + 1, &value); /* get printer status (port_in is noexcept) */
        if (value == NORMAL_STATUS) {
            /* Everything is all right.  Output another character. */
            // es, offset are int. get_byte expects unsigned.
            c = static_cast<char>(
                get_byte(static_cast<unsigned int>(es), static_cast<unsigned int>(offset)));
            ch = c & BYTE;                          // BYTE is int
            port_out(port_base, ch);                // port_out is noexcept
            port_out(port_base + 2, ASSERT_STROBE); // port_out is noexcept
            port_out(port_base + 2, NEGATE_STROBE); // port_out is noexcept
            offset++;                               // offset is int
            pcount--;                               // pcount is std::size_t
            cum_count++;                            /* count characters output (int) */
            for (i = 0; i < DELAY_COUNT; i++)
                ; /* delay loop */
        } else if (value == BUSY_STATUS) {
            return; /* printer is busy; wait for interrupt*/
        } else {
            break; /* err: send message to printer task */
        }
    }

    /* Count is 0 or an error occurred; send message to printer task. */
    int_mess.m_type = TTY_O_DONE;
    rep_status(int_mess) = (pcount == 0 ? OK : value);
    interrupt(PRINTER, &int_mess);
}
