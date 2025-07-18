/* This file contains a driver for the IBM or DTC winchester controller.
 * It was written by Adri Koppes.
 *
 * The driver supports two operations: read a block and
 * write a block.  It accepts two messages, one for reading and one for
 * writing, both using message format m2 and with the same parameters:
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   winchester_task:	main entry when system is brought up
 *
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <algorithm> // For std::min if needed, though not apparent yet
#include <cstddef>   // For std::size_t, nullptr
#include <cstdint>   // For uint64_t, uint16_t etc.

/* RAII helper ensuring critical sections use lock/unlock */
class ScopedPortLock {
  public:
    ScopedPortLock() { lock(); }
    ~ScopedPortLock() { unlock(); }
};

/* I/O Ports used by winchester disk task. */
#define WIN_DATA 0x320   /* winchester disk controller data register */
#define WIN_STATUS 0x321 /* winchester disk controller status register */
#define WIN_SELECT 0x322 /* winchester disk controller select port */
#define WIN_DMA 0x323    /* winchester disk controller dma register */
#define DMA_ADDR 0x006   /* port for low 16 bits of DMA address */
#define DMA_TOP 0x082    /* port for top 4 bits of 20-bit DMA addr */
#define DMA_COUNT 0x007  /* port for DMA count (count =  bytes - 1) */
#define DMA_M2 0x00C     /* DMA status port */
#define DMA_M1 0x00B     /* DMA status port */
#define DMA_INIT 0x00A   /* DMA init port */

/* Winchester disk controller command bytes. */
#define WIN_RECALIBRATE 0x01 /* command for the drive to recalibrate */
#define WIN_SENSE 0x03       /* command for the controller to get its status */
#define WIN_READ 0x08        /* command for the drive to read */
#define WIN_WRITE 0x0a       /* command for the drive to write */
#define WIN_SPECIFY 0x0C     /* command for the controller to accept params */
#define WIN_ECC_READ 0x0D    /* command for the controller to read ecc length */

#define DMA_INT 3    /* Command with dma and interrupt */
#define INT 2        /* Command with interrupt, no dma */
#define NO_DMA_INT 0 /* Command without dma and interrupt */
#define CTRL_BYTE 5  /* Control byte for controller */

/* DMA channel commands. */
#define DMA_READ 0x47  /* DMA read opcode */
#define DMA_WRITE 0x4B /* DMA write opcode */

/* Parameters for the disk drive. */
#define SECTOR_SIZE 512 /* physical sector size in bytes */
#define NR_SECTORS 0x11 /* number of sectors per track */

/* Error codes */
#define ERR -1 /* general error */

/* Miscellaneous. */
#define MAX_ERRORS 4        /* how often to try rd/wt before quitting */
#define MAX_RESULTS 4       /* max number of bytes controller returns */
#define NR_DEVICES 10       /* maximum number of drives */
#define MAX_WIN_RETRY 10000 /* max # times to try to output to WIN */
#define PART_TABLE 0x1C6    /* IBM partition table starts here in sect 0 */
#define DEV_PER_DRIVE 5     /* hd0 + hd1 + hd2 + hd3 + hd4 = 5 */

/* Variables. */
PRIVATE struct wini {             /* main drive struct, one entry per drive */
    int wn_opcode;                /* DISK_READ or DISK_WRITE */
    int wn_procnr;                /* which proc wanted this operation? */
    int wn_drive;                 /* drive number addressed */
    int wn_cylinder;              /* cylinder number addressed */
    int wn_sector;                /* sector addressed */
    int wn_head;                  /* head number addressed */
    int wn_heads;                 /* maximum number of heads */
    uint64_t wn_low;              /* lowest cylinder of partition (was long, now block offset) */
    uint64_t wn_size;             /* size of partition in blocks (was long) */
    std::size_t wn_count;         /* byte count (was int) */
    std::size_t wn_address;       /* user virtual address (was vir_bytes) */
    char wn_results[MAX_RESULTS]; /* the controller can give lots of output */
} wini[NR_DEVICES];

