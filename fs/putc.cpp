/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* FS must occasionally print some message.  It uses the standard library
 * routine printf(), which calls putc() and flush. Library
 * versions of these routines do printing by sending messages to FS.  Here
 * obviously can't do that, so FS calls the TTY task directly.
 */

#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"

#define STDOUTPUT 1 /* file descriptor for standard output */
#define BUFSIZE 100 /* print buffer size */

PRIVATE int bufcount;           /* # characters in the buffer */
PRIVATE char printbuf[BUFSIZE]; /* output is buffered here */
PRIVATE message putchmsg;       /* used for message to TTY task */

/*===========================================================================*
 *				putc					     *
 *===========================================================================*/
PUBLIC putc(c)
char c;
{

    if (c == 0) {
        flush();
        return;
    }
    printbuf[bufcount++] = c;
    if (bufcount == BUFSIZE)
        flush();
    if (c == '\n')
        flush();
}

/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
PRIVATE flush() {
    /* Flush the print buffer. */

    if (bufcount == 0)
        return;
    putchmsg.m_type = TTY_WRITE;
    proc_nr(putchmsg) = 1;
    tty_line(putchmsg) = 0;
    address(putchmsg) = printbuf;
    count(putchmsg) = bufcount;
    sendrec(TTY, &putchmsg);
    bufcount = 0;
}
