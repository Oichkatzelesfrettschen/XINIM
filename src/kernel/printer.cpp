/* This file contains the printer driver. It is a fairly simple driver,
 * supporting only one printer.
 */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/callnr.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include "console.hpp"

#define NORMAL_STATUS 0xDF
#define BUSY_STATUS 0x5F
#define ASSERT_STROBE 0x1D
#define NEGATE_STROBE 0x1C
#define SELECT 0x0C
#define INIT_PRINTER 0x08
#define NO_PAPER 0x20
#define OFF_LINE 0x10
#define PR_ERR_BIT 0x08 
#define PR_COLOR_BASE 0x378
#define PR_MONO_BASE 0x3BC
#define LOW_FOUR 0xF
#define CANCELED_VAL -999 
#define DELAY_COUNT 100
#define DELAY_LOOP 1000
#define MAX_REP 1000

// External kernel functions not in proc.hpp
extern "C" {
    unsigned char get_byte(unsigned int seg, unsigned int off) noexcept;
    void pr_char() noexcept; 
}

class PrinterDevice {
  public:
    explicit PrinterDevice(int base) noexcept : base_{base} {
        port_out(static_cast<unsigned>(base_ + 2), INIT_PRINTER);
        for (int i = 0; i < DELAY_COUNT; ++i) { } 
        port_out(static_cast<unsigned>(base_ + 2), SELECT);
    }
    ~PrinterDevice() noexcept { port_out(static_cast<unsigned>(base_ + 2), INIT_PRINTER); }
    [[nodiscard]] int base() const noexcept { return base_; }
  private:
    int base_;
};

static std::unique_ptr<PrinterDevice> g_printer_device;
static int caller_proc; 
static int req_proc;    
static std::size_t o_count; 
static int seg_part;     
static int off_part;     
PUBLIC std::size_t pcount;
PUBLIC int pr_busy;
PUBLIC int cum_count;
PUBLIC int prev_ct;

// Forward declarations
static void do_write(message *m_ptr) noexcept;
static void do_done(message *m_ptr) noexcept;
static void do_cancel(message *m_ptr) noexcept;
static void print_init() noexcept;
static void reply(int code, int replyee, int status) noexcept;
static void pr_error(int status) noexcept;

extern "C" void printer_task() noexcept {
    message print_mess;
    print_init();
    while (TRUE) {
        ipc_receive(ANY, &print_mess);
        switch (print_mess.m_type) {
        case TTY_WRITE: do_write(&print_mess); break;
        case CANCEL:    do_cancel(&print_mess); break;
        case TTY_O_DONE: do_done(&print_mess); break;
        default: break;
        }
    }
}

static void do_write(message *m_ptr) noexcept {
    int i, j, r;
    unsigned int value = 0;
    struct proc *rp;
    uint64_t phys;

    r = OK;
    if (pr_busy) r = static_cast<int>(ErrorCode::EAGAIN);
    if (count(*m_ptr) <= 0) r = static_cast<int>(ErrorCode::EINVAL);

    rp = proc_addr(proc_nr(*m_ptr));
    phys = umap(rp, D, reinterpret_cast<std::size_t>(address(*m_ptr)),
                static_cast<std::size_t>(count(*m_ptr)));
    if (phys == 0) r = static_cast<int>(ErrorCode::E_BAD_ADDR);

    if (r == OK) {
        lock();
        caller_proc = m_ptr->m_source;
        req_proc = proc_nr(*m_ptr);
        pcount = static_cast<std::size_t>(count(*m_ptr));
        o_count = static_cast<std::size_t>(count(*m_ptr));
        seg_part = static_cast<int>(phys >> CLICK_SHIFT);
        off_part = static_cast<int>(phys & LOW_FOUR);

        for (i = 0; i < MAX_REP; i++) {
            port_in(static_cast<unsigned>(g_printer_device->base() + 1), &value);
            if (value == NORMAL_STATUS) {
                pr_busy = TRUE;
                pr_char();
                r = SUSPEND;
                break;
            }
            if (value == BUSY_STATUS) {
                for (j = 0; j < DELAY_LOOP; j++) ; 
                continue;
            }
            pr_error(static_cast<int>(value));
            r = static_cast<int>(ErrorCode::EIO);
            break;
        }
    }

    if (value == BUSY_STATUS) r = static_cast<int>(ErrorCode::EAGAIN);
    reply(TASK_REPLY, m_ptr->m_source, r);
}

static void do_done(message *m_ptr) noexcept {
    int status = (rep_status(*m_ptr) == OK ? static_cast<int>(o_count) : static_cast<int>(ErrorCode::EIO));
    if (req_proc != CANCELED_VAL) {
        reply(REVIVE, caller_proc, status);
        if (status == static_cast<int>(ErrorCode::EIO))
            pr_error(rep_status(*m_ptr));
    }
    pr_busy = FALSE;
}

static void do_cancel(message *m_ptr) noexcept {
    if (pr_busy == FALSE) return;
    pr_busy = FALSE;
    pcount = 0;
    req_proc = CANCELED_VAL;
    reply(TASK_REPLY, m_ptr->m_source, static_cast<int>(ErrorCode::EINTR));
}

static void reply(int code, int replyee, int status) noexcept {
    message pr_mess;
    pr_mess.m_type = code;
    rep_status(pr_mess) = status;
    ipc_send(replyee, &pr_mess);
}

static void pr_error(int status) noexcept {
    if (status & NO_PAPER) Console::printf("Printer is out of paper\n");
    if ((status & OFF_LINE) == 0) Console::printf("Printer is not on line\n");
    if ((status & PR_ERR_BIT) == 0) Console::printf("Printer error\n");
}

static void print_init() noexcept {
    extern int color;
    g_printer_device = std::make_unique<PrinterDevice>(color ? PR_COLOR_BASE : PR_MONO_BASE);
    pr_busy = FALSE;
}

extern "C" void pr_char() noexcept {
    unsigned int value;
    int ch, i;
    char c;

    if (pcount != o_count) port_out(INT_CTL, ENABLE);
    if (pr_busy == FALSE) return;

    while (pcount > 0) {
        port_in(static_cast<unsigned>(g_printer_device->base() + 1), &value);
        if (value == NORMAL_STATUS) {
            c = static_cast<char>(
                get_byte(static_cast<unsigned int>(seg_part), static_cast<unsigned int>(off_part)));
            ch = c & 0xFF;
            port_out(static_cast<unsigned>(g_printer_device->base()), static_cast<unsigned>(ch));
            port_out(static_cast<unsigned>(g_printer_device->base() + 2), ASSERT_STROBE);
            port_out(static_cast<unsigned>(g_printer_device->base() + 2), NEGATE_STROBE);
            off_part++;
            pcount--;
            cum_count++;
            for (i = 0; i < DELAY_COUNT; i++) ; 
        } else if (value == BUSY_STATUS) {
            return;
        } else {
            break;
        }
    }

    int_mess.m_type = TTY_O_DONE;
    rep_status(int_mess) = (pcount == 0 ? OK : static_cast<int>(value));
    interrupt(PRINTER, &int_mess);
}