PRIVATE int w_need_reset = FALSE; /* set to 1 when controller must be reset */
PRIVATE int nr_drives;            /* Number of drives */

PRIVATE message w_mess; /* message buffer for in and out */

PRIVATE int command[6]; /* Common command block */

PRIVATE unsigned char buf[BLOCK_SIZE]; /* Buffer used by the startup routine */

PRIVATE struct param {
    int nr_cyl;     /* Number of cylinders */
    int nr_heads;   /* Number of heads */
    int reduced_wr; /* First cylinder with reduced write current */
    int wr_precomp; /* First cylinder with write precompensation */
    int max_ecc;    /* Maximum ECC burst length */
} param0, param1;

/*===========================================================================*
 *				winchester_task				     *
 *===========================================================================*/
PUBLIC void winchester_task() noexcept { // Added void return, noexcept
    /* Main program of the winchester disk driver task. */

    int r, caller, proc_nr;

    /* First initialize the controller */
    init_param();

    /* Here is the main loop of the disk task.  It waits for a message, carries
     * it out, and sends a reply.
     */

    while (TRUE) {
        /* First wait for a request to read or write a disk block. */
        receive(ANY, &w_mess); /* get a request to do some work */
        if (w_mess.m_source < 0) {
            printf("winchester task got message from %d ", w_mess.m_source);
            continue;
        }
        caller = w_mess.m_source;
        proc_nr = proc_nr(w_mess);

        /* Now carry out the work. */
        switch (w_mess.m_type) {
        case DISK_READ:
        case DISK_WRITE:
            r = w_do_rdwt(&w_mess);
            break;
        default:
            r = ErrorCode::EINVAL;
            break;
        }

        /* Finally, prepare and send the reply message. */
        w_mess.m_type = TASK_REPLY;
        rep_proc_nr(w_mess) = proc_nr;

        rep_status(w_mess) = r; /* # of bytes transferred or error code */
        send(caller, &w_mess);  /* send reply to caller */
    }
}

/*===========================================================================*
 *				w_do_rdwt					     *
 *===========================================================================*/
/* Handle a read or write request from the disk. */
static int w_do_rdwt(message *m_ptr) noexcept { // Added noexcept
    /* Carry out a read or write request from the disk. */
    register struct wini *wn;
    int r, device, errors = 0;
    int64_t sector; // Was long, for m_ptr->POSITION (int64_t)

    /* Decode the w_message parameters. */
    device = device(*m_ptr);
    if (device < 0 || device >= NR_DEVICES)
        return (ErrorCode::EIO);
    if (count(*m_ptr) != BLOCK_SIZE)
        return (ErrorCode::EINVAL);
    wn = &wini[device];                    /* 'wn' points to entry for this drive */
    wn->wn_drive = device / DEV_PER_DRIVE; /* save drive number */
    if (wn->wn_drive >= nr_drives)
        return (ErrorCode::EIO);
    wn->wn_opcode = m_ptr->m_type; /* DISK_READ or DISK_WRITE */
    // m_ptr->POSITION is int64_t (modernized message field for m2_l1)
    // BLOCK_SIZE is int.
    if (position(*m_ptr) % BLOCK_SIZE != 0)
        return (ErrorCode::EINVAL);
    sector = position(*m_ptr) / SECTOR_SIZE; // SECTOR_SIZE is int
    // wn_size is uint64_t. Result of division will be small.
    if ((sector + static_cast<int64_t>(BLOCK_SIZE / SECTOR_SIZE)) >
        static_cast<int64_t>(wn->wn_size))
        return (EOF);
    sector += static_cast<int64_t>(wn->wn_low); // wn_low is uint64_t
    // Result of arithmetic should fit in int for cylinder/sector/head.
    wn->wn_cylinder = static_cast<int>(sector / (wn->wn_heads * NR_SECTORS));
    wn->wn_sector = static_cast<int>(sector % NR_SECTORS);
    wn->wn_head = static_cast<int>((sector % (wn->wn_heads * NR_SECTORS)) / NR_SECTORS);
    wn->wn_count = static_cast<std::size_t>(count(*m_ptr));
    wn->wn_address = reinterpret_cast<std::size_t>(address(*m_ptr));
    wn->wn_procnr = proc_nr(*m_ptr);

    /* This loop allows a failed operation to be repeated. */
    while (errors <= MAX_ERRORS) {
        errors++; /* increment count once per loop cycle */
        if (errors >= MAX_ERRORS)
            return (ErrorCode::EIO);

        /* First check to see if a reset is needed. */
        if (w_need_reset)
            w_reset();

        /* Now set up the DMA chip. */
        w_dma_setup(wn);

        /* Perform the transfer. */
        r = w_transfer(*wn);
        if (r == OK)
            break; /* if successful, exit loop */
    }

    return (r == OK ? BLOCK_SIZE : ErrorCode::EIO);
}

