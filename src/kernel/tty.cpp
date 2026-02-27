/* This file contains the terminal driver.
 */

#include <cstddef>
#include <cstdint>
#include "early/serial_16550.hpp"
#include <cstring>

#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include "sys/callnr.hpp"
#include "sgtty.hpp"

#define NR_TTYS 1         
#define TTY_IN_BYTES 200  
#define TTY_RAM_WORDS 320 
#define TTY_BUF_SIZE 256  
#define TAB_SIZE 8        
#define TAB_MASK 07       
#define MAX_OVERRUN 16    

struct tty_struct {
    int tty_events;
    int tty_mode;
    int tty_column;
    int tty_outleft;
    int tty_outcum;
    int tty_inhibited;
    int tty_pgrp;
    int tty_opened;
    int tty_pnr;
    int tty_caller;
    char *tty_outptr;
    int tty_incount;
    char *tty_inhead;
    char *tty_intail;
    char tty_inbuf[TTY_IN_BYTES];
};

static tty_struct tty_table[NR_TTYS];

extern "C" void tty_sig_init() noexcept;

extern "C" {
    void vid_copy(const void *src, unsigned dst, unsigned offset, int words) noexcept;
    void p_dmp() noexcept;
    void map_dmp() noexcept;
}

static message keybd_mess;

extern xinim::early::Serial16550 early_serial;

extern "C" void tty_task() noexcept {
    message tty_mess;
    tty_sig_init();
    while (TRUE) {
        ipc_receive(ANY, &tty_mess);

        // Process messages instead of silently discarding them
        switch (tty_mess.m_type) {
            case TTY_WRITE: {
                // Forward write buffer to serial console
                const char* buf = reinterpret_cast<const char*>(
                    static_cast<uintptr_t>(tty_mess.m1_i1()));
                int count = tty_mess.m1_i2();
                if (buf && count > 0) {
                    for (int i = 0; i < count; ++i) {
                        early_serial.write_char(buf[i]);
                    }
                }
                // Reply with bytes written
                message reply;
                reply.m_type = TASK_REPLY;
                reply.m1_i1() = count;
                ipc_send(tty_mess.m_source, &reply);
                break;
            }
            case TTY_READ: {
                // Read not yet implemented; reply with 0 bytes
                message reply;
                reply.m_type = TASK_REPLY;
                reply.m1_i1() = 0;
                ipc_send(tty_mess.m_source, &reply);
                break;
            }
            case TTY_CHAR_INT:
                // Keyboard character received; buffer for later read
                break;
            default:
                // Unknown message type; ignore
                break;
        }
    }
}

static void tty_signal(tty_struct *tp, int sig) {
    int line = static_cast<int>(tp - tty_table);
    if (tp->tty_pgrp != 0) {
        cause_sig(LOW_USER + 1 + line, sig);
    }
}

#define KEYBD 0x60
#define PORT_B 0x61
#define KBIT 0x80

extern "C" void keyboard_interrupt() noexcept {
    unsigned int code, val;
    int port_b = PORT_B;
    port_in(KEYBD, &code);
    port_in(PORT_B, &val);
    port_out(static_cast<unsigned>(port_b), static_cast<unsigned>(val | KBIT));
    port_out(static_cast<unsigned>(port_b), val);
    
    if (code == 0x53) { // DEL
        reboot();
    }
    
    keybd_mess.m_type = TTY_CHAR_INT; 
    keybd_mess.m1_i1() = static_cast<int>(code);
    interrupt(TTY, &keybd_mess);
}

// Dummy use to satisfy -Werror
void tty_dummy_use() {
    (void)tty_table;
    (void)tty_signal;
}
