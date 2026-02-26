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

#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "dev.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"

PRIVATE message dev_mess;
PRIVATE int major = 0;
PRIVATE int minor = 0;
PRIVATE int task = 0;
extern int max_major;

[[noreturn]] void panic(const char *format, int num = NO_NUM);
void revive(int proc_nr, int status);
void suspend(int task);
int sendrec(int dest, message *m_ptr);
int receive(int source, message *m_ptr);
PRIVATE void find_dev(dev_nr dev) noexcept;
struct filp *get_filp(int fild);

/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
/**
 * @brief Open a device and delegate to the driver.
 * @param dev Device to open.
 * @param mod Access mode flags.
 * @return Driver status code.
 */
PUBLIC int dev_open(dev_nr dev, int mod) noexcept {
    /* Special files may need special processing upon open. */
    (void)mod;

    find_dev(dev);
    (*dmap[major].dmap_open)(task, &dev_mess);
    return (rep_status(dev_mess));
}

/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
/**
 * @brief Close a device and delegate to the driver.
 * @param dev Device to close.
 * @return Driver status code.
 */
PUBLIC int dev_close(dev_nr dev) noexcept {
    /* This procedure can be used when a special file needs to be closed. */

    find_dev(dev);
    (*dmap[major].dmap_close)(task, &dev_mess);
    return (rep_status(dev_mess));
}

/*===========================================================================*
 *				dev_io					     *
 *===========================================================================*/
/**
 * @brief Perform I/O on a block or character device.
 * @param rw_flag READING or WRITING.
 * @param dev     Major/minor device number.
 * @param pos     Byte position.
 * @param bytes   Byte count to transfer.
 * @param proc    Process address space for the buffer.
 * @param buff    Virtual address of the buffer.
 * @return Driver status code.
 */
PUBLIC int dev_io(int rw_flag, dev_nr dev, long pos, int bytes, int proc, char *buff) noexcept {
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
/**
 * @brief Perform the ioctl system call for a character device.
 * @return Status code from the driver or ::ErrorCode::ENOTTY.
 */
PUBLIC int do_ioctl() noexcept {
    /* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

    struct filp *f;
    struct inode *rip;
    if ((f = get_filp(ls_fd)) == NIL_FILP)
        return (err_code);
    rip = f->filp_ino; /* get inode pointer */
    if ((rip->i_mode & I_TYPE) != I_CHAR_SPECIAL)
        return static_cast<int>(ErrorCode::ENOTTY);
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
/**
 * @brief Resolve a device number into major/minor/task fields.
 * @param dev Device to resolve.
 */
PRIVATE void find_dev(dev_nr dev) noexcept {
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
/**
 * @brief Send a request to a device task and wait for the matching reply.
 * @param task_nr  Task to contact.
 * @param mess_ptr Request/response message.
 * @return ::OK when the transaction completes.
 */
PUBLIC int rw_dev(int task_nr, message *mess_ptr) noexcept {
    /* All file system I/O ultimately comes down to I/O on major/minor device
     * pairs.  These lead to calls on the following routines via the dmap table.
     */

    int target_proc;

    target_proc = proc_nr(*mess_ptr);

    if (sendrec(task_nr, mess_ptr) != OK)
        panic("rw_dev: can't send", NO_NUM);
    while (rep_proc_nr(*mess_ptr) != target_proc) {
        /* Instead of the reply to this request, we got a message for an
         * earlier request.  Handle it and go receive again.
         */
        revive(rep_proc_nr(*mess_ptr), rep_status(*mess_ptr));
        receive(task_nr, mess_ptr);
    }
    return (OK);
}

/*===========================================================================*
 *				rw_dev2					     *
 *===========================================================================*/
/**
 * @brief Handle /dev/tty by redirecting to the controlling terminal task.
 * @param dummy    Unused (compatibility with ::rw_dev()).
 * @param mess_ptr Request/response message.
 * @return ::OK when complete.
 */
PUBLIC int rw_dev2(int dummy, message *mess_ptr) noexcept {
    /* This routine is only called for one device, namely /dev/tty.  It's job
     * is to change the message to use the controlling terminal, instead of the
     * major/minor pair for /dev/tty itself.
     */

    int task_nr, major_device;

    major_device = (fp->fs_tty >> MAJOR) & BYTE;
    task_nr = dmap[major_device].dmap_task; /* task for controlling tty */
    device(*mess_ptr) = (fp->fs_tty >> MINOR) & BYTE;
    (void)dummy;
    return rw_dev(task_nr, mess_ptr);
}

/*===========================================================================*
 *				no_call					     *
 *===========================================================================*/
/**
 * @brief Dummy device handler that always succeeds.
 * @param task_nr Unused task identifier.
 * @param m_ptr   Message to update.
 * @return ::OK always.
 */
PUBLIC int no_call(int task_nr, message *m_ptr) noexcept {
    /* Null operation always succeeds. */

    rep_status(*m_ptr) = OK;
    (void)task_nr;
    return (OK);
}