/*===========================================================================*
 *				w_dma_setup				     *
 *===========================================================================*/
/* Prepare the DMA chip for a transfer. */
static void w_dma_setup(struct wini *wn) noexcept { // Modernized param, noexcept. Renamed wini to
                                                    // struct wini for clarity.
    /* The IBM PC can perform DMA operations by using the DMA chip.  To use it,
     * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
     * to by read from or written to, the byte count minus 1, and a read or write
     * opcode.  This routine sets up the DMA chip.  Note that the chip is not
     * capable of doing a DMA across a 64K boundary (e.g., you can't read a
     * 512-byte block starting at physical address 65520).
     */

    int mode, low_addr, high_addr, top_addr, low_ct, high_ct, top_end;
    std::size_t vir, ct; // vir_bytes -> std::size_t
    uint64_t user_phys;  // phys_bytes -> uint64_t
    // extern phys_bytes umap(); // umap returns uint64_t

    mode = (wn->wn_opcode == DISK_READ ? DMA_READ : DMA_WRITE);
    vir = wn->wn_address; // wn_address is std::size_t
    ct = wn->wn_count;    // wn_count is std::size_t
    // umap takes (proc*, int, std::size_t, std::size_t) returns uint64_t
    user_phys = umap(proc_addr(wn->wn_procnr), D, vir, ct);
    // BYTE is int (0377). user_phys is uint64_t.
    low_addr = static_cast<int>(user_phys & BYTE);
    high_addr = static_cast<int>((user_phys >> 8) & BYTE);
    top_addr = static_cast<int>((user_phys >> 16) &
                                BYTE); // This implies physical addresses are <= 20 bits for DMA
    low_ct = static_cast<int>((ct - 1) & BYTE); // ct is std::size_t
    high_ct = static_cast<int>(((ct - 1) >> 8) & BYTE);

    /* Check to see if the transfer will require the DMA address counter to
     * go from one 64K segment to another.  If so, do not even start it, since
     * the hardware does not carry from bit 15 to bit 16 of the DMA address.
     * Also check for bad buffer address.  These errors mean FS contains a bug.
     */
    if (user_phys == 0)
        panic("FS gave winchester disk driver bad addr",
              static_cast<int>(vir)); // vir is std::size_t, panic takes int
    top_end = static_cast<int>(((user_phys + ct - 1) >> 16) & BYTE);
    if (top_end != top_addr)
        panic("Trying to DMA across 64K boundary", top_addr); // top_addr is int

    /* Now set up the DMA registers. */
    {
        ScopedPortLock guard;          // lock DMA registers during setup
        port_out(DMA_M2, mode);        /* set the DMA mode */
        port_out(DMA_M1, mode);        /* set it again */
        port_out(DMA_ADDR, low_addr);  /* output low-order 8 bits */
        port_out(DMA_ADDR, high_addr); /* output next 8 bits */
        port_out(DMA_TOP, top_addr);   /* output highest 4 bits */
        port_out(DMA_COUNT, low_ct);   /* output low 8 bits of count - 1 */
        port_out(DMA_COUNT, high_ct);  /* output high 8 bits of count - 1 */
    }
}

