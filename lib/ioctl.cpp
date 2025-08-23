#include "../h/com.hpp"
#include "../include/lib.hpp" // C++23 header
#include "../include/sgtty.hpp"

// Helper union type encapsulating the possible ioctl argument structures.
union IoctlArg {
    sgttyb *argp; ///< Pointer to sgttyb parameters
    tchars *argt; ///< Pointer to terminal control characters
};

// Perform an I/O control operation on a terminal device.
int ioctl(int fd, int request, IoctlArg u)

{
    int n;
    long erase, kill, intr, quit, xon, xoff, eof, brk;

    tty_request(M) = request;
    tty_line(M) = fd;

    switch (request) {
    case TIOCSETP:
        erase = u.argp->sg_erase & 0377;
        kill = u.argp->sg_kill & 0377;
        tty_spek(M) = (erase << 8) | kill;
        tty_flags(M) = u.argp->sg_flags;
        n = callx(FS, IOCTL);
        return (n);

    case TIOCSETC:
        intr = u.argt->t_intrc & 0377;
        quit = u.argt->t_quitc & 0377;
        xon = u.argt->t_startc & 0377;
        xoff = u.argt->t_stopc & 0377;
        eof = u.argt->t_eofc & 0377;
        brk = u.argt->t_brkc & 0377; /* not used at the moment */
        tty_spek(M) = (intr << 24) | (quit << 16) | (xon << 8) | (xoff << 0);
        tty_flags(M) = (eof << 8) | (brk << 0);
        n = callx(FS, IOCTL);
        return (n);

    case TIOCGETP:
        n = callx(FS, IOCTL);
        u.argp->sg_erase = (tty_spek(M) >> 8) & 0377;
        u.argp->sg_kill = (tty_spek(M) >> 0) & 0377;
        u.argp->sg_flags = tty_flags(M);
        return (n);

    case TIOCGETC:
        n = callx(FS, IOCTL);
        u.argt->t_intrc = (tty_spek(M) >> 24) & 0377;
        u.argt->t_quitc = (tty_spek(M) >> 16) & 0377;
        u.argt->t_startc = (tty_spek(M) >> 8) & 0377;
        u.argt->t_stopc = (tty_spek(M) >> 0) & 0377;
        u.argt->t_eofc = (tty_flags(M) >> 8) & 0377;
        u.argt->t_brkc = (tty_flags(M) >> 8) & 0377;
        return (n);

    default:
        n = -1;
        errno = -static_cast<int>(ErrorCode::EINVAL);
        return (n);
    }
}
