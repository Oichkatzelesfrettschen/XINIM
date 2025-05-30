/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* MM must occasionally print some message.  It uses the standard library
 * routine prink().  (The name "printf" is really a macro defined as "printk").
 * Printing is done by calling the TTY task directly, not going through FS.
 */

#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"

#define STD_OUTPUT 1 /* file descriptor for standard output */
#define BUF_SIZE 100 /* print buffer size */

PRIVATE int buf_count;            /* # characters in the buffer */
PRIVATE char print_buf[BUF_SIZE]; /* output is buffered here */
PRIVATE message putch_msg;        /* used for message to TTY task */

/*===========================================================================*
 *				putc					     *
 *===========================================================================*/
PUBLIC putc(c)
char c;
{

    /* Accumulate another character.  If '\n' or buffer full, print it. */
    print_buf[buf_count++] = c;
    if (buf_count == BUF_SIZE)
        F_l_u_s_h();
    if (c == '\n')
        F_l_u_s_h();
}

/*===========================================================================*
 *				F_l_u_s_h				     *
 *===========================================================================*/
PRIVATE F_l_u_s_h() {
    /* Flush the print buffer by calling TTY task. */

    if (buf_count == 0)
        return;
    putch_msg.m_type = TTY_WRITE;
    putch_msg.PROC_NR = 0;
    putch_msg.TTY_LINE = 0;
    putch_msg.ADDRESS = print_buf;
    putch_msg.COUNT = buf_count;
    sendrec(TTY, &putch_msg);
    buf_count = 0;
}