/*===========================================================================*
 *				w_transfer				     *
 *===========================================================================*/
/* Transfer one block after the drive is positioned. */
static int w_transfer(struct wini &wn) noexcept { // Parameter is struct wini&, noexcept
    /* The drive is now on the proper cylinder. Read or write one block. */

    /* The command is issued by outputing 6 bytes to the controller chip. */
    command[0] = (wn->wn_opcode == DISK_READ ? WIN_READ : WIN_WRITE);
    command[1] = (wn->wn_head | (wn->wn_drive << 5));
    command[2] = (((wn->wn_cylinder & 0x0300) >> 2) | wn->wn_sector);
    command[3] = (wn->wn_cylinder & 0xFF);
    command[4] = BLOCK_SIZE / SECTOR_SIZE;
    command[5] = CTRL_BYTE;
    if (com_out(DMA_INT) != OK)
        return (ERR);

    port_out(DMA_INIT, 3); /* initialize DMA */
    /* Block, waiting for disk interrupt. */
    receive(HARDWARE, &w_mess);

    /* Get controller status and check for errors. */
    if (win_results(*wn) == OK)
        return (OK);
    if ((wn->wn_results[0] & 63) == 24)
        read_ecc();
    else
        w_need_reset = TRUE;
    return (ERR);
}

/*===========================================================================*
 *				win_results				     *
 *===========================================================================*/
static int win_results(struct wini &wn) noexcept { // Parameter is struct wini&, noexcept
    /* Extract results from the controller after an operation. */

    register int i;
    int status;

    port_in(WIN_DATA, &status);
    port_out(WIN_DMA, 0);
    if (!(status & 2))
        return (OK);
    command[0] = WIN_SENSE;
    command[1] = (wn->wn_drive << 5);
    if (com_out(NO_DMA_INT) != OK)
        return (ERR);

    /* Loop, extracting bytes from WIN */
    for (i = 0; i < MAX_RESULTS; i++) {
        if (hd_wait(1) != OK)
            return (ERR);
        port_in(WIN_DATA, &status);
        wn->wn_results[i] = status & BYTE;
    }
    if (wn->wn_results[0] & 63)
        return (ERR);
    else
        return (OK);
}

/*===========================================================================*
 *				win_out					     *
 *===========================================================================*/
/* Output a byte to the winchester disk controller. */
static void win_out(int val) noexcept { // Added noexcept
    /* Output a byte to the controller.  This is not entirely trivial, since you
     * can only write to it when it is listening, and it decides when to listen.
     * If the controller refuses to listen, the WIN chip is given a hard reset.
     */

    if (w_need_reset)
        return; /* if controller is not listening, return */
    if (hd_wait(1) == OK)
        port_out(WIN_DATA, val);
}

/*===========================================================================*
 *				w_reset					     *
 *===========================================================================*/
static int w_reset() noexcept { // Added noexcept
    /* Issue a reset to the controller.  This is done after any catastrophe,
     * like the controller refusing to respond.
     */

    int r = 1, i;

    /* Strobe reset bit low. */
    port_out(WIN_STATUS, r);
    for (i = 0; i < 10000; i++) {
        port_in(WIN_STATUS, &r);
        if ((r & 01) == 0)
            break;
    }
    if (r & 2) {
        printf("Hard disk won't reset\n");
        return (ERR);
    }

    /* Reset succeeded.  Tell WIN drive parameters. */
    w_need_reset = FALSE;

    return (win_init());
}

/*===========================================================================*
 *				win_init				     *
 *===========================================================================*/
