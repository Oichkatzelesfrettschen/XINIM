/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   dev_open:	 called when a special file is opened
 *   dev_close:  called when a special file is closed
 *   dev_io:	 perform a read or write on a block or character device
 *   do_ioctl:	 perform the IOCTL system call
 *   rw_dev:	 procedure that actually calls the kernel tasks
 *   rw_dev2:	 procedure that actually calls task for /dev/tty
 *   no_call:	 dummy procedure (e.g., used when device need not be opened)
 */

#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "dev.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"
#include "type.hpp"

PRIVATE message dev_mess;
PRIVATE major, minor, task;
extern max_major;

/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
PUBLIC int dev_open(dev, mod)
dev_nr dev; /* which device to open */
int mod;    /* how to open it */
{
    /* Special files may need special processing upon open. */

    find_dev(dev);
    (*dmap[major].dmap_open)(task, &dev_mess);
    return (rep_status(dev_mess));
}

/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
PUBLIC dev_close(dev)
dev_nr dev; /* which device to close */
{
    /* This procedure can be used when a special file needs to be closed. */

    find_dev(dev);
    (*dmap[major].dmap_close)(task, &dev_mess);
}

/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
PUBLIC int dev_io(rw_flag, dev, pos, bytes, proc, buff)
int rw_flag; /* READING or WRITING */
dev_nr dev;  /* major-minor device number */
long pos;    /* byte position */
int bytes;   /* how many bytes to transfer */
int proc;    /* in whose address space is buff? */
char *buff;  /* virtual address of the buffer */
{
    /* Read or write from a device.  The parameter 'dev' tells which one. */

    find_dev(dev);

    /* Set up the message passed to task. */
    dev_mess.m_type = (rw_flag == READING ? DISK_READ : DISK_WRITE);
    device(dev_mess) = (dev >> MINOR) & BYTE;
    position(dev_mess) = pos;
    proc_nr(dev_mess) = proc;
    address(dev_mess) = buff;
    count(dev_mess) = bytes;

    /* Call the task. */
    (*dmap[major].dmap_rw)(task, &dev_mess);

    /* Task has completed.  See if call completed. */
    if (rep_status(dev_mess) == SUSPEND)
        suspend(task); /* suspend user */

    return (rep_status(dev_mess));
}

/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
PUBLIC do_ioctl() {
    /* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

    struct filp *f;
    register struct inode *rip;
    extern struct filp *get_filp();

    if ((f = get_filp(ls_fd)) == NIL_FILP)
        return (err_code);
    rip = f->filp_ino; /* get inode pointer */
    if ((rip->i_mode & I_TYPE) != I_CHAR_SPECIAL)
        return (ErrorCode::ENOTTY);
    find_dev(rip->i_zone[0]);

    dev_mess.m_type = TTY_IOCTL;
    proc_nr(dev_mess) = who;
    tty_line(dev_mess) = minor;
    tty_request(dev_mess) = tty_request(m);
    tty_spek(dev_mess) = tty_spek(m);
    tty_flags(dev_mess) = tty_flags(m);

    /* Call the task. */
    (*dmap[major].dmap_rw)(task, &dev_mess);

    /* Task has completed.  See if call completed. */
    if (dev_mess.m_type == SUSPEND)
        suspend(task);                   /* User must be suspended. */
    tty_spek(m1) = tty_spek(dev_mess);   /* erase and kill */
    tty_flags(m1) = tty_flags(dev_mess); /* flags */
    return (rep_status(dev_mess));
}

/*===========================================================================*
 *				find_dev				     *
 *===========================================================================*/
PRIVATE find_dev(dev)
dev_nr dev; /* device */
{
    /* Extract the major and minor device number from the parameter. */

    major = (dev >> MAJOR) & BYTE; /* major device number */
    minor = (dev >> MINOR) & BYTE; /* minor device number */
    if (major == 0 || major >= max_major)
        panic("bad major dev", major);
    task = dmap[major].dmap_task; /* which task services the device */
    device(dev_mess) = minor;
}

/*===========================================================================*
 *				rw_dev					     *
 *===========================================================================*/
PUBLIC rw_dev(task_nr, mess_ptr)
int task_nr;       /* which task to call */
message *mess_ptr; /* pointer to message for task */
{
    /* All file system I/O ultimately comes down to I/O on major/minor device
     * pairs.  These lead to calls on the following routines via the dmap table.
     */

    int proc_nr;

    proc_nr = proc_nr(*mess_ptr);

    if (sendrec(task_nr, mess_ptr) != OK)
        panic("rw_dev: can't send", NO_NUM);
    while (rep_proc_nr(*mess_ptr) != proc_nr) {
        /* Instead of the reply to this request, we got a message for an
         * earlier request.  Handle it and go receive again.
         */
        revive(rep_proc_nr(*mess_ptr), rep_status(*mess_ptr));
        receive(task_nr, mess_ptr);
    }
}

/*===========================================================================*
 *				rw_dev2					     *
 *===========================================================================*/
PUBLIC rw_dev2(dummy, mess_ptr)
int dummy;         /* not used - for compatibility with rw_dev() */
message *mess_ptr; /* pointer to message for task */
{
    /* This routine is only called for one device, namely /dev/tty.  It's job
     * is to change the message to use the controlling terminal, instead of the
     * major/minor pair for /dev/tty itself.
     */

    int task_nr, major_device;

    major_device = (fp->fs_tty >> MAJOR) & BYTE;
    task_nr = dmap[major_device].dmap_task; /* task for controlling tty */
    device(*mess_ptr) = (fp->fs_tty >> MINOR) & BYTE;
    rw_dev(task_nr, mess_ptr);
}

/*===========================================================================*
 *				no_call					     *
 *===========================================================================*/
PUBLIC int no_call(task_nr, m_ptr)
int task_nr;    /* which task */
message *m_ptr; /* message pointer */
{
    /* Null operation always succeeds. */

    rep_status(*m_ptr) = OK;
}
