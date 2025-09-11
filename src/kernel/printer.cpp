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

#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstddef> // For std::size_t
#include <cstdint> // For uint64_t
#include <cstdio>  // For output
#include <format>  // For std::format
#include <memory>  // For RAII device wrapper

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

/**
 * @brief RAII wrapper for the parallel printer device.
 *
 * The constructor initializes the hardware interface while the destructor
 * returns the device to a safe state.
 */
class PrinterDevice {
  public:
    /**
     * @brief Construct a new PrinterDevice.
     * @param base I/O base port used to access the printer.
     */
    explicit PrinterDevice(int base) noexcept : base_{base} {
        port_out(base_ + 2, INIT_PRINTER);
        for (int i = 0; i < DELAY_COUNT; ++i) {
            // busy wait during initialization
        }
        port_out(base_ + 2, SELECT);
    }

    /**
     * @brief Destroy the PrinterDevice and reset the hardware.
     */
    ~PrinterDevice() noexcept { port_out(base_ + 2, INIT_PRINTER); }

    /**
     * @brief Retrieve the base port number.
     * @return Base port for subsequent I/O operations.
     */
    [[nodiscard]] int base() const noexcept { return base_; }

  private:
    int base_; /**< Base port for the printer device. */
};

static std::unique_ptr<PrinterDevice> g_printer_device; /**< Active printer device. */
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
/**
 * @brief Entry point for the printer task.
 *
 * Handles incoming messages and dispatches them to the appropriate
 * sub‑routines.
 */
PUBLIC void printer_task() noexcept {
    message print_mess; /**< Buffer for all incoming messages. */

    print_init();

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
/**
 * @brief Handle a TTY_WRITE request from user space.
 *
 * @param m_ptr Message describing the write request.
 */
[[maybe_unused]] static void do_write(message *m_ptr) noexcept {
    int i, j, r, value;
    struct proc *rp;
    uint64_t phys;

    r = OK;

    if (pr_busy)
        r = ErrorCode::EAGAIN;
    if (count(*m_ptr) <= 0)
        r = ErrorCode::EINVAL;

    rp = proc_addr(proc_nr(*m_ptr));
    phys = umap(rp, D, reinterpret_cast<std::size_t>(address(*m_ptr)),
                static_cast<std::size_t>(count(*m_ptr)));
    if (phys == 0)
        r = ErrorCode::E_BAD_ADDR;

    if (r == OK) {
        lock();
        caller = m_ptr->m_source;
        requesting_proc = proc_nr(*m_ptr);
        pcount = static_cast<std::size_t>(count(*m_ptr));
        orig_count = static_cast<std::size_t>(count(*m_ptr));
        es = static_cast<int>(phys >> CLICK_SHIFT);
        offset = static_cast<int>(phys & LOW_FOUR);

        for (i = 0; i < MAX_REP; i++) {
            port_in(g_printer_device->base() + 1, &value);
            if (value == NORMAL_STATUS) {
                pr_busy = TRUE;
                pr_char();
                r = SUSPEND;
                break;
            }
            if (value == BUSY_STATUS) {
                for (j = 0; j < DELAY_LOOP; j++)
                    ;
                continue;
            }
            pr_error(value);
            r = ErrorCode::EIO;
            break;
        }
    }

    if (value == BUSY_STATUS)
        r = ErrorCode::EAGAIN;
    reply(TASK_REPLY, m_ptr->m_source, proc_nr(*m_ptr), r);
}

/*===========================================================================*
 *				do_done					     *
 *===========================================================================*/
/**
 * @brief Handle completion of a print job.
 *
 * @param m_ptr Message describing the completion event.
 */
[[maybe_unused]] static void do_done(message *m_ptr) noexcept {
    int status = (rep_status(*m_ptr) == OK ? orig_count : ErrorCode::EIO);
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
/**
 * @brief Cancel a previously started print operation.
 *
 * @param m_ptr Pointer to the cancel message.
 */
[[maybe_unused]] static void do_cancel(message *m_ptr) noexcept {
    if (pr_busy == FALSE)
        return;
    pr_busy = FALSE;
    pcount = 0;
    requesting_proc = CANCELED;
    reply(TASK_REPLY, m_ptr->m_source, proc_nr(*m_ptr), ErrorCode::EINTR);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
/**
 * @brief Send a reply to the file system.
 *
 * @param code Message type (TASK_REPLY or REVIVE).
 * @param replyee Destination process.
 * @param process User process involved.
 * @param status Status or byte count.
 */
[[maybe_unused]] static void reply(int code, int replyee, int process, int status) noexcept {
    message pr_mess;
    pr_mess.m_type = code;
    rep_status(pr_mess) = status;
    rep_proc_nr(pr_mess) = process;
    send(replyee, &pr_mess);
}

/*===========================================================================*
 *				pr_error				     *
 *===========================================================================*/
/**
 * @brief Report hardware error conditions.
 *
 * @param status Status register bits returned by the device.
 */
[[maybe_unused]] static void pr_error(int status) noexcept {
    if (status & NO_PAPER)
        std::fputs(std::format("Printer is out of paper\n").c_str(), stdout);
    if ((status & OFF_LINE) == 0)
        std::fputs(std::format("Printer is not on line\n").c_str(), stdout);
    if ((status & PR_ERROR) == 0)
        std::fputs(std::format("Printer error\n").c_str(), stdout);
}

/*===========================================================================*
 *				print_init				     *
 *===========================================================================*/
/**
 * @brief Initialize the printer device and prepare the handle.
 */
[[maybe_unused]] static void print_init() noexcept {
    extern int color;
    g_printer_device = std::make_unique<PrinterDevice>(color ? PR_COLOR_BASE : PR_MONO_BASE);
    pr_busy = FALSE;
}

/*===========================================================================*
 *				pr_char				     *
 *===========================================================================*/
/**
 * @brief Interrupt handler invoked after a character is printed.
 */
PUBLIC void pr_char() noexcept {
    int value, ch, i;
    char c;
    extern unsigned char get_byte(unsigned int seg, unsigned int off) noexcept;

    if (pcount != orig_count)
        port_out(INT_CTL, ENABLE);
    if (pr_busy == FALSE)
        return;

    while (pcount > 0) {
        port_in(g_printer_device->base() + 1, &value);
        if (value == NORMAL_STATUS) {
            c = static_cast<char>(
                get_byte(static_cast<unsigned int>(es), static_cast<unsigned int>(offset)));
            ch = c & BYTE;
            port_out(g_printer_device->base(), ch);
            port_out(g_printer_device->base() + 2, ASSERT_STROBE);
            port_out(g_printer_device->base() + 2, NEGATE_STROBE);
            offset++;
            pcount--;
            cum_count++;
            for (i = 0; i < DELAY_COUNT; i++)
                ;
        } else if (value == BUSY_STATUS) {
            return;
        } else {
            break;
        }
    }

    int_mess.m_type = TTY_O_DONE;
    rep_status(int_mess) = (pcount == 0 ? OK : value);
    interrupt(PRINTER, &int_mess);
}