static int win_init() noexcept { // Added noexcept
    /* Routine to initialize the drive parameters after boot or reset */

    register int i;

    command[0] = WIN_SPECIFY;      /* Specify some parameters */
    command[1] = 0;                /* Drive 0 */
    if (com_out(NO_DMA_INT) != OK) /* Output command block */
        return (ERR);
    {
        ScopedPortLock guard; // lock controller while specifying drive 0

        /* No. of cylinders (high byte) */
        win_out(param0.nr_cyl >> 8);

        /* No. of cylinders (low byte) */
        win_out(param0.nr_cyl & 0xFF);

        /* No. of heads */
        win_out(param0.nr_heads);

        /* Start reduced write (high byte) */
        win_out(param0.reduced_wr >> 8);

        /* Start reduced write (low byte) */
        win_out(param0.reduced_wr & 0xFF);

        /* Start write precompensation (high byte) */
        win_out(param0.wr_precomp >> 8);

        /* Start write precompensation (low byte) */
        win_out(param0.wr_precomp & 0xFF);

        /* Ecc burst length */
        win_out(param0.max_ecc);
    }

    if (check_init() != OK) { /* See if controller accepted parameters */
        w_need_reset = TRUE;
        return (ERR);
    }

    if (nr_drives > 1) {
        command[1] = (1 << 5);         /* Drive 1 */
        if (com_out(NO_DMA_INT) != OK) /* Output command block */
            return (ERR);
        {
            ScopedPortLock guard; // lock controller while specifying drive 1

            /* No. of cylinders (high byte) */
            win_out(param1.nr_cyl >> 8);

            /* No. of cylinders (low byte) */
            win_out(param1.nr_cyl & 0xFF);

            /* No. of heads */
            win_out(param1.nr_heads);

            /* Start reduced write (high byte) */
            win_out(param1.reduced_wr >> 8);

            /* Start reduced write (low byte) */
            win_out(param1.reduced_wr & 0xFF);

            /* Start write precompensation (high byte) */
            win_out(param1.wr_precomp >> 8);

            /* Start write precompensation (low byte) */
            win_out(param1.wr_precomp & 0xFF);

            /* Ecc burst length */
            win_out(param1.max_ecc);
        }
        if (check_init() != OK) { /* See if controller accepted parameters */
            w_need_reset = TRUE;
            return (ERR);
        }
    }
    for (i = 0; i < nr_drives; i++) {
        command[0] = WIN_RECALIBRATE;
        command[1] = i << 5;
        command[5] = CTRL_BYTE;
        if (com_out(INT) != OK)
            return (ERR);
        receive(HARDWARE, &w_mess);
        if (win_results(wini[i * DEV_PER_DRIVE]) != OK) {
            w_need_reset = TRUE;
            return (ERR);
        }
    }
    return (OK);
}

/*============================================================================*
 *				check_init				      *
 *============================================================================*/
static int check_init() noexcept { // Added noexcept
    /* Routine to check if controller accepted the parameters */
    int r;

    if (hd_wait(2) == OK) {
        port_in(WIN_DATA, &r);
        if (r & 2)
            return (ERR);
        else
            return (OK);
    }
}

/*============================================================================*
 *				read_ecc				      *
 *============================================================================*/
static int read_ecc() noexcept { // Added noexcept
    /* Read the ecc burst-length and let the controller correct the data */

    int r;

    command[0] = WIN_ECC_READ;
    if (com_out(NO_DMA_INT) == OK && hd_wait(1) == OK) {
        port_in(WIN_DATA, &r);
        if (hd_wait(1) == OK) {
            port_in(WIN_DATA, &r);
            if (r & 1)
                w_need_reset = TRUE;
        }
    }
    return (ERR);
}

/*============================================================================*
 *				hd_wait					      *
 *============================================================================*/
static int hd_wait(int bit) noexcept { // Added noexcept
    /* Wait until the controller is ready to receive a command or send status */

    register int i = 0;
    int r;

    do {
        port_in(WIN_STATUS, &r);
        r &= bit;
    } while ((i++ < MAX_WIN_RETRY) && !r);

    if (i >= MAX_WIN_RETRY) {
        w_need_reset = TRUE;
        return (ERR);
    } else
        return (OK);
}

/*============================================================================*
 *				com_out					      *
 *============================================================================*/
static int com_out(int mode) noexcept { // Added noexcept
    /* Output the command block to the winchester controller and return status */

    register int i = 0;
    int r;

    port_out(WIN_SELECT, mode);
    port_out(WIN_DMA, mode);
    for (i = 0; i < MAX_WIN_RETRY; i++) {
        port_in(WIN_STATUS, &r);
        if ((r & 0x0F) == 0x0D)
            break;
    }
    if (i == MAX_WIN_RETRY) {
        w_need_reset = TRUE;
        return (ERR);
    }
    {
        ScopedPortLock guard; // ensure ports locked during writes
        for (i = 0; i < 6; i++)
            port_out(WIN_DATA, command[i]);
    }
    port_in(WIN_STATUS, &r);
    if (r & 1) {
        w_need_reset = TRUE;
        return (ERR);
    } else
        return (OK);
}

/*============================================================================*
 *				init_params				      *
 *============================================================================*/
static void init_params() noexcept { // Added noexcept
    /* This routine is called at startup to initialize the partition table,
     * the number of drives and the controller
     */
    unsigned int i, segment, offset;
    int type_0, type_1;
    uint64_t address; // phys_bytes -> uint64_t
    // extern phys_bytes umap(); // umap returns uint64_t
    extern int vec_table[];

    /* Read the switches from the controller */
    port_in(WIN_SELECT, &i);

    /* Calculate the drive types */
    type_0 = (i >> 2) & 3;
    type_1 = i & 3;

    /* Copy the parameter vector from the saved vector table */
    offset = vec_table[2 * 0x41];
    segment = vec_table[2 * 0x41 + 1];

    /* Calculate the address off the parameters and copy them to buf */
    address = (static_cast<uint64_t>(segment) << 4) + offset;
    // phys_copy takes (uint64_t, uint64_t, uint64_t)
    // umap takes (proc*, int, std::size_t, std::size_t) returns uint64_t
    // buf is unsigned char[].
    phys_copy(address,
              umap(proc_addr(WINCHESTER), D, reinterpret_cast<std::size_t>(buf),
                   static_cast<std::size_t>(64)),
              64ULL);

    /* Copy the parameters to the structures */
    copy_params((&buf[type_0 * 16]), &param0); // Renamed copy_param to copy_params
    copy_param((&buf[type_1 * 16]), &param1);

    /* Get the nummer of drives from the bios */
    phys_copy(0x475ULL,
              umap(proc_addr(WINCHESTER), D, reinterpret_cast<std::size_t>(buf),
                   static_cast<std::size_t>(1)),
              1ULL);
    nr_drives = static_cast<int>(*buf);

    /* Set the parameters in the drive structure */
    for (i = 0; i < 5; i++)
        wini[i].wn_heads = param0.nr_heads;
    wini[0].wn_low = wini[5].wn_low = 0L;
    wini[0].wn_size = (long)((long)param0.nr_cyl * (long)param0.nr_heads * (long)NR_SECTORS);
    for (i = 5; i < 10; i++)
        wini[i].wn_heads = param1.nr_heads;
    wini[5].wn_size = (long)((long)param1.nr_cyl * (long)param1.nr_heads * (long)NR_SECTORS);

    /* Initialize the controller */
    if ((nr_drives > 0) && (win_init() != OK))
        nr_drives = 0;

    /* Read the partition table for each drive and save them */
    for (i = 0; i < nr_drives; i++) {             // i is unsigned int
        device(w_mess) = static_cast<int>(i * 5); // device is int msg field
        position(w_mess) = 0LL;                   // position is int64_t msg field
        count(w_mess) = BLOCK_SIZE;               // count is int msg field, BLOCK_SIZE is int
        address(w_mess) = reinterpret_cast<char *>(buf); // address is char* msg field
        proc_nr(w_mess) = WINCHESTER;                    // proc_nr is int msg field
        w_mess.m_type = DISK_READ;
        if (w_do_rdwt(&w_mess) != BLOCK_SIZE) // w_do_rdwt is noexcept
            panic("Can't read partition table of winchester ",
                  static_cast<int>(i)); // panic is noexcept
        copy_prt(static_cast<int>(i * 5));
    }
}

/*============================================================================*
 *				copy_params				      *
 *============================================================================*/
static void copy_params(unsigned char *src, struct param *dest) noexcept { // Added noexcept
    /* This routine copies the parameters from src to dest
     * and sets the parameters for partition 0 and 5
     */

    // This relies on direct memory interpretation. Using memcpy would be safer.
    // Assuming struct param members are int or compatible.
    dest->nr_cyl = *reinterpret_cast<int *>(src);
    dest->nr_heads = (int)src[2];
    dest->reduced_wr = *(int *)&src[3];
    dest->wr_precomp = *(int *)&src[5];
    dest->max_ecc = (int)src[7];
}

/*============================================================================*
 *				copy_prt				      *
 *============================================================================*/
static void copy_prt(int drive) noexcept { // Added noexcept
    /* This routine copies the partition table for the selected drive to
     * the variables wn_low and wn_size
     */

    register int i, offset;
    struct wini *wn;
    uint64_t temp_val; // For reading long from buf before assigning to uint64_t
    int64_t adjust64;  // Was long, for arithmetic with wn_low/wn_size

    for (i = 0; i < 4; i++) {
        adjust64 = 0;
        wn = &wini[i + drive + 1]; // wini elements have wn_low, wn_size as uint64_t
        offset = PART_TABLE + i * 0x10;
        // Safely read a long (assuming 32-bit from buf) and then assign to uint64_t
        // This assumes buf contains 32-bit little-endian longs for partition table entries.
        // For robustness, memcpy is preferred over reinterpret_cast if alignment is uncertain.
        // temp_val = *reinterpret_cast<long *>(&buf[offset]); // Original logic
        // wn->wn_low = static_cast<uint64_t>(temp_val);
        // A safer approach (though original code did direct cast):
        memcpy(&temp_val, &buf[offset], sizeof(long)); // Assuming long is what's in buf
        wn->wn_low = static_cast<uint64_t>(temp_val);

        if ((wn->wn_low % (BLOCK_SIZE / SECTOR_SIZE)) != 0) {
            adjust64 = static_cast<int64_t>(wn->wn_low);
            wn->wn_low = (wn->wn_low / (BLOCK_SIZE / SECTOR_SIZE) + 1) * (BLOCK_SIZE / SECTOR_SIZE);
            adjust64 = static_cast<int64_t>(wn->wn_low) - adjust64;
        }
        // wn->wn_size = *(long *)&buf[offset + sizeof(long)] - adjust;
        memcpy(&temp_val, &buf[offset + sizeof(long)], sizeof(long));
        wn->wn_size = static_cast<uint64_t>(temp_val) - static_cast<uint64_t>(adjust64);
    }
    sort(&wini[drive + 1]);
}

/* Sort drive order */
static void sort(struct wini *wn) noexcept { // Removed register, added noexcept
    register int i,
        j; // Keep register for loop counters if desired, though modern compilers often ignore

    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++)
            if ((wn[j].wn_low == 0) && (wn[j + 1].wn_low != 0))
                swap(&wn[j], &wn[j + 1]);
            else if (wn[j].wn_low > wn[j + 1].wn_low &&
                     wn[j + 1].wn_low != 0) // uint64_t comparison
                swap(&wn[j], &wn[j + 1]);
}

/* Swap two wini structures */
static void swap(struct wini *first,
                 struct wini *second) noexcept { // Removed register, added noexcept
    register struct wini tmp;

    tmp = *first;
    *first = *second;
    *second = tmp;
}
